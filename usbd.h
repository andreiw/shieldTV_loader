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

#ifndef USBD_H
#define USBD_H

#include <defs.h>

/*
 * Endpoint Transfer Descriptor is a 28 byte structure, aligned on a 32-byte
 * boundary.
 *
 */
typedef struct ep_td_struct {
#define DTD_NEXT_TERMINATE                   0x00000001
	uint32_t next_td_ptr;	/* Next TD pointer(31-5), T(0) set
				   indicate invalid */
#define DTD_IOC                              0x00008000
#define DTD_STATUS_ACTIVE                    0x00000080
#define DTD_STATUS_HALTED                    0x00000040
#define DTD_STATUS_DATA_BUFF_ERR             0x00000020
#define DTD_STATUS_TRANSACTION_ERR           0x00000008
#define DTD_RESERVED_FIELDS                  0x80007300
	uint32_t size_ioc_sts;	/* Total bytes (30-16), IOC (15),
				   MultO(11-10), STS (7-0)	*/
	uint32_t buff_ptr0;		/* Buffer pointer Page 0 */
	uint32_t buff_ptr1;		/* Buffer pointer Page 1 */
	uint32_t buff_ptr2;		/* Buffer pointer Page 2 */
	uint32_t buff_ptr3;		/* Buffer pointer Page 3 */
	uint32_t buff_ptr4;		/* Buffer pointer Page 4 */
	uint32_t dummy_for_aligned_qtd_packing;
} __packed ep_td_struct;

#define DTD_ADDR_MASK                        0xFFFFFFE0
#define DTD_PACKET_SIZE                      0x7FFF0000
#define DTD_LENGTH_BIT_POS                   16
#define DTD_ERROR_MASK                       (DTD_STATUS_HALTED | \
					      DTD_STATUS_DATA_BUFF_ERR | \
					      DTD_STATUS_TRANSACTION_ERR)
#define DTD_ALIGNMENT			      0x20

/*
 * Endpoint Queue Head.
 */
typedef struct ep_queue_head {
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
} __packed ep_queue_head;

#define EP_QUEUE_HEAD_MULT_POS               30
#define EP_QUEUE_HEAD_ZLT_SEL                0x20000000
#define EP_QUEUE_HEAD_MAX_PKT_LEN_POS        16
#define EP_QUEUE_HEAD_MAX_PKT_LEN(ep_info)   (((ep_info)>>16)&0x07ff)
#define EP_QUEUE_HEAD_IOS                    0x00008000
#define EP_QUEUE_HEAD_NEXT_TERMINATE         0x00000001
#define EP_QUEUE_HEAD_IOC                    0x00008000
#define EP_QUEUE_HEAD_MULTO                  0x00000C00
#define EP_QUEUE_HEAD_STATUS_HALT            0x00000040
#define EP_QUEUE_HEAD_STATUS_ACTIVE          0x00000080
#define EP_QUEUE_CURRENT_OFFSET_MASK         0x00000FFF
#define EP_QUEUE_HEAD_NEXT_POINTER_MASK      0xFFFFFFE0
#define EP_QUEUE_FRINDEX_MASK                0x000007FF
#define EP_MAX_LENGTH_TRANSFER               0x4000

typedef struct usbd_req {
	ep_td_struct *qtd;
	int ep;
	bool_t send;
	bool_t error;
	uint32_t packet_length;
	uint32_t buffer_length;
	void *buffer;
	uint8_t small_buffer[64];
	uint32_t io_done;
	void *ctx;
	int (*complete)(struct usbd_req *req);
} usbd_req_t;

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
#define USB_REQ_SET_CONFIGURATION 0x9
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __packed usb_ctrlrequest;

#define USBD_ALIGNMENT DTD_ALIGNMENT

typedef enum {
  USBD_SUCCESS,
  USBD_BAD_ALIGNMENT,
  USBD_PORT_CHANGE_ERROR,
  USBD_USBSTS_ERROR,
  USBD_SETUP_PACKET_UNSUPPORTED,
} usbd_status;

typedef struct usbd {
	ep_td_struct ep0_out_qtd;
	ep_td_struct ep0_in_qtd;
	usbd_req_t ep0_out;
	usbd_req_t ep0_in;
	void *ctx;
	int max_control_packet;
	int max_bulk_packet;
	int max_interrupt_packet;
	int max_iso_packet;
	int (*port_reset)(struct usbd *);
	usbd_status (*port_setup)(struct usbd *, int ep,
			   usb_ctrlrequest *req);
} usbd;

typedef enum usbd_ep_type {
	EP_TYPE_NONE,
	EP_TYPE_CTLR,
	EP_TYPE_ISO,
	EP_TYPE_BULK,
	EP_TYPE_INTR,
} usbd_ep_type_t;

usbd_status usbd_init(usbd *context);
usbd_status usbd_poll(usbd *context);

#endif /* USBD_H */
