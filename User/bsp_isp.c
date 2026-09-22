#include "bsp_isp.h"
#include "bsp_pwm.h"
#include "bsp_led.h"
#include "bsp_adc.h"
#include "debug.h"

void BSP_ISP_JumpToBootloader(void)
{
    PRINT("\r\n[ISP] Entering Factory ISP Bootloader Mode...\r\n");

    // 1. 关断 PWM 与 ADC
    BSP_PWM_AllOff();
    BSP_ADC_DeInit();

    // 2. RGB 指示灯点亮金黄色，提示用户正在进入 ISP 刷机模式
    BSP_StatusLED_SetMode(STATUS_LED_ISP);
    Delay_Ms(300);

    // 3. 关闭全局中断
    __disable_irq();

    // 4. 关闭 RGB 指示灯并拉低引脚
    BSP_StatusLED_AllOff();

    // 5. 复位 SysTick 计数器与控制寄存器
    SysTick->CTLR = 0;
    SysTick->SR   = 0;
    SysTick->CNT  = 0;
    SysTick->CMP  = 0;

    // 6. 清除 PFIC 中断控制器的所有使能与挂起状态
    for (uint8_t i = 0; i < 8; i++)
    {
        NVIC->IRER[i] = 0xFFFFFFFF; // 关闭中断使能
        NVIC->IPRR[i] = 0xFFFFFFFF; // 清除挂起中断
    }

    // 7. 外设总线复位并释放，恢复 GPIO (特别是 PA11/USB_DM, PA12/USB_DP) 为初始复位态
    RCC_APB2PeriphResetCmd(0xFFFFFFFF, ENABLE);
    RCC_APB2PeriphResetCmd(0xFFFFFFFF, DISABLE);
    RCC_APB1PeriphResetCmd(0xFFFFFFFF, ENABLE);
    RCC_APB1PeriphResetCmd(0xFFFFFFFF, DISABLE);

    // 8. 将系统核心时钟复位为默认内部高速振荡器 (HSI 8MHz)
    RCC_DeInit();

    // 9. 内核寄存器复位并无条件平滑跳转至系统存储区 (0x1FFFF000)
    asm volatile(
        "csrw mtvec, zero\n"
        "li t0, 0x1800\n"       // MPP = 3 (Machine mode)
        "csrw mstatus, t0\n"
        "li sp, 0x20005000\n"   // 栈指针重置为 20KB SRAM 顶部
        "li t0, 0x1FFFF000\n"   // 原厂 ROM Bootloader 入口基地址
        "jr t0\n"
    );

    while (1);
}
