#include "bsp_key.h"

/* 按键时间参数 (基于10ms调用周期) */
#define KEY_DEBOUNCE_CNT        2       // 20ms 防抖
#define KEY_DOUBLE_CLICK_TIME   30      // 300ms 双击判定窗口
#define KEY_LONG_PRESS_TIME     120     // 1200ms 长按判定 (1.2秒)

typedef enum {
    KEY_FSM_IDLE = 0,
    KEY_FSM_PRESS_DEBOUNCE,
    KEY_FSM_PRESSED,
    KEY_FSM_WAIT_SECOND_PRESS,
    KEY_FSM_SECOND_DEBOUNCE,
    KEY_FSM_SECOND_PRESSED,
    KEY_FSM_WAIT_THIRD_PRESS,
    KEY_FSM_THIRD_DEBOUNCE,
    KEY_FSM_WAIT_RELEASE
} KeyFsmState_e;

static KeyFsmState_e s_key_state = KEY_FSM_IDLE;
static uint16_t s_press_timer = 0;
static uint16_t s_double_timer = 0;

static uint8_t s_aux_last_state = 1; // 备用按键上拉，默认高

void BSP_Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA0: 主按键 (外部下拉，高电平有效) */
    GPIO_InitStructure.GPIO_Pin = MAIN_KEY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; // 内部下拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MAIN_KEY_PORT, &GPIO_InitStructure);

    /* PA6: 备用按键 (内部上拉，低电平有效) */
    GPIO_InitStructure.GPIO_Pin = AUX_KEY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 内部上拉输入
    GPIO_Init(AUX_KEY_PORT, &GPIO_InitStructure);

    s_key_state = KEY_FSM_IDLE;
    s_press_timer = 0;
    s_double_timer = 0;
}

uint8_t BSP_MainKey_IsPressed(void)
{
    // PA0 为高电平表示按下
    return (GPIO_ReadInputDataBit(MAIN_KEY_PORT, MAIN_KEY_PIN) == Bit_SET);
}

/**
 * @brief 每 10ms 调用一次，返回主按键事件 (单击/双击/三击/长按)
 */
Key_Event_e BSP_Key_Scan_10ms(void)
{
    Key_Event_e event = KEY_EVT_NONE;
    uint8_t pressed = BSP_MainKey_IsPressed();

    switch (s_key_state)
    {
        case KEY_FSM_IDLE:
            if (pressed)
            {
                s_key_state = KEY_FSM_PRESS_DEBOUNCE;
                s_press_timer = 0;
            }
            break;

        case KEY_FSM_PRESS_DEBOUNCE:
            if (pressed)
            {
                s_press_timer++;
                if (s_press_timer >= KEY_DEBOUNCE_CNT)
                {
                    s_key_state = KEY_FSM_PRESSED;
                }
            }
            else
            {
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        case KEY_FSM_PRESSED:
            if (pressed)
            {
                s_press_timer++;
                if (s_press_timer >= KEY_LONG_PRESS_TIME)
                {
                    // 长按 (1.2s)
                    event = KEY_EVT_LONG_PRESS;
                    s_key_state = KEY_FSM_WAIT_RELEASE;
                }
            }
            else
            {
                // 第一次短按松手，等待第二次按下
                s_key_state = KEY_FSM_WAIT_SECOND_PRESS;
                s_double_timer = 0;
            }
            break;

        case KEY_FSM_WAIT_SECOND_PRESS:
            s_double_timer++;
            if (pressed)
            {
                // 窗口期内按下第二击
                s_key_state = KEY_FSM_SECOND_DEBOUNCE;
                s_press_timer = 0;
            }
            else if (s_double_timer >= KEY_DOUBLE_CLICK_TIME)
            {
                // 超时未按第二击，确认为【单击】！
                event = KEY_EVT_SINGLE_CLICK;
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        case KEY_FSM_SECOND_DEBOUNCE:
            if (pressed)
            {
                s_press_timer++;
                if (s_press_timer >= KEY_DEBOUNCE_CNT)
                {
                    s_key_state = KEY_FSM_SECOND_PRESSED;
                }
            }
            else
            {
                event = KEY_EVT_SINGLE_CLICK;
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        case KEY_FSM_SECOND_PRESSED:
            if (pressed)
            {
                s_press_timer++;
                if (s_press_timer >= KEY_LONG_PRESS_TIME)
                {
                    event = KEY_EVT_LONG_PRESS;
                    s_key_state = KEY_FSM_WAIT_RELEASE;
                }
            }
            else
            {
                // 第二次短按松手，等待第三次按下
                s_key_state = KEY_FSM_WAIT_THIRD_PRESS;
                s_double_timer = 0;
            }
            break;

        case KEY_FSM_WAIT_THIRD_PRESS:
            s_double_timer++;
            if (pressed)
            {
                // 窗口期内按下第三击
                s_key_state = KEY_FSM_THIRD_DEBOUNCE;
                s_press_timer = 0;
            }
            else if (s_double_timer >= KEY_DOUBLE_CLICK_TIME)
            {
                // 超时未按第三击，确认为【双击】！
                event = KEY_EVT_DOUBLE_CLICK;
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        case KEY_FSM_THIRD_DEBOUNCE:
            if (pressed)
            {
                s_press_timer++;
                if (s_press_timer >= KEY_DEBOUNCE_CNT)
                {
                    // 第三击防抖完成，确认为【三连击】！
                    event = KEY_EVT_TRIPLE_CLICK;
                    s_key_state = KEY_FSM_WAIT_RELEASE;
                }
            }
            else
            {
                event = KEY_EVT_DOUBLE_CLICK;
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        case KEY_FSM_WAIT_RELEASE:
            if (!pressed)
            {
                s_key_state = KEY_FSM_IDLE;
            }
            break;

        default:
            s_key_state = KEY_FSM_IDLE;
            break;
    }

    return event;
}

/**
 * @brief 备用按键扫描 (低电平按下)
 */
AuxKey_Event_e BSP_AuxKey_Scan_10ms(void)
{
    AuxKey_Event_e evt = AUX_KEY_EVT_NONE;
    uint8_t curr = GPIO_ReadInputDataBit(AUX_KEY_PORT, AUX_KEY_PIN);

    if (s_aux_last_state == 1 && curr == 0)
    {
        evt = AUX_KEY_EVT_PRESS;
    }
    else if (s_aux_last_state == 0 && curr == 1)
    {
        evt = AUX_KEY_EVT_RELEASE;
    }
    s_aux_last_state = curr;
    return evt;
}
