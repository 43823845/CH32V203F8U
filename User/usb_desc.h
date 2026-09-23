#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "ch32v20x.h"

#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05

#define CDC_DESCRIPTOR_TYPE                     0x21
#define CDC_HEADER_FUNC_DESC                    0x00
#define CDC_CALL_MANAGE_FUNC_DESC               0x01
#define CDC_ACM_FUNC_DESC                       0x02
#define CDC_UNION_FUNC_DESC                     0x06

#define USB_CDC_CMD_PACKET_SIZE                 8
#define USB_CDC_DATA_PACKET_SIZE                64

#define USB_DESC_DEVICE_LEN             18
#define USB_DESC_CONFIG_TOTAL_LEN       67
#define USB_DESC_STRING_LANG_LEN        4
#define USB_DESC_STRING_VENDOR_LEN      8
#define USB_DESC_STRING_PRODUCT_LEN     38
#define USB_DESC_STRING_SERIAL_LEN      18

extern const uint8_t USB_DeviceDescriptor[USB_DESC_DEVICE_LEN];
extern const uint8_t USB_ConfigDescriptor[USB_DESC_CONFIG_TOTAL_LEN];
extern const uint8_t USB_StringLangID[USB_DESC_STRING_LANG_LEN];
extern const uint8_t USB_StringVendor[USB_DESC_STRING_VENDOR_LEN];
extern const uint8_t USB_StringProduct[USB_DESC_STRING_PRODUCT_LEN];
extern const uint8_t USB_StringSerial[USB_DESC_STRING_SERIAL_LEN];

#endif /* __USB_DESC_H */
