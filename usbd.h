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
 * Copyright (c) 2008, Google Inc.
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

#ifndef USBD_H
#define USBD_H

#include <defs.h>

/*
 * Endpoint Transfer Descriptor is a 28 byte structure, aligned on a 32-byte
 * boundary.
 *
 */
typedef struct usbd_td {
#define USBD_TD_NEXT_TERMINATE                   0x00000001
	uint32_t next_td_ptr;	/* Next TD pointer(31-5), T(0) set
				   indicate invalid */
#define USBD_TD_IOC                              0x00008000
#define USBD_TD_STATUS_ACTIVE                    0x00000080
#define USBD_TD_STATUS_HALTED                    0x00000040
#define USBD_TD_STATUS_DATA_BUFF_ERR             0x00000020
#define USBD_TD_STATUS_TRANSACTION_ERR           0x00000008
#define USBD_TD_RESERVED_FIELDS                  0x80007300
	uint32_t size_ioc_sts;	/* Total bytes (30-16), IOC (15),
				   MultO(11-10), STS (7-0)	*/
	uint32_t buff_ptr0;		/* Buffer pointer Page 0 */
	uint32_t buff_ptr1;		/* Buffer pointer Page 1 */
	uint32_t buff_ptr2;		/* Buffer pointer Page 2 */
	uint32_t buff_ptr3;		/* Buffer pointer Page 3 */
	uint32_t buff_ptr4;		/* Buffer pointer Page 4 */
	uint32_t dummy_for_aligned_qtd_packing;
} __packed usbd_td;

#define USBD_TD_ADDR_MASK                        0xFFFFFFE0
#define USBD_TD_PACKET_SIZE                      0x7FFF0000
#define USBD_TD_LENGTH_BIT_POS                   16
#define USBD_TD_ERROR_MASK                       (USBD_TD_STATUS_HALTED | \
					      USBD_TD_STATUS_DATA_BUFF_ERR | \
					      USBD_TD_STATUS_TRANSACTION_ERR)
#define USBD_TD_ALIGNMENT			      0x20

/*
 * Endpoint Queue Head.
 */
typedef struct usbd_qh {
	uint32_t max_pkt_length;	/* Mult(31-30), Zlt(29), Max Pkt len and IOS(15) */
	uint32_t curr_dtd_ptr;		/* Current dTD Pointer(31-5) */
	uint32_t next_dtd_ptr;		/* Next dTD Pointer(31-5), T(0) */
	uint32_t size_ioc_int_sts;	/* Total bytes (30-16), IOC (15),
					    MultO(11-10), STS (7-0)      */
	uint32_t buff_ptr0;		/* Buffer pointer Page 0 (31-12) */
	uint32_t buff_ptr1;		/* Buffer pointer Page 1 (31-12) */
	uint32_t buff_ptr2;		/* Buffer pointer Page 2 (31-12) */
	uint32_t buff_ptr3;		/* Buffer pointer Page 3 (31-12) */
	uint32_t buff_ptr4;		/* Buffer pointer Page 4 (31-12) */
	uint32_t res1;
	uint8_t setup_buffer[8]; /* Setup data 8 bytes */
	uint32_t res2[4];
} __packed usbd_qh;

#define USBD_QH_MULT_POS               30
#define USBD_QH_ZLT_SEL                0x20000000
#define USBD_QH_MAX_PKT_LEN_POS        16
#define USBD_QH_MAX_PKT_LEN(ep_info)   (((ep_info)>>16)&0x07ff)
#define USBD_QH_IOS                    0x00008000
#define USBD_QH_NEXT_TERMINATE         0x00000001
#define USBD_QH_IOC                    0x00008000
#define USBD_QH_MULTO                  0x00000C00
#define USBD_QH_STATUS_HALT            0x00000040
#define USBD_QH_STATUS_ACTIVE          0x00000080
#define EP_QUEUE_CURRENT_OFFSET_MASK   0x00000FFF
#define USBD_QH_NEXT_POINTER_MASK      0xFFFFFFE0
#define EP_QUEUE_FRINDEX_MASK          0x000007FF
#define EP_MAX_LENGTH_TRANSFER         0x4000

#define USBD_CONTROL_MAX 64
#define USBD_FS_BULK_MAX 64
#define USBD_HS_BULK_MAX 512
#define USBD_FS_INTR_MAX 64
#define USBD_HS_INTR_MAX 1024
#define USBD_FS_ISO_MAX 1023
#define USBD_HS_ISO_MAX 1024

struct usbd;
struct usbd_ep;

typedef struct usbd_req {
	struct usbd_ep *ep;
	bool_t error;
	uint32_t buffer_length;
	void *buffer;
	uint8_t small_buffer[USBD_CONTROL_MAX];
	uint32_t io_done;
	void (*complete)(struct usbd *context, struct usbd_req *req);
} usbd_req;

typedef struct usb_ctrlrequest {
#define USB_REQ_DEV_R	0x80
#define USB_REQ_DEV_W	0x00
#define USB_REQ_IFACE_R	0x81
#define USB_REQ_IFACE_W	0x01
#define USB_REQ_EP_R	0x82
#define USB_REQ_EP_W	0x02
	uint8_t bRequestType;
#define USB_REQ_SET_ADDRESS       0x5
#define USB_REQ_GET_DESCRIPTOR    0x6
#define USB_REQ_GET_CONFIGURATION 0x8
#define USB_REQ_SET_CONFIGURATION 0x9
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __packed usb_ctrlrequest;

#define USBD_ALIGNMENT USBD_TD_ALIGNMENT

typedef enum {
	USBD_SUCCESS,
	USBD_BAD_ALIGNMENT,
	USBD_PORT_CHANGE_ERROR,
	USBD_USBSTS_ERROR,
	USBD_EP_UNCONFIGURED,
	USBD_SETUP_PACKET_UNSUPPORTED,
	USBD_CONFIG_UNSUPPORTED,
} usbd_status;

typedef struct  {
	void *data;
	uint16_t length;
	uint16_t id;
} usbd_desc_table;

#define USB_DESC_ID(type,num) ((type << 8) | num)

#define USB_DESC_TYPE_DEVICE			0x01
#define USB_DESC_TYPE_CONFIGURATION		0x02
#define USB_DESC_TYPE_STRING			0x03
#define USB_DESC_TYPE_INTERFACE			0x04
#define USB_DESC_TYPE_ENDPOINT			0x05
#define USB_DESC_TYPE_DEVICE_QUALIFIER		0x06
#define USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION	0x07
#define USB_DESC_TYPE_INTERFACE_POWER		0x08
#define USB_DESC_TYPE_HID			0x21
#define USB_DESC_TYPE_REPORT			0x22

typedef enum usbd_ep_type {
	EP_TYPE_NONE,
	EP_TYPE_CTLR,
	EP_TYPE_ISO,
	EP_TYPE_BULK,
	EP_TYPE_INTR,
} usbd_ep_type;

typedef struct usbd_ep {
	int num;
	bool_t send;
	usbd_ep_type type;
	usbd_td *qtd;
} usbd_ep;

typedef struct usbd {
	usbd_td *qtds;
	size_t qtd_count;
	usbd_ep ep0_out;
	usbd_ep ep0_in;
	usbd_req ep0_out_req;
	usbd_req ep0_in_req;
	bool_t hs;
	usbd_desc_table *descs;
	uint8_t current_config;
/*
 * User-servicable parts go below.
 */
	void *ctx;
	/*
	 * Everything but EP0.
	 */
	usbd_ep **eps;
	usbd_desc_table *fs_descs;
	usbd_desc_table *hs_descs;
	usbd_status (*port_reset)(struct usbd *);
	usbd_status (*port_setup)(struct usbd *, int ep,
				  usb_ctrlrequest *req);
	usbd_status (*set_config)(struct usbd *, uint8_t config);
} usbd;

usbd_status usbd_init(usbd *context,
		      usbd_td *qtds,
		      size_t qtd_count);
usbd_status usbd_poll(usbd *context);
void usbd_ep_enable(usbd *context, usbd_ep *ep_out, usbd_ep *ep_in);
void usbd_ep_disable(usbd *context, usbd_ep *ep_out, usbd_ep *ep_in);
void usbd_req_init(usbd_req *req, usbd_ep *ep);
usbd_status usbd_req_submit(usbd *context, usbd_req *req);

#endif /* USBD_H */
