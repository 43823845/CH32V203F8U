#ifndef __GUNLIGHT_H
#define __GUNLIGHT_H

#include "ch32v20x.h"

typedef enum {
    GL_STATE_OFF = 0,          // 全部关闭/灭灯等待，10秒无操作进低功耗
    GL_STATE_MODE1_100,        // 模式1: 主灯 100%, 副灯 0%
    GL_STATE_MODE2_25,         // 模式2: 主灯 25%,  副灯 0%
    GL_STATE_MODE3_DUAL,       // 模式3: 主灯 (3535 WLED 100%) + 副灯 (650nm ~3mW 红光激光瞄准) 同亮
    GL_STATE_STROBE,           // 双击: 10Hz 战术爆闪
    GL_STATE_SOS               // 三击: SOS 救援模式 (三短 三长 三短)
} Gunlight_State_e;

void Gunlight_Init(void);
void Gunlight_Process_10ms(void);
void Gunlight_Enter_LowPower_Standby(void);
void Gunlight_TurnOn_Mode1(void);

#endif /* __GUNLIGHT_H */
