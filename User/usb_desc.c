#include "usb_desc.h"

/* USB Device Descriptor (Standard CDC ACM) */
const uint8_t USB_DeviceDescriptor[] =
{
    18,                             // bLength
    USB_DEVICE_DESCRIPTOR_TYPE,     // bDescriptorType (Device)
    0x10, 0x01,                     // bcdUSB: 1.10
    0x02,                           // bDeviceClass: CDC
    0x00,                           // bDeviceSubClass
    0x00,                           // bDeviceProtocol
    64,                             // bMaxPacketSize0: 64 bytes
    0x86, 0x1A,                     // idVendor: 0x1A86 (WCH)
    0x0C, 0xFE,                     // idProduct: 0xFE0C (WCH CDC Virtual COM Port)
    0x00, 0x01,                     // bcdDevice: 1.00
    1,                              // iManufacturer
    2,                              // iProduct
    3,                              // iSerialNumber
    1                               // bNumConfigurations
};

/* USB Configuration Descriptor Set (Total length: 67 bytes) */
const uint8_t USB_ConfigDescriptor[USB_DESC_CONFIG_TOTAL_LEN] =
{
    /* Configuration Descriptor */
    0x09,                           // bLength
    USB_CONFIGURATION_DESCRIPTOR_TYPE, // bDescriptorType
    USB_DESC_CONFIG_TOTAL_LEN, 0x00,// wTotalLength (67 bytes)
    0x02,                           // bNumInterfaces: 2 (Control + Data)
    0x01,                           // bConfigurationValue: 1
    0x00,                           // iConfiguration
    0x80,                           // bmAttributes: Bus powered
    0x32,                           // bMaxPower: 100 mA

    /* Interface 0: CDC Communication Interface */
    0x09,                           // bLength
    USB_INTERFACE_DESCRIPTOR_TYPE,  // bDescriptorType
    0x00,                           // bInterfaceNumber: 0
    0x00,                           // bAlternateSetting: 0
    0x01,                           // bNumEndpoints: 1 (Notification EP1)
    0x02,                           // bInterfaceClass: CDC (Communication Interface Class)
    0x02,                           // bInterfaceSubClass: Abstract Control Model (ACM)
    0x01,                           // bInterfaceProtocol: Common AT commands
    0x00,                           // iInterface

    /* CDC Header Functional Descriptor */
    0x05,                           // bLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x00,                           // bDescriptorSubtype: Header Functional Descriptor
    0x10, 0x01,                     // bcdCDC: 1.10

    /* CDC Call Management Functional Descriptor */
    0x05,                           // bLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x01,                           // bDescriptorSubtype: Call Management
    0x00,                           // bmCapabilities: Device handles call management itself
    0x01,                           // bDataInterface: 1

    /* CDC ACM Functional Descriptor */
    0x04,                           // bLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x02,                           // bDescriptorSubtype: Abstract Control Management
    0x02,                           // bmCapabilities: Line Coding & Serial State support

    /* CDC Union Functional Descriptor */
    0x05,                           // bLength
    0x24,                           // bDescriptorType: CS_INTERFACE
    0x06,                           // bDescriptorSubtype: Union Functional Descriptor
    0x00,                           // bMasterInterface: 0 (Communication Class Interface)
    0x01,                           // bSlaveInterface0: 1 (Data Class Interface)

    /* Endpoint 1: CDC Notification (Interrupt IN) */
    0x07,                           // bLength
    USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType
    0x81,                           // bEndpointAddress: EP1 IN
    0x03,                           // bmAttributes: Interrupt
    USB_CDC_CMD_PACKET_SIZE, 0x00,  // wMaxPacketSize: 8 bytes
    0x0A,                           // bInterval: 10 ms

    /* Interface 1: CDC Data Interface */
    0x09,                           // bLength
    USB_INTERFACE_DESCRIPTOR_TYPE,  // bDescriptorType
    0x01,                           // bInterfaceNumber: 1
    0x00,                           // bAlternateSetting: 0
    0x02,                           // bNumEndpoints: 2 (Data IN EP2, Data OUT EP3)
    0x0A,                           // bInterfaceClass: CDC Data
    0x00,                           // bInterfaceSubClass: 0
    0x00,                           // bInterfaceProtocol: 0
    0x00,                           // iInterface

    /* Endpoint 2: Data IN (Bulk IN, Device -> Host) */
    0x07,                           // bLength
    USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType
    0x82,                           // bEndpointAddress: EP2 IN
    0x02,                           // bmAttributes: Bulk
    USB_CDC_DATA_PACKET_SIZE, 0x00, // wMaxPacketSize: 64 bytes
    0x00,                           // bInterval: 0 (ignored for Bulk)

    /* Endpoint 3: Data OUT (Bulk OUT, Host -> Device) */
    0x07,                           // bLength
    USB_ENDPOINT_DESCRIPTOR_TYPE,   // bDescriptorType
    0x03,                           // bEndpointAddress: EP3 OUT
    0x02,                           // bmAttributes: Bulk
    USB_CDC_DATA_PACKET_SIZE, 0x00, // wMaxPacketSize: 64 bytes
    0x00                            // bInterval: 0 (ignored for Bulk)
};

/* Language ID: English (United States) */
const uint8_t USB_StringLangID[] =
{
    0x04,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09, 0x04
};

/* Manufacturer: "WCH" */
const uint8_t USB_StringVendor[] =
{
    0x08,
    USB_STRING_DESCRIPTOR_TYPE,
    'W', 0x00, 'C', 0x00, 'H', 0x00
};

/* Product: "CH32V203 WML Debug" */
const uint8_t USB_StringProduct[] =
{
    38,
    USB_STRING_DESCRIPTOR_TYPE,
    'C', 0x00, 'H', 0x00, '3', 0x00, '2', 0x00, 'V', 0x00,
    '2', 0x00, '0', 0x00, '3', 0x00, ' ', 0x00, 'W', 0x00,
    'M', 0x00, 'L', 0x00, ' ', 0x00, 'D', 0x00, 'e', 0x00,
    'b', 0x00, 'u', 0x00, 'g', 0x00
};

/* Serial Number: "WML-2026" */
const uint8_t USB_StringSerial[] =
{
    18,
    USB_STRING_DESCRIPTOR_TYPE,
    'W', 0x00, 'M', 0x00, 'L', 0x00, '-', 0x00,
    '2', 0x00, '0', 0x00, '2', 0x00, '6', 0x00
};
