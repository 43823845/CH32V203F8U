#ifndef __BSP_ISP_H
#define __BSP_ISP_H

#include "ch32v20x.h"

/**
 * @brief 软件平滑跳转进入内置系统 Bootloader (ISP 烧录模式)
 * @note  执行此函数后将复位所有外设、时钟及内核状态，直接跳转至 0x1FFFF000。
 *        电脑可通过 Type-C 原生 USB 识别为 WCH ISP 设备进行免下载器刷机。
 */
void BSP_ISP_JumpToBootloader(void);

#endif /* __BSP_ISP_H */
