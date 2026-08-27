/**
 * @file wifi_manager.hpp
 * @brief WiFi管理模块 - 支持通过陶晶驰屏幕配置WiFi
 * @details 实现功能：
 *          1. 从配置文件读取WiFi信息
 *          2. 连接/断开WiFi
 *          3. 保存WiFi配置到文件
 *          4. 获取当前WiFi状态
 */

#ifndef __WIFI_MANAGER_HPP
#define __WIFI_MANAGER_HPP

#include "include.hpp"

/*============================================================================
 *                              WiFi配置结构
 *============================================================================*/
#define WIFI_CONFIG_FILE "/home/root/wifi_config.txt"  // WiFi配置文件路径
#define WIFI_SSID_MAX_LEN     64  // SSID最大长度
#define WIFI_PASSWORD_MAX_LEN 64  // 密码最大长度

// WiFi配置结构体
struct WiFiConfig {
    char ssid[WIFI_SSID_MAX_LEN];         // WiFi账号
    char password[WIFI_PASSWORD_MAX_LEN]; // WiFi密码
    bool configured;                      // 是否已配置
};

/*============================================================================
 *                              WiFi连接状态枚举
 *============================================================================*/
enum WiFiConnectionState {
    WIFI_STATE_NONE,        // 未初始化
    WIFI_STATE_CONNECTING,  // 正在连接
    WIFI_STATE_CONNECTED,   // 已连接
    WIFI_STATE_FAILED       // 连接失败
};

/*============================================================================
 *                              全局变量声明
 *============================================================================*/
extern WiFiConfig wifi_config;           // 当前WiFi配置
extern char wifi_status_str[32];         // WiFi状态字符串（用于屏幕显示）
extern WiFiConnectionState wifi_state;   // WiFi连接状态
extern bool wifi_connected;              // WiFi连接标志
extern char device_ip_str[32];           // 设备IP地址
extern char wifi_ssid_display[64];       // 当前连接的WiFi名称显示

/*============================================================================
 *                              定时状态检查
 *============================================================================*/
/**
 * @brief   WiFi状态定时检查（在主循环中调用，每3秒检查一次）
 * @details 更新连接状态和IP地址显示
 */
void WiFi_Periodic_Check(void);

/*============================================================================
 *                              函数声明
 *============================================================================*/

/**
 * @brief   初始化WiFi管理模块
 * @details 读取配置文件，如果存在则使用配置连接WiFi
 */
void WiFi_Manager_Init(void);

/**
 * @brief   设置WiFi账号密码（从屏幕接收）
 * @param   ssid     WiFi账号
 * @param   password WiFi密码
 */
void WiFi_Set_Credentials(const char* ssid, const char* password);

/**
 * @brief   连接WiFi
 * @details 使用当前配置的账号密码连接WiFi
 * @return  true-连接成功或正在连接, false-失败
 */
bool WiFi_Connect(void);

/**
 * @brief   断开WiFi连接
 */
void WiFi_Disconnect(void);

/**
 * @brief   保存WiFi配置到文件
 * @details 保存到 /home/root/wifi_config.txt
 * @return  true-保存成功, false-失败
 */
bool WiFi_Save_Config(void);

/**
 * @brief   从文件读取WiFi配置
 * @return  true-读取成功, false-失败或文件不存在
 */
bool WiFi_Load_Config(void);

/**
 * @brief   获取当前WiFi状态
 * @details 更新wifi_status_str变量
 */
void WiFi_Update_Status(void);

/**
 * @brief   更新设备IP地址显示
 * @details 从wlan0接口获取IP地址，更新device_ip_str和wifi_connected状态
 * @return  true-获取到IP, false-未获取到IP
 */
bool WiFi_Update_Device_IP(void);

/**
 * @brief   检查WiFi是否已连接
 * @return  true-已连接, false-未连接
 */
bool WiFi_Is_Connected(void);

#endif /* __WIFI_MANAGER_HPP */
