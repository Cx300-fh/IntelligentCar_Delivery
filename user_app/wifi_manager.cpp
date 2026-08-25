/**
 * @file wifi_manager.cpp
 * @brief WiFi管理模块实现
 * @details 本模块实现龙芯2K300/301开发板的WiFi管理功能，包括：
 *          - 开机自动连接WiFi（从配置文件加载）
 *          - 通过陶晶驰屏幕配置WiFi（支持保存新WiFi、重连、断开）
 *          - 定时检测WiFi连接状态（每3秒检查一次）
 *          - WiFi配置文件持久化存储（/home/root/wifi_config.txt）
 * 
 * @note 使用AIC8800 WiFi芯片，通过wpa_supplicant连接WiFi
 * @note WiFi状态通过全局变量与屏幕显示模块交互
 */

#include "wifi_manager.hpp"

/*============================================================================
 *                              全局变量定义
 *============================================================================*/

/**
 * @var wifi_config
 * @brief 当前WiFi配置信息
 * @details 存储当前使用的WiFi账号密码，由WiFi_Set_Credentials()设置
 *          或由WiFi_Load_Config()从文件加载
 */
WiFiConfig wifi_config = {{0}, {0}, false};

/**
 * @var wifi_status_str
 * @brief WiFi状态字符串（用于调试打印）
 * @details 显示当前WiFi连接状态，如"Connected:LongQiu_5G"、"Connecting..."、"Failed"等
 */
char wifi_status_str[32] = "Not Configured";

// WiFi连接状态（供屏幕显示使用）
WiFiConnectionState wifi_state = WIFI_STATE_NONE;
bool wifi_connected = false;
char device_ip_str[32] = "No IP";
char wifi_ssid_display[64] = "No Network";

/*============================================================================
 *                              内部辅助函数
 *============================================================================*/

/**
 * @brief   执行shell命令并获取输出
 * @param   cmd        要执行的命令字符串
 * @param   output     输出缓冲区，用于存储命令输出
 * @param   output_len 缓冲区长度
 * @return  true-执行成功, false-执行失败
 * @note    使用popen执行命令，适用于获取命令输出结果
 */
static bool exec_cmd(const char* cmd, char* output, int output_len)
{
    FILE* fp = popen(cmd, "r");
    if (fp == NULL) {
        return false;
    }
    
    if (output && output_len > 0) {
        if (fgets(output, output_len, fp) != NULL) {
            // 去掉末尾的换行符
            output[strcspn(output, "\n")] = 0;
        }
    }
    
    pclose(fp);
    return true;
}

/**
 * @brief   停止WiFi相关进程
 * @details 在切换WiFi或断开连接前调用，停止wpa_supplicant和udhcpc进程
 * @note    使用pkill -9强制终止进程，确保旧连接完全断开
 */
static void wifi_stop_processes(void)
{
    // 停止wpa_supplicant（WiFi认证客户端）
    system("pkill -9 wpa_supplicant 2>/dev/null");
    // 停止udhcpc（DHCP客户端）
    system("pkill -9 udhcpc 2>/dev/null");
    // 等待1秒确保进程完全退出
    system("sleep 1");
}

/**
 * @brief   加载WiFi驱动
 * @details 检查并加载AIC8800 WiFi芯片驱动
 *          驱动文件路径：/usr/lib/modules/4.19.190+/aic8800_*.ko
 * @note    如果驱动已加载则跳过，避免重复加载
 */
static void wifi_load_drivers(void)
{
    // 检查驱动是否已加载
    FILE* fp = popen("lsmod | grep aic8800_bsp", "r");
    if (fp) {
        char buf[64];
        // 如果没有输出，说明驱动未加载
        if (fgets(buf, sizeof(buf), fp) == NULL) {
            // 加载BSP驱动（基础驱动）
            system("insmod /usr/lib/modules/4.19.190+/aic8800_bsp.ko");
            // 加载功能驱动
            system("insmod /usr/lib/modules/4.19.190+/aic8800_fdrv.ko");
            // 等待驱动初始化完成
            system("sleep 1");
        }
        pclose(fp);
    }
}

/*============================================================================
 *                              接口函数实现
 *============================================================================*/

/**
 * @brief   初始化WiFi管理模块
 * @details 开机自启动时调用，完成以下工作：
 *          1. 检查是否已连接WiFi（有IP则跳过，避免SSH断开）
 *          2. 尝试从配置文件加载WiFi信息
 *          3. 配置文件不存在：显示"No Config"，需要通过屏幕设置WiFi
 *          4. 配置文件存在：尝试连接WiFi
 *          5. 连接成功：更新IP显示，连接失败：显示"No Network"
 *
 * @note    此函数在All_Init()中调用，是WiFi功能的入口点
 * @see     All_Init() in init.cpp
 */
void WiFi_Manager_Init(void)
{
    printf("[WiFi] 初始化WiFi管理模块...\n");

    // 步骤1：检查是否已连接WiFi（避免重复连接导致SSH断开）
    if (WiFi_Is_Connected()) {
        printf("[WiFi] WiFi已连接，跳过初始化\n");
        WiFi_Update_Device_IP();
        // 先尝试加载配置获取SSID，如果加载失败则显示"Connected"
        if (WiFi_Load_Config()) {
            strncpy(wifi_ssid_display, wifi_config.ssid, sizeof(wifi_ssid_display) - 1);
        } else {
            strncpy(wifi_ssid_display, "Connected", sizeof(wifi_ssid_display) - 1);
        }
        wifi_state = WIFI_STATE_CONNECTED;
        printf("[WiFi] 当前IP: %s\n", device_ip_str);
        return;
    }

    // 步骤2：未连接，加载配置并连接
    if (WiFi_Load_Config()) {
        printf("[WiFi] 已从配置文件加载WiFi配置: %s\n", wifi_config.ssid);
        // 更新显示的SSID为配置中的名称
        strncpy(wifi_ssid_display, wifi_config.ssid, sizeof(wifi_ssid_display) - 1);
    } else {
        // 配置文件不存在，需要通过屏幕设置WiFi
        printf("[WiFi] 未找到配置文件，请通过屏幕设置WiFi\n");
        snprintf(wifi_status_str, sizeof(wifi_status_str), "No Config");
        strncpy(device_ip_str, "No IP", sizeof(device_ip_str) - 1);
        wifi_connected = false;
        wifi_state = WIFI_STATE_NONE;
        return;
    }

    // 步骤3：尝试连接WiFi
    if (WiFi_Connect()) {
        // 连接成功，获取并显示IP地址
        WiFi_Update_Device_IP();
        printf("[WiFi] 初始化完成，已连接: %s, IP: %s\n", wifi_config.ssid, device_ip_str);
    } else {
        // 连接失败，显示无网络状态
        snprintf(wifi_status_str, sizeof(wifi_status_str), "No Network");
        strncpy(device_ip_str, "No IP", sizeof(device_ip_str) - 1);
        wifi_connected = false;
        printf("[WiFi] 初始化完成，无网络连接\n");
    }
}

/**
 * @brief   设置WiFi账号密码
 * @param   ssid     WiFi账号（网络名称）
 * @param   password WiFi密码
 * @details 仅设置配置，不立即连接。实际连接需要调用WiFi_Connect()
 *          或WiFi_Save_Config()保存后下次开机自动连接
 * 
 * @note    用于屏幕输入新WiFi后设置，配合WiFi_Save_Connect使用
 * @see     Screen_Rx_Process() in screen.cpp
 */
void WiFi_Set_Credentials(const char* ssid, const char* password)
{
    if (ssid == NULL || password == NULL) {
        printf("[WiFi] 错误: SSID或密码为空\n");
        return;
    }
    
    // 复制SSID和密码到配置结构
    strncpy(wifi_config.ssid, ssid, sizeof(wifi_config.ssid) - 1);
    strncpy(wifi_config.password, password, sizeof(wifi_config.password) - 1);
    wifi_config.configured = true;
    
    printf("[WiFi] 已设置WiFi: SSID=%s\n", wifi_config.ssid);
    snprintf(wifi_status_str, sizeof(wifi_status_str), "Configured:%s", wifi_config.ssid);
}

/**
 * @brief   WiFi状态定时检查
 * @details 在主循环中调用，建议每3秒检查一次连接状态
 *          - 如果检测到IP变化，更新状态
 *          - 如果连接断开，更新为失败状态
 * 
 * @note    此函数在main()主循环中调用，实现状态监控
 * @see     main() in main.cpp
 */
void WiFi_Periodic_Check(void)
{
    // 使用静态变量记录上次检查时间
    static auto last_check = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_check).count();
    
    // 每3秒检查一次，避免频繁检查占用CPU
    if (elapsed < 3) {
        return;
    }
    last_check = now;
    
    // 检查当前连接状态
    bool has_ip = WiFi_Update_Device_IP();
    
    if (has_ip) {
        // 有IP地址，确认连接状态为已连接
        if (wifi_state != WIFI_STATE_CONNECTED) {
            wifi_state = WIFI_STATE_CONNECTED;
            printf("[WiFi] 状态更新: 已连接, IP=%s\n", device_ip_str);
        }
    } else {
        // 无IP地址，说明连接已断开
        if (wifi_state == WIFI_STATE_CONNECTED) {
            wifi_state = WIFI_STATE_FAILED;
            strncpy(wifi_ssid_display, "No Network", sizeof(wifi_ssid_display) - 1);
            strncpy(device_ip_str, "No IP", sizeof(device_ip_str) - 1);
            printf("[WiFi] 状态更新: 连接断开\n");
        }
    }
}

/**
 * @brief   更新设备IP地址显示
 * @details 从wlan0网络接口获取当前IP地址
 *          - 成功：更新device_ip_str和wifi_connected标志
 *          - 失败：设置为"No IP"，wifi_connected = false
 * 
 * @return  true-成功获取到IP地址, false-未获取到IP
 * @note    使用ifconfig命令获取IP，兼容大多数Linux系统
 */
bool WiFi_Update_Device_IP(void)
{
    // 执行ifconfig命令获取wlan0的IP地址
    FILE *fp = popen("ifconfig wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d: -f2", "r");
    if (fp) {
        char ip_buf[32] = {0};
        if (fgets(ip_buf, sizeof(ip_buf), fp) != NULL) {
            // 去掉换行符
            ip_buf[strcspn(ip_buf, "\n")] = 0;
            if (strlen(ip_buf) > 0) {
                // 成功获取IP
                strncpy(device_ip_str, ip_buf, sizeof(device_ip_str) - 1);
                wifi_connected = true;
                pclose(fp);
                return true;
            }
        }
        pclose(fp);
    }
    
    // 获取失败，设置默认值
    strncpy(device_ip_str, "No IP", sizeof(device_ip_str) - 1);
    wifi_connected = false;
    return false;
}

/**
 * @brief   连接WiFi
 * @details 使用当前配置的WiFi信息（wifi_config）进行连接：
 *          1. 加载WiFi驱动
 *          2. 停止旧的WiFi进程
 *          3. 生成wpa_supplicant配置文件
 *          4. 启动wpa_supplicant进行认证
 *          5. 启动udhcpc获取IP地址
 *          6. 等待5秒后检查连接结果
 * 
 * @return  true-连接成功且获取到IP, false-连接失败
 * @note    连接结果会更新wifi_state状态机
 */
bool WiFi_Connect(void)
{
    // 检查是否已配置WiFi
    if (!wifi_config.configured) {
        printf("[WiFi] 错误: WiFi未配置\n");
        snprintf(wifi_status_str, sizeof(wifi_status_str), "Not Configured");
        strncpy(wifi_ssid_display, "No Network", sizeof(wifi_ssid_display) - 1);
        wifi_connected = false;
        wifi_state = WIFI_STATE_FAILED;
        return false;
    }
    
    printf("[WiFi] 正在连接: %s\n", wifi_config.ssid);
    wifi_state = WIFI_STATE_CONNECTING;
    snprintf(wifi_status_str, sizeof(wifi_status_str), "Connecting...");
    
    // 步骤1：加载WiFi驱动（如果未加载）
    wifi_load_drivers();
    
    // 步骤2：停止旧的WiFi进程
    wifi_stop_processes();
    
    // 步骤3：生成wpa_supplicant配置文件
    // wpa_passphrase命令生成包含SSID和密码的配置文件
    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
        "wpa_passphrase \"%s\" \"%s\" > /tmp/wpa.conf",
        wifi_config.ssid, wifi_config.password);
    system(cmd);
    
    // 步骤4：启动wpa_supplicant进行WiFi认证
    // -B: 后台运行, -i wlan0: 指定接口, -c: 指定配置文件
    system("wpa_supplicant -B -i wlan0 -c /tmp/wpa.conf");
    
    // 步骤5：启动DHCP客户端获取IP地址
    system("udhcpc -i wlan0 &");
    
    // 步骤6：关闭WiFi电源节省模式（避免连接不稳定）
    system("iw dev wlan0 set power_save off");
    
    printf("[WiFi] WiFi连接指令已发送，等待获取IP...\n");
    
    // 步骤7：等待5秒让DHCP完成IP获取
    sleep(5);
    
    // 步骤8：检查是否成功获取IP
    if (WiFi_Is_Connected()) {
        printf("[WiFi] 连接成功，IP已获取\n");
        wifi_state = WIFI_STATE_CONNECTED;
        snprintf(wifi_status_str, sizeof(wifi_status_str), "Connected:%s", wifi_config.ssid);
        strncpy(wifi_ssid_display, wifi_config.ssid, sizeof(wifi_ssid_display) - 1);
        
        // 更新设备IP显示
        WiFi_Update_Device_IP();
        
        return true;
    } else {
        printf("[WiFi] 连接失败，未获取到IP\n");
        wifi_state = WIFI_STATE_FAILED;
        snprintf(wifi_status_str, sizeof(wifi_status_str), "Failed:%s", wifi_config.ssid);
        strncpy(wifi_ssid_display, "No Network", sizeof(wifi_ssid_display) - 1);
        return false;
    }
}

/**
 * @brief   断开WiFi连接
 * @details 停止WiFi相关进程，断开当前连接
 *          注意：此函数不会卸载驱动，只是断开连接
 * 
 * @note    用于屏幕端手动断开WiFi，或切换WiFi前的准备
 */
void WiFi_Disconnect(void)
{
    printf("[WiFi] 正在断开WiFi...\n");
    
    // 停止WiFi进程
    wifi_stop_processes();
    
    // 可选：卸载WiFi驱动（一般不执行，避免重启时出问题）
    // system("rmmod aic8800_fdrv.ko 2>/dev/null");
    // system("rmmod aic8800_bsp.ko 2>/dev/null");
    
    // 更新状态
    snprintf(wifi_status_str, sizeof(wifi_status_str), "Disconnected");
    strncpy(wifi_ssid_display, "No Network", sizeof(wifi_ssid_display) - 1);
    wifi_connected = false;
    printf("[WiFi] WiFi已断开\n");
}

/**
 * @brief   保存WiFi配置到文件
 * @details 将当前wifi_config保存到配置文件，实现掉电保存
 *          文件路径：/home/root/wifi_config.txt
 *          文件格式：
 *            SSID=WiFi名称
 *            PASSWORD=WiFi密码
 * 
 * @return  true-保存成功, false-保存失败
 * @note    下次开机时WiFi_Manager_Init()会自动读取此文件
 */
bool WiFi_Save_Config(void)
{
    // 检查是否有配置可保存
    if (!wifi_config.configured) {
        printf("[WiFi] 错误: 无配置可保存\n");
        return false;
    }
    
    // 打开配置文件（写入模式，覆盖原有内容）
    FILE* fp = fopen(WIFI_CONFIG_FILE, "w");
    if (fp == NULL) {
        printf("[WiFi] 错误: 无法创建配置文件\n");
        return false;
    }
    
    // 写入SSID和密码
    fprintf(fp, "SSID=%s\n", wifi_config.ssid);
    fprintf(fp, "PASSWORD=%s\n", wifi_config.password);
    fclose(fp);
    
    printf("[WiFi] 配置已保存到: %s\n", WIFI_CONFIG_FILE);
    return true;
}

/**
 * @brief   从文件读取WiFi配置
 * @details 从配置文件读取WiFi信息到wifi_config
 *          解析格式：SSID=xxx, PASSWORD=xxx
 * 
 * @return  true-读取成功, false-文件不存在或格式错误
 * @note    由WiFi_Manager_Init()调用，实现开机自动加载配置
 */
bool WiFi_Load_Config(void)
{
    // 打开配置文件（只读模式）
    FILE* fp = fopen(WIFI_CONFIG_FILE, "r");
    if (fp == NULL) {
        printf("[WiFi] 配置文件不存在: %s\n", WIFI_CONFIG_FILE);
        return false;
    }
    
    char line[128];
    wifi_config.configured = false;
    
    // 逐行读取配置文件
    while (fgets(line, sizeof(line), fp)) {
        // 去掉末尾换行符
        line[strcspn(line, "\n")] = 0;
        
        // 解析SSID行
        if (strncmp(line, "SSID=", 5) == 0) {
            strncpy(wifi_config.ssid, line + 5, sizeof(wifi_config.ssid) - 1);
            wifi_config.configured = true;
        }
        // 解析PASSWORD行
        else if (strncmp(line, "PASSWORD=", 9) == 0) {
            strncpy(wifi_config.password, line + 9, sizeof(wifi_config.password) - 1);
        }
    }
    
    fclose(fp);
    
    // 检查是否成功读取SSID
    if (wifi_config.configured) {
        printf("[WiFi] 配置加载成功: %s\n", wifi_config.ssid);
        return true;
    }
    
    return false;
}

/**
 * @brief   获取当前WiFi状态
 * @details 更新wifi_status_str为当前状态字符串
 * @deprecated 建议使用WiFi_Update_Device_IP()替代，功能更完整
 */
void WiFi_Update_Status(void)
{
    // 检查wlan0是否有IP
    char ip_buf[32] = {0};
    FILE* fp = popen("ifconfig wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d: -f2", "r");
    if (fp) {
        if (fgets(ip_buf, sizeof(ip_buf), fp) != NULL) {
            ip_buf[strcspn(ip_buf, "\n")] = 0;
            if (strlen(ip_buf) > 0) {
                snprintf(wifi_status_str, sizeof(wifi_status_str), "IP:%s", ip_buf);
            } else {
                snprintf(wifi_status_str, sizeof(wifi_status_str), "No IP");
            }
        } else {
            snprintf(wifi_status_str, sizeof(wifi_status_str), "Disconnected");
        }
        pclose(fp);
    }
}

/**
 * @brief   检查WiFi是否已连接
 * @details 检查wlan0接口是否有有效的IP地址
 * 
 * @return  true-已获取IP（已连接）, false-无IP（未连接）
 * @note    这是一个简单的状态检查，不更新全局变量
 */
bool WiFi_Is_Connected(void)
{
    char ip_buf[32] = {0};
    FILE* fp = popen("ifconfig wlan0 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d: -f2", "r");
    if (fp) {
        if (fgets(ip_buf, sizeof(ip_buf), fp) != NULL) {
            ip_buf[strcspn(ip_buf, "\n")] = 0;
            pclose(fp);
            // 检查IP字符串长度大于0表示有IP
            return (strlen(ip_buf) > 0);
        }
        pclose(fp);
    }
    return false;
}
