/**
 * @file screen_probe.cpp
 * @brief 编码测试：在DConfirm页同时发UTF-8和GBK两种编码的中文，判断屏幕字符集
 */

#include "include.hpp"

#define PROBE_UART_PIN     UART5_PIN64
#define PROBE_UART_BAUD    230400

static ls_uart* probe_uart = nullptr;
static uint8_t rbuf[64];
static volatile size_t rlen = 0;

static void rx_cb(uint8_t data) { if (rlen < sizeof(rbuf)) rbuf[rlen++] = data; }

static void send_line(const char* s, size_t n)
{
    probe_uart->uart_write((const uint8_t*)s, n);
    const char FF[3] = {(char)0xFF, (char)0xFF, (char)0xFF};
    probe_uart->uart_write((const uint8_t*)FF, 3);
}

static void heartbeat()
{
    send_line("n_online.val=1", 14);
    send_line("page_sys.v_hb_timeout.val=0", 27);
}

int main()
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("===== 中文编码测试 =====\n");

    // CPU满负载模拟：main运行时的OpenCV/AprilTag等重负载
    for (int i = 0; i < 3; i++) {
        if (fork() == 0) { while (true) { } }
    }
    printf("LOAD_START\n");
    probe_uart = new ls_uart(PROBE_UART_PIN, PROBE_UART_BAUD,
                             LS_UART_DATA8, LS_UART_STOP1, LS_UART_PARITY_NONE,
                             UART_MODE_THREAD, rx_cb);
    usleep(300 * 1000);
    heartbeat();

    // 切到DConfirm页
    send_line("page DConfirm", 13);
    usleep(300 * 1000);
    heartbeat();

    // 第一行: UTF-8编码（UTF-8字节）
    const char* t1 = "d_s_1.txt=\"UTF8: \xE5\xB7\xB2\xE5\x88\xB0\xE5\x8F\x96\xE4\xBB\xB6\xE7\x82\xB9\"";
    // 第二行: GBK编码（GBK字节）
    const char* t2 = "d_s_2.txt=\"GBK: \xD2\xD1\xB5\xBD\xC8\xA1\xBC\xFE\xB5\xE3\"";
    // 第三行: 纯ASCII对照
    const char* t3 = "d_s_3.txt=\"ASCII line 123 OK\"";
    // 第四行: UTF-8编码
    const char* t4 = "d_s_4.txt=\"U: \xE7\x89\xA9\xE5\x93\x81\xE5\xB7\xB2\xE8\xA3\x85\xE5\xA5\xBD\"";

    send_line(t1, strlen(t1));
    usleep(50 * 1000);
    send_line(t2, strlen(t2));
    usleep(50 * 1000);
    send_line(t3, strlen(t3));
    usleep(50 * 1000);
    send_line(t4, strlen(t4));

    usleep(500 * 1000);
    heartbeat();
    for (int round = 0; round < 20; round++) {
        usleep(500 * 1000);
        heartbeat();
        send_line(t1, strlen(t1));
        usleep(5 * 1000);
        send_line(t2, strlen(t2));
        usleep(5 * 1000);
        send_line(t4, strlen(t4));
    }

    printf("已发送测试文本，请看屏幕：\n");
    printf("  d_s_1 = UTF-8编码的'已到取件点'\n");
    printf("  d_s_2 = GBK编码的'已到取件点'\n");
    printf("  d_s_3 = 纯英文对照\n");
    printf("  d_s_4 = UTF-8编码的'物品已装好'\n");
    printf("哪行中文正常显示=>屏幕字符集就是那种编码\n");
    return 0;
}
