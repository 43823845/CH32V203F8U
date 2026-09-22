#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "ch32v20x.h"

/*
 * 按键硬件连接与引脚对应关系 (CH32V203F8U6 - QFN20):
 * -------------------------------------------------------------
 * 【SW1 主功能按键】 -> 对应物理引脚：Pin 1 (PA0 / WKUP)
 *   - 硬件连接：按键一端接 +3.0V(VDD)，另一端接 PA0 (Pin 1)。
 *   - 平时状态：PA0 外部下拉 100kΩ 电阻到 GND，未按下为低电平(0)，按下为高电平(1)。
 *   - 核心功能：
 *     1. 唤醒/开机：待机(Standby)时，PA0作为硬件 WKUP 引脚，短按瞬间唤醒开机点亮模式 1。
 *     2. 运行控制：单击切档(100%->25%->双通)、双击10Hz爆闪、长按(>1.2s)关机复位。
 *     3. 免下载器刷机：关机状态下按住 SW1 (PA0) 持续 8 秒，MCU 自动平滑跳转进入 USB ISP 烧录模式。
 *
 * 【SW2 备用按键】   -> 对应物理引脚：Pin 20 (PA6)
 *   - 硬件连接：按键一端接 PA6 (Pin 20)，另一端接 GND，芯片内部开启上拉电阻。
 *   - 核心功能：战术点动高亮 (按住即 100% 满功率点亮，松手恢复)。
 * -------------------------------------------------------------
 */

#define SW1_MAIN_KEY_PORT   GPIOA
#define SW1_MAIN_KEY_PIN    GPIO_Pin_0          // Pin 1 (PA0 / WKUP)
#define MAIN_KEY_PORT       SW1_MAIN_KEY_PORT
#define MAIN_KEY_PIN        SW1_MAIN_KEY_PIN

#define SW2_AUX_KEY_PORT    GPIOA
#define SW2_AUX_KEY_PIN     GPIO_Pin_6          // Pin 20 (PA6)
#define AUX_KEY_PORT        SW2_AUX_KEY_PORT
#define AUX_KEY_PIN         SW2_AUX_KEY_PIN

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_SINGLE_CLICK,   // 单击
    KEY_EVT_DOUBLE_CLICK,   // 双击
    KEY_EVT_TRIPLE_CLICK,   // 三连击
    KEY_EVT_LONG_PRESS,     // 长按 (超过1.2秒，关机复位)
    KEY_EVT_RELEASE         // 释放
} Key_Event_e;

typedef enum {
    AUX_KEY_EVT_NONE = 0,
    AUX_KEY_EVT_PRESS,      // 备用按键按下(可做战术点射)
    AUX_KEY_EVT_RELEASE     // 备用按键松开
} AuxKey_Event_e;

void BSP_Key_Init(void);
Key_Event_e BSP_Key_Scan_10ms(void);
AuxKey_Event_e BSP_AuxKey_Scan_10ms(void);
uint8_t BSP_MainKey_IsPressed(void);

#endif /* __BSP_KEY_H */
