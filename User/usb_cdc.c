/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.c
 * Author             : WCH & Antigravity
 * Version            : V2.0.0 (Based on official USBD peripheral)
 * Description        : CH32V203F8U6 hardware USBD CDC driver with ring buffers
 *******************************************************************************/
#include "usb_cdc.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "hw_config.h"

extern uint8_t USBD_Endp3_Busy;
extern uint8_t USBD_ENDPx_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len);

/* 环形接收缓冲区 */
static uint8_t  s_rx_buf[USB_CDC_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

/* 环形发送缓冲区 */
static uint8_t  s_tx_buf[USB_CDC_TX_BUF_SIZE];
static volatile uint16_t s_tx_head = 0;
static volatile uint16_t s_tx_tail = 0;
static uint8_t  s_tx_packet[64];

static void USB_CDC_TriggerNextTx(void);

/**
 * @brief 初始化 USB 虚拟串口 (USBD 外设)
 */
void USB_CDC_Init(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
    s_tx_head = 0;
    s_tx_tail = 0;
    USBD_Endp3_Busy = 0;

    // 1. 配置 USB 48MHz 时钟并开启 APB1 外设时钟
    Set_USBConfig();

    // 2. 初始化 USBD 底层寄存器与描述符
    USB_Init();

    // 3. 配置并使能 USB 数据与唤醒中断
    USB_Interrupts_Config();

    // 4. 浮空 PA11/PA12 并使能片内 1.5k DP 上拉电阻，通知 PC 主机枚举
    USB_Port_Set(ENABLE, ENABLE);
}

/**
 * @brief 释放并安全关闭 USB 虚拟串口 (待机休眠前调用，消除毫安级漏电)
 */
void USB_CDC_DeInit(void)
{
    // 1. 断开片内 1.5k DP 上拉电阻，将 PA11/PA12 置低电平
    USB_Port_Set(DISABLE, DISABLE);

    // 2. 关闭 APB1 总线上的 USB 外设时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, DISABLE);

    // 3. 关闭中断
    NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    NVIC_DisableIRQ(USBWakeUp_IRQn);

    bDeviceState = UNCONNECTED;
    USBD_Endp3_Busy = 0;
}

/**
 * @brief 查询 USB 是否已成功枚举配置完成
 */
uint8_t USB_CDC_IsConfigured(void)
{
    return (bDeviceState == CONFIGURED);
}

/**
 * @brief 从发送环形缓冲中取出最多 64 字节送入硬件端点 3
 */
static void USB_CDC_TriggerNextTx(void)
{
    if (bDeviceState != CONFIGURED || USBD_Endp3_Busy)
    {
        return;
    }

    uint16_t head = s_tx_head;
    uint16_t tail = s_tx_tail;
    if (head == tail)
    {
        return; // 发送缓冲已空
    }

    uint16_t count = 0;
    while (tail != head && count < 64)
    {
        s_tx_packet[count++] = s_tx_buf[tail];
        tail = (tail + 1) % USB_CDC_TX_BUF_SIZE;
    }
    s_tx_tail = tail;

    if (count > 0)
    {
        USBD_ENDPx_DataUp(ENDP3, s_tx_packet, count);
    }
}

/**
 * @brief 发送数据到 USB 虚拟串口 (供 printf 重定向与应用层调用)
 */
uint32_t USB_CDC_SendBytes(const uint8_t *data, uint32_t len)
{
    if (data == 0 || len == 0)
    {
        return 0;
    }

    // 若尚未枚举连接，不阻塞直接返回
    if (bDeviceState != CONFIGURED)
    {
        return len;
    }

    uint32_t written = 0;
    while (written < len)
    {
        uint16_t next = (s_tx_head + 1) % USB_CDC_TX_BUF_SIZE;
        if (next == s_tx_tail)
        {
            // 环形缓冲已满，尝试触发一次发送以腾出空间
            USB_CDC_TriggerNextTx();
            break;
        }
        s_tx_buf[s_tx_head] = data[written++];
        s_tx_head = next;
    }

    USB_CDC_TriggerNextTx();
    return written;
}

/**
 * @brief 接收数据包钩子 (由 EP2_OUT_Callback 在中断中调用)
 */
void USB_CDC_OnRxChunk(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        uint16_t next = (s_rx_head + 1) % USB_CDC_RX_BUF_SIZE;
        if (next != s_rx_tail)
        {
            s_rx_buf[s_rx_head] = data[i];
            s_rx_head = next;
        }
    }
}

/**
 * @brief 发送完成中断钩子 (由 EP3_IN_Callback 在中断中调用)
 */
void USB_CDC_OnTxComplete(void)
{
    USB_CDC_TriggerNextTx();
}

/**
 * @brief 查询接收缓冲区可读字节数
 */
uint32_t USB_CDC_Available(void)
{
    return (s_rx_head >= s_rx_tail) ?
           (s_rx_head - s_rx_tail) :
           (USB_CDC_RX_BUF_SIZE - s_rx_tail + s_rx_head);
}

/**
 * @brief 读取单个字节
 */
int16_t USB_CDC_ReadByte(void)
{
    if (s_rx_head == s_rx_tail)
    {
        return -1;
    }
    uint8_t byte = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % USB_CDC_RX_BUF_SIZE;
    return byte;
}

/**
 * @brief 批量读取字节
 */
uint32_t USB_CDC_ReadBytes(uint8_t *buf, uint32_t max_len)
{
    uint32_t count = 0;
    while (count < max_len)
    {
        int16_t b = USB_CDC_ReadByte();
        if (b < 0)
        {
            break;
        }
        buf[count++] = (uint8_t)b;
    }
    return count;
}
