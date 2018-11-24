/*
 * Copyright (C) 2018 Andrei Warkentin <andrey.warkentin@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <lib.h>
#include <usbd.h>

#ifndef USB_DESCRIPTOR_H
#define USB_DESCRIPTOR_H

static unsigned short manufacturer_string[] = {
	(USB_DESC_TYPE_STRING << 8) | (9 * 2),
	's', 'h', 'i', 'e', 'l', 'd', 'T', 'V',
};

static unsigned short product_string[] = {
	(USB_DESC_TYPE_STRING << 8) | (20 * 2),
	's', 'h', 'i', 'e', 'l', 'd', 'T', 'V', ' ', 'l', 'o', 'a', 'd', 'e', 'r', ' ', '1', '.', '0',
};

static unsigned short default_string[] = {
	(USB_DESC_TYPE_STRING << 8) | (8 * 2),
	'd', 'e', 'f', 'a', 'u', 'l', 't',
};

static unsigned short language_table[] = {
	(USB_DESC_TYPE_STRING << 8) | 4,
	0x0409, // LANGID for US English
};

static unsigned char device_desc[] = {
	18,              // length
	USB_DESC_TYPE_DEVICE,     // type
	0x10, 0x02,      // usb spec rev 1.00
	0x00,            // class
	0x00,            // subclass
	0x00,            // protocol
	0x40,            // max packet size
	0xD1, 0x18,      // vendor id
	0x0D, 0xD0,      // product id
	0x00, 0x01,      // version 1.0
	0x01,            // manufacturer str idx
	0x02,            // product str idx
	0x00,            // serial number index
	0x01,            // number of configs,
};

static unsigned char config_desc[] = {
	0x09,            // length
	USB_DESC_TYPE_CONFIGURATION,
	0x20, 0x00,      // total length
	0x01,            // # interfaces
	0x01,            // config value
	0x00,            // config string
	0x80,            // attributes
	0x80,            // XXX max power (250ma)

	0x09,            // length
	USB_DESC_TYPE_INTERFACE,
	0x00,            // interface number
	0x00,            // alt number
	0x02,            // # endpoints
	0xFF,
	0x42,
	0x03,
	0x00,            // interface string

	0x07,            // length
	USB_DESC_TYPE_ENDPOINT,
	0x81,            // in, #1
	0x02,            // bulk
	0x00, 0x02,      // max packet 512
	0x00,            // interval

	0x07,            // length
	USB_DESC_TYPE_ENDPOINT,
	0x01,            // out, #1
	0x02,            // bulk
	0x00, 0x02,      // max packet 512
	0x01,            // interval
};

static unsigned char config_desc_fs[] = {
	0x09,            // length
	USB_DESC_TYPE_CONFIGURATION,
	0x20, 0x00,      // total length
	0x01,            // # interfaces
	0x01,            // config value
	0x00,            // config string
	0x80,            // attributes
	0x80,            // XXX max power (250ma)

	0x09,            // length
	USB_DESC_TYPE_INTERFACE,
	0x00,            // interface number
	0x00,            // alt number
	0x02,            // # endpoints
	0xFF,
	0x42,
	0x03,
	0x00,            // interface string

	0x07,            // length
	USB_DESC_TYPE_ENDPOINT,
	0x81,            // in, #1
	0x02,            // bulk
	0x40, 0x00,      // max packet 64
	0x00,            // interval

	0x07,            // length
	USB_DESC_TYPE_ENDPOINT,
	0x01,            // out, #1
	0x02,            // bulk
	0x40, 0x00,      // max packet 64
	0x00,            // interval
};

static usbd_desc_table descr_hs[] = {
	{ device_desc, sizeof(device_desc), USB_DESC_ID(USB_DESC_TYPE_DEVICE, 0) },
	{ config_desc, sizeof(config_desc), USB_DESC_ID(USB_DESC_TYPE_CONFIGURATION, 0) },
	{ manufacturer_string, sizeof(manufacturer_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 1) },
	{ product_string, sizeof(product_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 2) },
	{ default_string, sizeof(default_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 4) },
	{ language_table, sizeof(language_table), USB_DESC_ID(USB_DESC_TYPE_STRING, 0) },
	{ 0, 0, 0 },
};

static usbd_desc_table descr_fs[] = {
	{ device_desc, sizeof(device_desc), USB_DESC_ID(USB_DESC_TYPE_DEVICE, 0) },
	{ config_desc_fs, sizeof(config_desc), USB_DESC_ID(USB_DESC_TYPE_CONFIGURATION, 0) },
	{ manufacturer_string, sizeof(manufacturer_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 1) },
	{ product_string, sizeof(product_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 2) },
	{ default_string, sizeof(default_string), USB_DESC_ID(USB_DESC_TYPE_STRING, 4) },
	{ language_table, sizeof(language_table), USB_DESC_ID(USB_DESC_TYPE_STRING, 0) },
	{ 0, 0, 0 },
};

#endif /* USB_DESCRIPTOR_H */
