#ifndef __USB_CDC_H
#define __USB_CDC_H

#include "ch32v20x.h"

#define USB_CDC_RX_BUF_SIZE    256
#define USB_CDC_TX_BUF_SIZE    512

void     USB_CDC_Init(void);
void     USB_CDC_DeInit(void);
uint8_t  USB_CDC_IsConfigured(void);
uint32_t USB_CDC_SendBytes(const uint8_t *data, uint32_t len);
uint32_t USB_CDC_Available(void);
int16_t  USB_CDC_ReadByte(void);
uint32_t USB_CDC_ReadBytes(uint8_t *buf, uint32_t max_len);

// 供 USBLIB 端点中断回调使用
void     USB_CDC_OnRxChunk(const uint8_t *data, uint32_t len);
void     USB_CDC_OnTxComplete(void);

#endif /* __USB_CDC_H */
