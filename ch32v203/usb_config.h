#ifndef _USB_CONFIG_H
#define _USB_CONFIG_H

// Optional USB serial identity/configuration, read by ch32fun's USB library.
// Only DEBUG_USB=1 compiles that library. This file describes the device to
// the computer; rainus.c handles incoming commands and outgoing status text.
//
// CDC (Communications Device Class) makes it appear as a standard serial port.
// An endpoint (EP) is a numbered USB communication channel, not a GPIO pin:
//   EP0: initial device setup, managed by the library.
//   EP1: board -> computer serial-status notifications.
//   EP2: computer -> board command characters (HandleUSBInput callback).
//   EP3: board -> computer status text (USBD_SendEndpointNEW).
// USB calls directions IN/OUT from the COMPUTER's perspective: IN is data
// coming in to the computer; OUT is data sent out from it. TX/RX below are
// transmit/receive from the BOARD's perspective.
//
// Descriptor arrays are fixed-format USB packets, not executable code.
// Their numeric fields identify lengths, types, interfaces and endpoints as
// required by USB. Comments retain specification field names for reference.
// Keep their bytes/lengths in agreement when changing the advertised device.

#include "funconfig.h"
#include "ch32fun.h"

#define FUSB_BUFFERS_NUMBER   4 // One buffer each for setup, notifications, receive, send.
#define FUSB_EP1_MODE         USBD_EP_MODE_TX // Notifications to the computer (IN).
#define FUSB_EP2_MODE         USBD_EP_MODE_RX // Commands from the computer (OUT).
#define FUSB_EP3_MODE         USBD_EP_MODE_TX // Status text to the computer (IN).
#define FUSB_USER_HANDLERS    1 // Ask rainus.c's HandleUSBInput to process received bytes.

#include "usb_defines.h"

#define FUSB_USB_VID 0x1209 // Advertised vendor ID (VID).
#define FUSB_USB_PID 0xd035 // Advertised product ID (PID).
#define FUSB_USB_REV 0x0007 // Advertised device revision.
#define FUSB_STR_MANUFACTURER u"Rainus"
#define FUSB_STR_PRODUCT      u"Rainus Bench Debug"
#define FUSB_STR_SERIAL       u"007" // USB descriptor string, NOT the chip's factory ID.

//Taken from http://www.usbmadesimple.co.uk/ums_ms_desc_dev.htm
static const uint8_t device_descriptor[] = {
	18, //bLength - Length of this descriptor
	1,  //bDescriptorType - Type (Device)
	0x10, 0x01, //bcdUSB - The highest USB spec version this device supports (USB1.1)
	0x02, //bDeviceClass - Communications Device Class (virtual serial).
	0x0, //bDeviceSubClass - Device Subclass
	0x0, //bDeviceProtocol - Device Protocol  (000 = use config descriptor)
	64, //bMaxPacketSize - Max packet size for EP0
  (uint8_t)(FUSB_USB_VID), (uint8_t)(FUSB_USB_VID >> 8), //idVendor - ID Vendor
	(uint8_t)(FUSB_USB_PID), (uint8_t)(FUSB_USB_PID >> 8), //idProduct - ID Product
	(uint8_t)(FUSB_USB_REV), (uint8_t)(FUSB_USB_REV >> 8), //bcdDevice - Device Release Number
	1, //iManufacturer - Index of Manufacturer string
	2, //iProduct - Index of Product string
	3, //iSerialNumber - Index of Serial string
	1, //bNumConfigurations - Max number of configurations (if more then 1, you can switch between them)
};

// One configuration, containing a control interface and a serial-data
// interface. A USB interface groups related endpoints/functions.
static const uint8_t config_descriptor[ ] =
{
  0x09,        // bLength
  0x02,        // bDescriptorType (Configuration)
  0x43, 0x00,  // wTotalLength 67
  0x02,        // bNumInterfaces 2
  0x01,        // bConfigurationValue
  0x00,        // iConfiguration (String Index)
  0x80,        // bmAttributes - bus powered, no remote wake advertised.
  0x32,        // bMaxPower 100mA

  0x09,        // bLength
  0x04,        // bDescriptorType - Interface
  0x00,        // bInterfaceNumber - 0
  0x00,        // bAlternateSetting
  0x01,        // bNumEndpoints - 1
  0x02,        // bInterfaceClass - CDC
  0x02,        // bInterfaceSubClass - Abstract Control Model (Table 4 in CDC120.pdf)
  0x01,        // bInterfaceProtocol - AT Commands: V.250 etc (Table 5)
  0x00,        // iInterface (String Index)

  // CDC control information. "CS_INTERFACE" here means Class-Specific
  // Interface descriptor; this CS is unrelated to SPI's chip-select wire.
  0x05,        // bLength
  0x24,        // bDescriptorType - CS_INTERFACE (Table 12)
  0x00,        // bDescriptorSubType - Header Functional Descriptor (Table 13)
  0x10, 0x01,  // bcdCDC - CDC class specification version 1.10.
  // Call Management Functional Descriptor
  0x05,        // bLength
  0x24,        // bDescriptorType - CS_INTERFACE
  0x01,        // bDescriptorSubType - Call Management Functional Descriptor (Table 13)
  0x00,        // bmCapabilities: (Table 3 in PSTN120.pdf)
  // Bit 0 — Device handles call management itself:
  //  1 = device handles call management (e.g. call setup, termination, etc.)
  //  0 = host handles it
  // Bit 1 — Device can send/receive call management information over a Data Class interface:
  //  1 = can use the Data Class interface for call management
  //  0 = must use the Communication Class interface
  0x01,        // bDataInterface - Indicates that multiplexed commands are handled via data interface 01h (same value as used in the UNION Functional Descriptor)
  // Abstract Control Management Functional Descriptor
  0x04,        // bLength
  0x24,        // bDescriptorType - CS_INTERFACE
  0x02,        // bDescriptorSubType - Abstract Control Management Functional Descriptor (Table 13)
  0x02,        // bmCapabilities - Device supports the request combination of Set_Line_Coding, Set_Control_Line_State, Get_Line_Coding, and the notification Serial_State (Table 4 in PSTN120.pdf)
  // Union Descriptor Functional Descriptor
  0x05,        // bLength
  0x24,        // bDescriptorType - CS_INTERFACE
  0x06,        // bDescriptorSubType - Union Descriptor Functional Descriptor (Table 13)
  0x00,        // bControlInterface (Interface number of the control (Communications Class) interface)
  0x01,        // bSubordinateInterface0 (Interface number of the subordinate (Data Class) interface)
  // EP1: serial-status notifications sent to the computer. USB calls this an
  // "Interrupt" endpoint: it is polled by the host; it is not a CPU interrupt.
  0x07,        // bLength
  0x05,        // bDescriptorType (Endpoint)
  0x81,        // bEndpointAddress (IN/D2H)
  0x03,        // bmAttributes (Interrupt)
  0x40, 0x00,  // wMaxPacketSize 64
  0x01,        // bInterval 1 (unit depends on device speed)

  // Serial-data interface. Bulk endpoints carry ordinary byte streams.
  0x09,        // bLength
  0x04,        // bDescriptorType (Interface)
  0x01,        // bInterfaceNumber 1
  0x00,        // bAlternateSetting
  0x02,        // bNumEndpoints 2
  0x0A,        // bInterfaceClass
  0x00,        // bInterfaceSubClass
  0x00,        // bInterfaceProtocol - Transparent
  0x00,        // iInterface (String Index)
  // EP2 - computer to board (OUT), used for command characters.
  0x07,        // bLength
  0x05,        // bDescriptorType (Endpoint)
  0x02,        // bEndpointAddress (OUT/H2D)
  0x02,        // bmAttributes (Bulk)
  0x40, 0x00,  // wMaxPacketSize 64
  0x00,        // bInterval 0 (unit depends on device speed)
  // EP3 - board to computer (IN), used for status replies.
  0x07,        // bLength
  0x05,        // bDescriptorType (Endpoint)
  0x83,        // bEndpointAddress (IN/D2H)
  0x02,        // bmAttributes (Bulk)
  0x40, 0x00,  // wMaxPacketSize 64
  0x00,        // bInterval 0 (unit depends on device speed)

  // 67 bytes
};

// USB strings use 16-bit characters. The first two bytes give total length
// and descriptor type; the rest is text (or a language code for string zero).
struct usb_string_descriptor_struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t wString[];
};
const static struct usb_string_descriptor_struct language __attribute__((section(".rodata"))) = {
	4,
	3,
	{0x0409}  // Language ID - English US (look in USB_LANGIDs)
};
const static struct usb_string_descriptor_struct string1 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_MANUFACTURER),
	3,  // bDescriptorType - String Descriptor (0x03)
	FUSB_STR_MANUFACTURER
};
const static struct usb_string_descriptor_struct string2 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_PRODUCT),
	3,
	FUSB_STR_PRODUCT
};
const static struct usb_string_descriptor_struct string3 __attribute__((section(".rodata")))  = {
	sizeof(FUSB_STR_SERIAL),
	3,
	FUSB_STR_SERIAL
};

// Lookup table used by the library when the computer requests a descriptor.
// Its key packs two USB request fields: (wIndex << 16) | wValue. wValue's
// high byte is descriptor type; its low byte is descriptor/string number.
// wIndex supplies the string language (0x0409 = US English), or zero.
const static struct descriptor_list_struct {
	uint32_t	lIndexValue;  // Packed request key; name required by the USB library.
	const uint8_t	*addr;
	uint8_t		length;
} descriptor_list[] = {
	{0x00000100, device_descriptor, sizeof(device_descriptor)},
	{0x00000200, config_descriptor, sizeof(config_descriptor)},
	{0x00000300, (const uint8_t *)&language, 4},
	{0x04090301, (const uint8_t *)&string1, string1.bLength},
	{0x04090302, (const uint8_t *)&string2, string2.bLength},
	{0x04090303, (const uint8_t *)&string3, string3.bLength}
};
#define DESCRIPTOR_LIST_ENTRIES ((sizeof(descriptor_list))/(sizeof(struct descriptor_list_struct)) )


#endif

