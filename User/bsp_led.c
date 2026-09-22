#include "bsp_led.h"
#include "debug.h"

static StatusLed_Mode_e s_current_mode = STATUS_LED_OFF;
static uint16_t s_led_timer = 0;

/* 动态自适应不同时钟频率的时序计数值 (基准为每次 addi+bnez 循环耗时约 3 个周期) */
static uint32_t s_t0h_cycles = 9;
static uint32_t s_t0l_cycles = 25;
static uint32_t s_t1h_cycles = 25;
static uint32_t s_t1l_cycles = 11;

static inline void ws2812_delay_ticks(uint32_t count)
{
    asm volatile(
        "1: addi %0, %0, -1\n"
        "   bnez %0, 1b\n"
        : "+r"(count)
    );
}

/**
 * @brief 发送单个 bit 归零码 (800kHz)
 */
static inline void ws2812_send_bit(uint8_t bit)
{
    if (bit)
    {
        // 1 码: 高电平 ~800ns, 低电平 ~350ns
        GPIOA->BSHR = LED_RGB_PIN;
        ws2812_delay_ticks(s_t1h_cycles);
        GPIOA->BCR = LED_RGB_PIN;
        ws2812_delay_ticks(s_t1l_cycles);
    }
    else
    {
        // 0 码: 高电平 ~300ns, 低电平 ~800ns
        GPIOA->BSHR = LED_RGB_PIN;
        ws2812_delay_ticks(s_t0h_cycles);
        GPIOA->BCR = LED_RGB_PIN;
        ws2812_delay_ticks(s_t0l_cycles);
    }
}

/**
 * @brief 向单线 WS2812B / XL-1615RGBC 发送 24 位 GRB 颜色数据
 */
static void ws2812_send_24bit(uint8_t r, uint8_t g, uint8_t b)
{
    // WS2812 通信顺序为: G[7..0], R[7..0], B[7..0]
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    __disable_irq();

    for (int8_t i = 23; i >= 0; i--)
    {
        ws2812_send_bit((grb >> i) & 0x01);
    }

    __enable_irq();

    // 产生 > 80us 低电平复位锁存信号
    GPIOA->BCR = LED_RGB_PIN;
    Delay_Us(80);
}

void BSP_StatusLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // 使能 GPIOA 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // PA4 推挽输出 (单线 RGB 数据引脚)
    GPIO_InitStructure.GPIO_Pin = LED_RGB_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_RGB_PORT, &GPIO_InitStructure);

    // 计算当前主频下各时序对应的循环计数
    uint32_t mhz = SystemCoreClock / 1000000;
    if (mhz == 0) mhz = 8; // 兜底

    // 单次循环约消耗 3 周期 (3000 / mhz 纳秒)
    s_t0h_cycles = (300 * mhz) / 3000;
    s_t0l_cycles = (800 * mhz) / 3000;
    s_t1h_cycles = (800 * mhz) / 3000;
    s_t1l_cycles = (350 * mhz) / 3000;

    if (s_t0h_cycles < 2) s_t0h_cycles = 2;
    if (s_t0l_cycles < 4) s_t0l_cycles = 4;
    if (s_t1h_cycles < 4) s_t1h_cycles = 4;
    if (s_t1l_cycles < 2) s_t1l_cycles = 2;

    BSP_StatusLED_AllOff();
}

/**
 * @brief 直接设置当前 RGB 物理灯珠颜色 (0~255)
 */
void BSP_StatusLED_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    ws2812_send_24bit(r, g, b);
}

/**
 * @brief 关闭 RGB 指示灯
 */
void BSP_StatusLED_AllOff(void)
{
    s_current_mode = STATUS_LED_OFF;
    s_led_timer = 0;
    BSP_StatusLED_SetColor(0, 0, 0);
    GPIO_ResetBits(LED_RGB_PORT, LED_RGB_PIN);
}

/**
 * @brief 设置功能指示灯工作模式
 */
void BSP_StatusLED_SetMode(StatusLed_Mode_e mode)
{
    if (s_current_mode == mode) return;

    s_current_mode = mode;
    s_led_timer = 0;

    // 立即响应初始颜色
    switch (s_current_mode)
    {
        case STATUS_LED_OFF:
            BSP_StatusLED_SetColor(0, 0, 0);
            break;

        case STATUS_LED_BAT_HIGH:
            // 绿色常亮 (电量充足)
            BSP_StatusLED_SetColor(0, 255, 0);
            break;

        case STATUS_LED_BAT_MED:
            // 蓝色常亮 (电量良好)
            BSP_StatusLED_SetColor(0, 120, 255);
            break;

        case STATUS_LED_BAT_LOW:
            // 黄色初始亮 (低电提醒慢闪)
            BSP_StatusLED_SetColor(255, 160, 0);
            break;

        case STATUS_LED_BAT_CRITICAL:
            // 红色初始亮 (极低电快闪)
            BSP_StatusLED_SetColor(255, 0, 0);
            break;

        case STATUS_LED_STROBE:
            // 青色/冰蓝频闪 (爆闪指示)
            BSP_StatusLED_SetColor(0, 255, 255);
            break;

        case STATUS_LED_SOS:
            // 紫色/洋红 (SOS求救指示)
            BSP_StatusLED_SetColor(200, 0, 255);
            break;

        case STATUS_LED_ISP:
            // 金黄色常亮 (进入 ISP 刷机指示)
            BSP_StatusLED_SetColor(255, 200, 0);
            break;

        default:
            break;
    }
}

/**
 * @brief 每 10ms 调用一次，处理指示灯闪烁及动态效果
 */
void BSP_StatusLED_Process_10ms(void)
{
    s_led_timer++;

    switch (s_current_mode)
    {
        case STATUS_LED_BAT_LOW:
            // 黄灯 1Hz 慢闪: 500ms 亮, 500ms 灭
            if (s_led_timer == 50)
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            else if (s_led_timer >= 100)
            {
                s_led_timer = 0;
                BSP_StatusLED_SetColor(255, 160, 0);
            }
            break;

        case STATUS_LED_BAT_CRITICAL:
            // 红灯 4Hz 快闪: 120ms 亮, 130ms 灭
            if (s_led_timer == 12)
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            else if (s_led_timer >= 25)
            {
                s_led_timer = 0;
                BSP_StatusLED_SetColor(255, 0, 0);
            }
            break;

        case STATUS_LED_STROBE:
            // 青色随 10Hz 爆闪同步翻转: 50ms 亮, 50ms 灭
            if (s_led_timer == 5)
            {
                BSP_StatusLED_SetColor(0, 0, 0);
            }
            else if (s_led_timer >= 10)
            {
                s_led_timer = 0;
                BSP_StatusLED_SetColor(0, 255, 255);
            }
            break;

        default:
            // 常亮模式或关闭模式无需定时翻转
            break;
    }
}
