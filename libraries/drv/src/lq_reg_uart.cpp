#include "lq_reg_uart.hpp"
#include "lq_map_addr.hpp"
#include "lq_assert.hpp"

/********************************************************************************
 * @brief   硬件配置 UART 的无参构造函数.
 * @param   _pin    : UART 引脚配置, 参考 uart_pin_t 枚举.
 * @param   _baud   : 波特率.
 * @param   _stop   : 停止位, 参考 ls_uart_stop_bits_t 枚举.
 * @param   _data   : 数据位, 参考 ls_uart_data_bits_t 枚举.
 * @param   _parity : 校验位, 参考 ls_uart_parity_t 枚举.
 * @return  none.
 * @note    none.
 ********************************************************************************/
ls_uart::ls_uart(uart_pin_t _pin, uint32_t _baud, ls_uart_data_bits_t _data, ls_uart_stop_bits_t _stop, ls_uart_parity_t _parity) :
    uart_reg(nullptr), rx_pin(PIN_INVALID), tx_pin(PIN_INVALID), port(UART_PORT_INVALID), baudrate(_baud), data_bits(_data), stop_bits(_stop), parity(_parity)
{
    uint32_t tickstart = lq_get_tick_ms();
    if (_baud == 0) {
        lq_log_error("串口 %d 波特率不能为0", ((_pin>>10)&0x0F));
        return;
    }
    // 保存参数
    this->rx_pin    = (gpio_pin_t)(_pin & 0xFF);
    this->tx_pin    = (gpio_pin_t)(this->rx_pin + 1);
    this->port      = (uart_port_t)((_pin>>10)&0x0F);
    this->mux       = (gpio_mux_mode_t)((_pin>>8)&0x03);
    // 配置 GPIO 复用
    gpio_mux_set(this->rx_pin, this->mux);
    gpio_mux_set(this->tx_pin, this->mux);
    // 获取 UART 控制器基地址
    this->uart_reg = (ls_uart_reg_t *)(LQ::ls_addr_mmap(LS_UART_BASE_ADDR + (this->port * LS_UART_BASE_OFS)));
    if (this->uart_reg == nullptr) {
        lq_log_error("串口 %d 寄存器映射失败", this->port);
        return;
    }
    // 等待空闲
    while (!(this->uart_reg->LSR & LS_UART_LSR_TE)) {
        if (lq_get_tick_ms() - tickstart > 1000) {
            lq_log_error("串口 %d 等待空闲超时", this->port);
            return;
        }
    }
    // 设置访问操作分频锁存器
    this->uart_reg->LCR = LS_UART_LCR_DLAB;
    // 设置波特率
    uint64_t uart_div = (LS_UART_CLK_FRE << 8) / (16 * _baud);
    this->uart_reg->DL_H = (uart_div >> 16) & 0xFF;
    this->uart_reg->DL_L = (uart_div >>  8) & 0xFF;
    this->uart_reg->DL_D = uart_div & 0xFF;
    // 设置访问操作正常寄存器, 并设置数据位, 停止位, 校验位
    this->uart_reg->LCR = _data|_stop|_parity;
    // 关闭所有中断
    this->uart_reg->IER = 0;
    // 清空、复位接收和发送 FIFO 缓冲区
    this->uart_reg->FCR = LS_UART_FCR_RXSET | LS_UART_FCR_TXSET;
    // 状态清空
    (void)this->uart_reg->LSR;
    (void)this->uart_reg->IIR;
}

/********************************************************************************
 * @brief   UART 发送数据.
 * @param   _buf     : 发送数据缓冲区指针.
 * @param   _len     : 发送数据长度.
 * @param   _timeout : 超时时间, 单位: 毫秒.
 * @return  发送数据长度.
 * @note    发送数据时, 会阻塞等待, 直到发送完成或超时.
 ********************************************************************************/
ssize_t ls_uart::uart_write(const uint8_t *_buf, ssize_t _len, uint32_t _timeout)
{
    uint32_t tickstart = lq_get_tick_ms();
    // 检查发送过程中是否有错误
    if (_buf == nullptr || _len <= 0) {
        return -1;
    }
    // 检查 UART 是否初始化
    if (this->uart_reg == nullptr) {
        return -2;
    }
    std::lock_guard<std::mutex> lock(this->tx_mtx);
    ssize_t size = 0;
    // 初始化超时管理机制
    while (size < _len) {
        // 等待 LS_UART_LSR_TFE 标志位
        while (!(this->uart_reg->LSR & LS_UART_LSR_TFE)) {
            if (lq_get_tick_ms() - tickstart > _timeout) {
                // lq_log_error("串口 %d 发送超时", this->port);
                return size;
            }
        }
        this->uart_reg->DAT = *_buf++;
        size++;
    }
    while (!(this->uart_reg->LSR & LS_UART_LSR_TFE)) {
        if (lq_get_tick_ms() - tickstart > _timeout) {
            // lq_log_error("串口 %d 等待最后一字节完成超时", this->port);
            break;
        }
    }
    return size;
}

/********************************************************************************
 * @brief   UART 接收数据.
 * @param   _buf     : 接收数据缓冲区指针.
 * @param   _len     : 接收数据长度.
 * @param   _timeout : 超时时间, 单位: 毫秒.
 * @return  接收数据长度.
 * @note    接收数据时, 会阻塞等待, 直到接收完成或超时.
 ********************************************************************************/
ssize_t ls_uart::uart_read(uint8_t *_buf, ssize_t _len, uint32_t _timeout)
{
    uint32_t tickstart = lq_get_tick_ms();
    // 检查接收过程中是否有错误
    if (_buf == nullptr || _len <= 0) {
        return -1;
    }
    // 检查 UART 是否初始化
    if (this->uart_reg == nullptr) {
        return -2;
    }
    std::lock_guard<std::mutex> lock(this->rx_mtx);
    ssize_t size = 0;
    // 初始化超时管理机制
    while (size < _len) {
        // 等待 FIFO 中没有数据
        while (!(this->uart_reg->LSR & LS_UART_LSR_DR)) {
            if (lq_get_tick_ms() - tickstart > _timeout) {
                // lq_log_error("串口 %d 接收超时", this->port);
                return size;
            }
        }
        *_buf++ = this->uart_reg->DAT;
        size++;
    }
    return size;
}

/********************************************************************************
 * @brief   UART 获取串口端口号.
 * @param   none.
 * @return  串口端口号.
 ********************************************************************************/
uart_port_t ls_uart::get_uart_port() const
{
    return this->port;
}

/********************************************************************************
 * @brief   UART 获取数据位数.
 * @param   none.
 * @return  数据位数.
 ********************************************************************************/
ls_uart_data_bits_t ls_uart::get_data_bits() const
{
    return this->data_bits;
}

/********************************************************************************
 * @brief   UART 获取停止位数.
 * @param   none.
 * @return  停止位数.
 ********************************************************************************/
ls_uart_stop_bits_t ls_uart::get_stop_bits() const
{
    return this->stop_bits;
}

/********************************************************************************
 * @brief   UART 获取校验位.
 * @param   none.
 * @return  校验位.
 ********************************************************************************/
ls_uart_parity_t ls_uart::get_parity() const
{
    return this->parity;
}

/********************************************************************************
 * @brief   UART 获取波特率.
 * @param   none.
 * @return  波特率.
 ********************************************************************************/
uint32_t ls_uart::get_baudrate() const
{
    return this->baudrate;
}

/********************************************************************************
 * @brief   UART 清理资源.
 * @param   none.
 * @return  none.
 * @note    调用该函数后, 该串口实例将无法再使用.
 ********************************************************************************/
void ls_uart::cleanup()
{
    if (this->uart_reg != nullptr) {
        LQ::ls_addr_munmap((ls_reg32_addr_t)this->uart_reg);
        this->uart_reg  = nullptr;
        this->rx_pin    = PIN_INVALID;
        this->tx_pin    = PIN_INVALID;
        this->port      = UART_PORT_INVALID;
        this->mux       = GPIO_MUX_INVALID;
        this->data_bits = LS_UART_DATA_INVALID;
        this->stop_bits = LS_UART_STOP_INVALID;
        this->parity    = LS_UART_PARITY_INVALID;
        this->baudrate  = 0;
    }
}

/********************************************************************************
 * @brief   UART 析构函数.
 * @param   none.
 * @return  none.
 * @note    变量生命周期结束时, 自动调用析构函数, 释放 UARTx 控制器基地址映射.
 ********************************************************************************/
ls_uart::~ls_uart()
{
    this->cleanup();
}
