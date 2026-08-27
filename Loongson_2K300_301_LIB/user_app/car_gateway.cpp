/**
 * @file car_gateway.cpp
 * @brief 中央配送——TCP网关 实现
 * @details 网络线程唯一职责：连接管理/收发/解析入队。
 *          业务决策（认证处理、命令执行、ACK生成）全部在主线程。
 */

#include "car_gateway.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

CarGateway::~CarGateway()
{
    Stop();
}

bool CarGateway::Start()
{
    if (running_.load()) return true;
    if (cfg_.host.empty()) return false;
    running_.store(true);
    try {
        thread_ = std::thread(&CarGateway::Thread_Fn, this);
    } catch (...) {
        running_.store(false);
        return false;
    }
    return true;
}

void CarGateway::Stop()
{
    if (!running_.load()) return;
    running_.store(false);
    // 唤醒可能阻塞在poll/recv/connect上的线程
    if (fd_ >= 0) {
        shutdown(fd_, SHUT_RDWR);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

//================================================================================
// 主线程侧接口
//================================================================================
bool CarGateway::Send_Line(const std::string& line)
{
    std::lock_guard<std::mutex> lk(send_mtx_);
    if (send_q_.size() >= cfg_.send_queue_limit) return false;
    send_q_.push_back(line);
    return true;
}

bool CarGateway::Poll_Server_Message(ServerMessage* out)
{
    std::lock_guard<std::mutex> lk(recv_mtx_);
    if (recv_q_.empty()) return false;
    *out = recv_q_.front();
    recv_q_.pop_front();
    return true;
}

void CarGateway::Notify_Auth_Result(bool ok)
{
    if (!ok) auth_reject_.store(true);   // 线程内看到后主动断开
}

bool CarGateway::Is_Link_Up() const
{
    return link_up_.load();
}

uint32_t CarGateway::Reconnect_Count() const
{
    return reconnect_count_.load();
}

void CarGateway::Set_Link_Loss_Callback(std::function<void()> cb)
{
    link_loss_cb_ = cb;
}

//================================================================================
// 网络线程
//================================================================================
uint64_t CarGateway::Now_Ms_Monotonic()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

bool CarGateway::Connect_With_Timeout()
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;
    int rc = getaddrinfo(cfg_.host.c_str(), NULL, &hints, &res);
    if (rc != 0 || res == NULL) return false;

    int fd = socket(res->ai_family, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return false; }

    // 非阻塞connect + poll超时
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    if (res->ai_family == AF_INET)
        ((struct sockaddr_in*)res->ai_addr)->sin_port = htons(cfg_.port);
    else
        ((struct sockaddr_in6*)res->ai_addr)->sin6_port = htons(cfg_.port);

    rc = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc != 0 && errno != EINPROGRESS) { close(fd); return false; }
    if (rc != 0) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        rc = poll(&pfd, 1, (int)cfg_.connect_timeout_ms);
        if (rc <= 0) { close(fd); return false; }
        int soerr = 0;
        socklen_t slen = sizeof(soerr);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) != 0 || soerr != 0) {
            close(fd);
            return false;
        }
    }

    // 恢复阻塞模式，后续统一用poll
    fcntl(fd, F_SETFL, flags);

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    fd_ = fd;
    return true;
}

bool CarGateway::Send_All(const char* data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLOUT;
        int rc = poll(&pfd, 1, (int)cfg_.connect_timeout_ms);
        if (rc <= 0) return false;
        ssize_t n = send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

void CarGateway::Flush_Send_Queue()
{
    // 每次最多发送若干条，避免长时间占用线程；队列竞争窗口极短
    for (int burst = 0; burst < 16; burst++) {
        std::string line;
        {
            std::lock_guard<std::mutex> lk(send_mtx_);
            if (send_q_.empty()) return;
            line = send_q_.front();
            send_q_.pop_front();
        }
        line.push_back('\n');
        if (!Send_All(line.data(), line.size())) {
            Handle_Disconnect(true);
            return;
        }
    }
}

void CarGateway::Handle_Disconnect(bool notify_loss)
{
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    // 健康会话（存活≥5s）断线后退避从1s重新开始；非健康断线沿用当前序列继续增长
    if (Now_Ms_Monotonic() - connected_at_ms_ >= 5000) backoff_ms_ = 1000;
    need_backoff_ = true;
    link_up_.store(false);
    auth_reject_.store(false);
    decoder_.Reset();
    reconnect_count_.fetch_add(1);
    if (notify_loss && link_loss_cb_) link_loss_cb_();
}

void CarGateway::Thread_Fn()
{
    const uint32_t kMaxBackoffMs = 30000;
    uint64_t last_rx_ms = 0;                    // 最近一次收到服务器数据的时刻

    while (running_.load()) {
        if (fd_ < 0) {
            // ---- 断线退避：先睡当前间隔，睡完翻倍（1,2,4,8,16,30,max）----
            if (need_backoff_) {
                need_backoff_ = false;
                uint32_t waited = 0;
                while (running_.load() && waited < backoff_ms_) {
                    struct timespec ts = {0, 50 * 1000 * 1000};
                    nanosleep(&ts, NULL);
                    waited += 50;
                }
                if (!running_.load()) break;
                if (backoff_ms_ < kMaxBackoffMs) backoff_ms_ *= 2;
                if (backoff_ms_ > kMaxBackoffMs) backoff_ms_ = kMaxBackoffMs;
            }

            // ---- 尝试建连 ----
            if (Connect_With_Timeout()) {
                link_up_.store(true);
                connected_at_ms_ = Now_Ms_Monotonic();
                last_rx_ms = connected_at_ms_;
                decoder_.Reset();
                printf("[GW] 已连接 %s:%u\n", cfg_.host.c_str(), (unsigned)cfg_.port);
            } else {
                need_backoff_ = true;   // 连接失败也要退避后再试
            }
            continue;
        }

        // ---- 已连接：poll收发 ----
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;
        int rc = poll(&pfd, 1, (int)cfg_.poll_tick_ms);

        if (!running_.load()) break;            // Stop()触发的shutdown会唤醒poll

        if (rc > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
            char buf[4096];
            ssize_t n = recv(fd_, buf, sizeof(buf), 0);
            if (n == 0) {
                // 服务器正常关闭
                printf("[GW] 服务器关闭连接\n");
                Handle_Disconnect(true);
                continue;
            }
            if (n < 0 && errno != EINTR && errno != EAGAIN) {
                printf("[GW] recv错误: %s\n", strerror(errno));
                Handle_Disconnect(true);
                continue;
            }
            if (n > 0) {
                last_rx_ms = Now_Ms_Monotonic();
                std::vector<std::string> lines;
                if (!decoder_.Feed(buf, (size_t)n, &lines)) {
                    printf("[GW] 单行超过64KiB，断开重连\n");
                    Handle_Disconnect(true);   // 协议异常按掉线处理
                    continue;
                }
                for (size_t i = 0; i < lines.size(); i++) {
                    ServerMessage msg;
                    ParseResult pr = Delivery_Parse_Server_Message(lines[i], &msg);
                    if (!pr.ok) {
                        // 语法/公共头非法：记录不中断（业务fault上报由主线程阶段5接入）
                        printf("[GW] 非法消息(%s): %s\n",
                               Delivery_Error_Name(pr.code), pr.message.c_str());
                        continue;
                    }
                    std::lock_guard<std::mutex> lk(recv_mtx_);
                    if (recv_q_.size() >= cfg_.recv_queue_limit) {
                        recv_q_.pop_front();   // 丢最旧保最新（主线程消化不及）
                        recv_dropped_++;
                    }
                    recv_q_.push_back(msg);
                }
            }
        }

        // 认证失败：主动断开（服务器拒绝后由主线程决定是否重试）
        if (auth_reject_.load()) {
            printf("[GW] 认证失败，主动断开\n");
            Handle_Disconnect(false);          // 服务器已知，不触发link loss回调
            continue;
        }

        // 掉线判定：单调时钟，超时未收到任何服务器数据
        if (Now_Ms_Monotonic() - last_rx_ms > cfg_.connection_timeout_ms) {
            printf("[GW] %ums未收到服务器消息，判掉线\n",
                   (unsigned)cfg_.connection_timeout_ms);
            Handle_Disconnect(true);
            continue;
        }

        // 发送队列冲刷
        if (link_up_.load()) Flush_Send_Queue();
    }

    // 线程退出清理（不触发回调——主动退出不算掉线）
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    link_up_.store(false);
}
