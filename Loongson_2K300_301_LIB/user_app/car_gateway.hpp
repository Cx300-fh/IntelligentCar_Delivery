/**
 * @file car_gateway.hpp
 * @brief 中央配送——TCP网关（专用DeliveryTcpClient，不改公共lq_tcp_client）
 * @details 职责（Kevin 222.md 第八节）：
 *          建连、NDJSON拆包、JSON语法解析、命令入队、发送队列、心跳发送、重连退避；
 *          只能强制禁止运动（经回调），不能允许启动、不直接ACK、不修改导航。
 *          独立可测模块：不include工程include.hpp，仅依赖delivery_protocol。
 */

#ifndef __CAR_GATEWAY_HPP
#define __CAR_GATEWAY_HPP

#include "delivery_protocol.hpp"

#include <string>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

class CarGateway {
public:
    struct Config {
        std::string host            = "127.0.0.1";
        uint16_t    port            = 8898;
        uint32_t    connect_timeout_ms     = 3000;  // 单次connect超时
        uint32_t    connection_timeout_ms  = 6000;  // 无任何服务器消息判掉线（单调时钟）
        uint32_t    poll_tick_ms            = 100;  // 线程poll粒度
        size_t      send_queue_limit        = 64;   // 有界发送队列
        size_t      recv_queue_limit        = 64;   // 有界接收队列
    };

    explicit CarGateway(const Config& cfg) : cfg_(cfg) {}
    ~CarGateway();

    CarGateway(const CarGateway&) = delete;
    CarGateway& operator=(const CarGateway&) = delete;

    // 启动网络线程；Stop()安全退出（shutdown唤醒阻塞 + join）
    bool Start();
    void Stop();

    // ================= 主线程侧接口 =================
    // JSON行入有界发送队列（不含\n）；队列满返回false（心跳可丢，ACK/事件不可丢需上报）
    bool Send_Line(const std::string& line);
    // 取一条已解析服务器消息（解析在网络线程完成）；无消息返回false
    bool Poll_Server_Message(ServerMessage* out);
    // 主线程处理hello_ack后回告认证结果；false时网关主动断开重连
    void Notify_Auth_Result(bool ok);
    // TCP连接建立且未超时（认证状态由主线程根据hello_ack判定）
    bool Is_Link_Up() const;
    // 累计断线重连次数（诊断用）
    uint32_t Reconnect_Count() const;
    // 掉线回调（网络线程调用）：main中绑定 Safety_Inhibit_Set(INHIBIT_REASON_LINK_LOSS)
    void Set_Link_Loss_Callback(std::function<void()> cb);

private:
    void Thread_Fn();
    bool Connect_With_Timeout();
    bool Send_All(const char* data, size_t len);
    void Flush_Send_Queue();
    void Handle_Disconnect(bool notify_loss);
    static uint64_t Now_Ms_Monotonic();

    Config cfg_;
    std::atomic<bool>     running_{false};
    std::atomic<bool>     link_up_{false};
    std::atomic<bool>     auth_reject_{false};   // 主线程告知认证失败，线程内断开
    std::atomic<uint32_t> reconnect_count_{0};
    std::thread           thread_;
    int                   fd_ = -1;              // 仅网络线程访问

    std::mutex                send_mtx_;
    std::deque<std::string>   send_q_;
    std::mutex                recv_mtx_;
    std::deque<ServerMessage> recv_q_;
    size_t                    recv_dropped_ = 0; // 接收队列满丢弃计数

    NdjsonDecoder              decoder_;          // 仅网络线程访问
    std::function<void()>      link_loss_cb_;     // Start前设置

    // 重连退避状态（仅网络线程访问）：断线后先睡当前值再重连，睡完翻倍，上限30s；
    // 存活≥5s的健康会话断线后退避重置为1s
    uint32_t    backoff_ms_       = 1000;
    bool        need_backoff_     = false;
    uint64_t    connected_at_ms_  = 0;
};

#endif /* __CAR_GATEWAY_HPP */
