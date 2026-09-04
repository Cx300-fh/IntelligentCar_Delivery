/**
 * @file hx711.cpp
 * @brief HX711 称重模块实现（GPIO软件模拟通信）
 * @details 结构按老师要求拆成三层：
 *          1. GPIO初始化      -> HX711_GPIO_Init()
 *          2. 模块初始化/去皮  -> HX711_Init()（main的while循环之前调用一次，允许短暂等待）
 *          3. 通信+获取重量    -> HX711_Is_Ready() / HX711_Read_Raw() / HX711_Raw_To_Weight()
 *          4. 定时器服务函数   -> HX711_Service()（周期调用，内部严禁delay/while阻塞）
 *
 *          屏幕上一共放4个控件，2静2动：
 *          - "总重量"     : 静态文本标签，直接在HMI设计器里打字，不需要代码管，叫什么名字都行
 *          - t_weight_val : 【会变的】Text控件，显示总重量，比如 "235g"，随时刷新
 *          - "重量变动"   : 静态文本标签，同上，不需要代码管
 *          - t_delta_val  : 【会变的】Text控件，显示变化量，比如 "+235g"/"-80g"，
 *                           只在重量重新稳定后才刷新一次
 *
 *          注意：t_weight_val 和 t_delta_val 这两个名字必须跟代码里
 *          Screen_Send_Text() 传的名字一模一样（区分大小写），HMI里没建
 *          这两个控件的话，串口指令发过去屏幕也不会显示任何东西。
 *
 *          这两个值是龙芯主动定时推给屏幕的（HX711_Service()里每次读到新数据就发一次），
 *          所以 HMI 那边的"定时事件"完全不需要写任何代码去处理这两个控件——
 *          串口指令一到，t_weight_val.txt / t_delta_val.txt 就自动更新了，跟你原来
 *          处理 active_phase/active_slot 那种"轮询变量、自己拼文字"的写法不是一回事。
 */

#include "hx711.hpp"
#include <cmath>

/*============================================================================
 *                              全局变量定义
 *============================================================================*/
// 2026-09-04 实测标定（换电池后，供电干净）：226g手机
//   空载去皮 = -37797   加载 avg = -141550   delta = -103753 counts / 226g
//   factor = -459.1 counts/g
// 符号为正：空秤去皮=-141834，压上去读数往正方向走。
// （早先按 -37797 那次去皮算出过负号，那次去皮时秤上还压着东西，作废。）
//
// 旧电池时期标过一次 48.35，那是错的——当时电池压降让噪声占了信号的一半以上。
// HX711是比率式的，供电一脏读数就跟着脏。标定前务必确认电池是满的。
double hx711_calibration_factor = 211.3;
long   hx711_offset = 0;

float  weight_g          = 0.0f;

// 标定开关：true 时每秒往控制台打印一次原始值，用来算 hx711_calibration_factor。
// 标定完成、系数填好之后改成 false 即可（不用删代码，下次换秤台还要用）。
bool   hx711_debug_raw = true;

/*============================================================================
 *                              内部对象/参数
 *============================================================================*/
static ls_gpio  hx711_sck(HX711_SCK_PIN, GPIO_MODE_OUT);
static ls_gpio  hx711_dt(HX711_DT_PIN, GPIO_MODE_IN);
static lq_timer hx711_timer;   // 定时器：周期触发 HX711_Service()

// 下面三个阈值必须跟实测噪声匹配。滤波后噪声标准差约6.8g，
// 原来的1.5g/1.0g比噪声还小，会导致"永远等不到稳定"或者"每一拍都误报变化"。
#define STABLE_THRESHOLD_G   2.0f   // 相邻两次【滤波后】读数差小于这个值，认为"没在变化"
#define STABLE_TICKS_NEEDED  12     // 连续稳定这么多次读数，才认为"已经稳定下来"（12拍≈1.2秒）

#define HX711_CLK_DELAY_US   2      // SCK每个电平的保持时间（微秒）
#define MEDIAN_WINDOW        9      // 中值滤波窗口（奇数），吃掉偶发的跳变样本
#define AVG_WINDOW           40     // 中值之后再做滑动平均的窗口（40拍≈4秒）
#define ZERO_TRACK_BAND_G    5.0f   // 读数落在零点±这个范围内才允许零点跟踪
#define ZERO_TRACK_STEP      20     // 每拍把offset往当前读数方向挪这么多counts（≈0.4g/秒）

/**
 * @brief   微秒级忙等，只用于满足HX711的位时序，不是"等待传感器就绪"
 * @note    HX711手册：SCK高电平最短0.2us、最长50us（超过60us芯片会掉电休眠），
 *          DOUT在SCK下降沿后0.1us才稳定。裸跑寄存器读写只有几十纳秒，
 *          不加这个延时会违反建立时间，低位随机出错（实测标准差6000+counts）。
 *          单次读总共 24*2*2us ≈ 100us，每100ms才发生一次，可以忽略。
 *          这里必须忙等不能用usleep：usleep最小粒度几十微秒，
 *          会把SCK高电平拖过60us让HX711休眠。
 */
static inline void hx711_delay_us(unsigned us)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    long need = (long)us * 1000L;
    do
    {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    } while (((t1.tv_sec - t0.tv_sec) * 1000000000L + (t1.tv_nsec - t0.tv_nsec)) < need);
}

/**
 * @brief   对最近 MEDIAN_WINDOW 次原始读数取中值（纯计算，不碰GPIO、不阻塞）
 * @note    HX711接在编码器接口上跟电机/相机共电源，单次读数抖动很大，
 *          中值比均值好：能直接把偶发的整数级跳变样本丢掉，而不是被它拉偏。
 */
static long med_buf[MEDIAN_WINDOW];
static int  med_count = 0;
static int  med_idx   = 0;

static long avg_buf[AVG_WINDOW];
static long avg_sum   = 0;
static int  avg_count = 0;
static int  avg_idx   = 0;

/**
 * @brief   用去皮结果预填两级滤波器，让开机第一拍就是0克
 * @note    不预填的话缓冲区初值是0counts，而零点在-5万左右，
 *          开机头几秒会显示上百克的假读数（用户实测看到 -108.9g）。
 */
static void hx711_filter_seed(long v)
{
    for (int i = 0; i < MEDIAN_WINDOW; i++) med_buf[i] = v;
    med_count = MEDIAN_WINDOW;
    med_idx   = 0;

    for (int i = 0; i < AVG_WINDOW; i++) avg_buf[i] = v;
    avg_sum   = (long)v * AVG_WINDOW;
    avg_count = AVG_WINDOW;
    avg_idx   = 0;
}

static long hx711_median_filter(long sample)
{
    med_buf[med_idx] = sample;
    med_idx = (med_idx + 1) % MEDIAN_WINDOW;
    if (med_count < MEDIAN_WINDOW) med_count++;

    long tmp[MEDIAN_WINDOW];
    for (int i = 0; i < med_count; i++) tmp[i] = med_buf[i];

    // 插入排序（最多9个元素，开销可忽略）
    for (int i = 1; i < med_count; i++)
    {
        long key = tmp[i];
        int  j   = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = key;
    }
    return tmp[med_count / 2];
}

/**
 * @brief   滑动平均（纯计算，不碰GPIO、不阻塞）
 * @note    实测空载单次读数标准差1466 counts（按48.35 counts/g折合约30g），
 *          中值只能去掉跳变样本、削不动这种连续游走的模拟噪声，
 *          必须再平均一层。平均20拍把标准差降到 1466/sqrt(20) ≈ 328 counts ≈ 6.8g。
 *          代价是响应变慢约2秒——放件称重这个场景完全可以接受。
 */
static long hx711_moving_average(long sample)
{
    if (avg_count == AVG_WINDOW) avg_sum -= avg_buf[avg_idx];   // 窗口满了，先减掉即将被覆盖的那个
    avg_buf[avg_idx] = sample;
    avg_sum += sample;
    avg_idx = (avg_idx + 1) % AVG_WINDOW;
    if (avg_count < AVG_WINDOW) avg_count++;

    return avg_sum / avg_count;
}

/*============================================================================
 *                       第1步：GPIO初始化
 *============================================================================*/

/**
 * @brief   只初始化两个GPIO的方向（SCK输出、DT输入），不做别的事
 */
void HX711_GPIO_Init(void)
{
    hx711_sck.gpio_direction_set(GPIO_MODE_OUT);
    hx711_dt.gpio_direction_set(GPIO_MODE_IN);
    hx711_sck.gpio_level_set(GPIO_LOW);   // SCK 拉低 -> HX711进入正常工作模式（拉高>60us会掉电休眠）
}

/*============================================================================
 *                第2步：模块初始化（main循环之前调用，允许短暂阻塞）
 *============================================================================*/

/**
 * @brief   HX711模块初始化：GPIO初始化 + 去皮 + 启动周期服务定时器
 * @note    只在 main() 的 while 循环开始之前调用一次。老师说的
 *          "读取程序里不能有delay/while阻塞"针对的是下面的 HX711_Service()
 *          （定时器周期调用的那个函数），不是这里。
 */
void HX711_Init(void)
{
    HX711_GPIO_Init();

    printf("HX711 初始化中 (SCK=GPIO%d, DT=GPIO%d)...\n", HX711_SCK_PIN, HX711_DT_PIN);

    // 开机去皮：等待最多10次有效读数，取平均作为0点（仅此一处允许while等待）
    long sum = 0;
    int  got = 0;
    int  wait_ms = 0;
    while (got < 10 && wait_ms < 2000)   // 最多等2秒，防止没接HX711时卡死
    {
        long raw;
        if (HX711_Read_Raw(&raw))
        {
            sum += raw;
            got++;
        }
        else
        {
            usleep(1000);
            wait_ms++;
        }
    }
    hx711_offset = (got > 0) ? (sum / got) : 0;
    printf("HX711 去皮完成，零点偏移 = %ld（有效采样 %d 次）\n", hx711_offset, got);

    // 用去皮值预填滤波器，保证开机第一拍显示就是0克而不是几百克的假读数
    hx711_filter_seed(hx711_offset);
    weight_g = 0.0f;

    // 每 100ms 触发一次 HX711_Service()，把最新重量/变化量发送到屏幕
    hx711_timer.set_seconds_ms(100, HX711_Service);

    printf("HX711 初始化完成\n");
}

/*============================================================================
 *                       第3步：通信 + 获取重量
 *============================================================================*/

/**
 * @brief   查询HX711是否转换完成
 * @note    立即返回，不等待。DT为低电平代表数据已就绪。
 */
bool HX711_Is_Ready(void)
{
    return hx711_dt.gpio_level_get() == GPIO_LOW;
}

/**
 * @brief   非阻塞读取一次24位原始值
 * @param   out_value 读取成功时写入原始值，失败时不修改
 * @return  true=本次读取成功  false=还没转换好，本次不读（不阻塞、不等待）
 */
bool HX711_Read_Raw(long* out_value)
{
    if (!HX711_Is_Ready())
    {
        return false;   // 没转换好，直接放弃本次，不等待
    }

    uint32_t value = 0;

    // 24个时钟脉冲，每个脉冲读1位，MSB先出（固定次数for循环，微秒级，不是"等待型"while）
    for (int i = 0; i < 24; i++)
    {
        hx711_sck.gpio_level_set(GPIO_HIGH);
        hx711_delay_us(HX711_CLK_DELAY_US);   // 满足SCK高电平最短0.2us（且远小于60us休眠线）
        value = value << 1;
        hx711_sck.gpio_level_set(GPIO_LOW);
        hx711_delay_us(HX711_CLK_DELAY_US);   // 等DOUT稳定（下降沿后0.1us）再采样
        if (hx711_dt.gpio_level_get() == GPIO_HIGH)
        {
            value++;
        }
    }

    // 第25个脉冲：选择下一次转换用"通道A + 128倍增益"（HX711最常用配置）
    hx711_sck.gpio_level_set(GPIO_HIGH);
    hx711_delay_us(HX711_CLK_DELAY_US);
    hx711_sck.gpio_level_set(GPIO_LOW);
    hx711_delay_us(HX711_CLK_DELAY_US);

    // 24位补码 -> 有符号整数
    // 注意：龙芯是 LP64，long 有 8 字节，只 |= 0xFF000000 的话第32~63位还是0，
    // 负读数会变成 42 亿的正数；必须先补成 32 位再由 int32_t 符号扩展到 long。
    if (value & 0x800000u)
    {
        value |= 0xFF000000u;
    }

    long result = (long)(int32_t)value;

    // 废读判定：实测每分钟约有1拍读回 0xFFFFFF(-1)，即整个24周期DOUT都没拉低。
    // 这种样本会把均值拉飞（一个-1能把60个样本的均值抬高870 counts），
    // 当成"没就绪"丢掉即可，下一拍定时器会重试，不影响非阻塞要求。
    if (result == -1 || result == 0 ||
        result == 0x7FFFFF || result == -0x800000)
    {
        return false;
    }

    *out_value = result;
    return true;
}

/**
 * @brief   原始值换算成克数（纯计算，不碰GPIO，不会阻塞）
 */
float HX711_Raw_To_Weight(long raw)
{
    return (float)(raw - hx711_offset) / hx711_calibration_factor;
}

/*============================================================================
 *                第4步：定时器服务函数（周期调用，绝不阻塞）
 *============================================================================*/

/**
 * @brief   定时器服务函数：每次调用最多读一次HX711，更新总重量+变化量，发送到屏幕
 * @note    由 HX711_Init() 里的定时器每100ms自动调用一次。
 *          内部没有任何 delay/usleep，也没有 while 死等——
 *          没读到就什么都不做，直接返回，等下一次定时器触发。
 *
 *          变化量的判断逻辑：连续 STABLE_TICKS_NEEDED 次读数都没怎么变(<STABLE_THRESHOLD_G)，
 *          就认为"东西已经放稳/拿稳了"，这时候拿当前重量减去上一次的基准重量，
 *          如果差值够大(>DELTA_IGNORE_G)就当成一次真实的"放入/取出"，刷新 t_delta，
 *          然后把这次的重量存为新的基准，等待下一次变化。
 */
/**
 * @brief   定时器服务函数：每次调用最多读一次HX711，更新总重量并发送到屏幕
 * @note    由 HX711_Init() 里的定时器每100ms自动调用一次。
 *          内部没有任何 delay/usleep，也没有 while 死等——
 *          没读到就什么都不做，直接返回，等下一次定时器触发。
 *
 *          总重量同时往 t_weight_val 和 t_delta_val 两个控件都发一份：
 *          HMI 上这两个控件的标题贴反过，与其猜哪个是哪个，不如两个都发同一个值，
 *          屏幕上留哪个控件都能显示正确的总重量。以后HMI删掉多余的那个控件也不影响。
 */
void HX711_Service(void)
{
    static float prev_sample  = 0.0f;
    static int   stable_count = 0;

    long raw;
    if (!HX711_Read_Raw(&raw))
    {
        return;   // 这一拍HX711还没转换好，跳过，不阻塞
    }

    long  med      = hx711_median_filter(raw);   // 先中值：丢掉跳变样本
    long  filtered = hx711_moving_average(med);  // 再平均：压住连续游走的模拟噪声
    float w = HX711_Raw_To_Weight(filtered);

    // ---- 稳定判定（零点跟踪要用） ----
    if (fabsf(w - prev_sample) < STABLE_THRESHOLD_G)
    {
        if (stable_count < STABLE_TICKS_NEEDED) stable_count++;
    }
    else
    {
        stable_count = 0;
    }
    prev_sample = w;

    // ---- 零点跟踪：空秤且稳定时，缓慢把offset拉向当前读数，抵消模拟零点漂移 ----
    if (stable_count >= STABLE_TICKS_NEEDED && fabsf(w) < ZERO_TRACK_BAND_G)
    {
        if      (filtered > hx711_offset) hx711_offset += ZERO_TRACK_STEP;
        else if (filtered < hx711_offset) hx711_offset -= ZERO_TRACK_STEP;
    }

    // 重新标定 2026-09-04：459.1 时 63g 的砝码只显示 29g，
    // 说明系数偏大，按 459.1*29/63 修正为 211.3 counts/g。
    // 负数一律显示0：拿东西出去时零点漂移会让读数掉到负值，屏幕上显示 -12g 很怪。
    // 注意必须放在零点跟踪之后——跟踪要看真实的正负号才知道往哪个方向拉。
    if (w < 0.0f) w = 0.0f;

    weight_g = w;

    // ---- 发送总重量（两个控件发同一个值） ----
    char buf[16];
    snprintf(buf, sizeof(buf), "%dg", (int)weight_g);
    Screen_Send_Text("t_weight_val", buf);
    // t_delta_val 已在HMI里删掉，再发会被屏幕拒收(0x1A)，只留 t_weight_val

    // 排障用：每10拍（约1秒）打印一次，不需要了就把 hx711_debug_raw 置 false
    if (hx711_debug_raw)
    {
        static int dbg_tick = 0;
        if (++dbg_tick >= 10)
        {
            dbg_tick = 0;
            printf("[HX711] raw=%ld med=%ld avg=%ld off=%ld w=%.1fg stab=%d\n",
                   raw, med, filtered, hx711_offset, w, stable_count);
            fflush(stdout);
        }
    }
}

/*============================================================================
 *                              标定方法（做一次就行）
 *============================================================================
 * 1. 秤台空载，重启一次板子（HX711_Init里已经去皮），确认打印 w=0.0g。
 * 2. 放一个已知重量的物体，看控制台 [HX711] 打印里的 avg 值。
 * 3. hx711_calibration_factor = (avg_加载 - avg_空载) / 已知重量(克)
 * 4. 把算出来的值填回本文件开头的 hx711_calibration_factor 初始值。
 * 注意：HX711是比率式的，电池电压低会让读数噪声暴涨（实测能到信号的一半），
 *       标定前务必确认电池是满的。
 *===========================================================================*/
