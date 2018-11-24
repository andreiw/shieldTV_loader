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

#include <lib.h>
#include <usbd.h>

#define EHCI_BASE UN(0x7d000000)

#define QH_OFFSET_OUT(n) (EHCI_BASE + 0x1000 + (UN(n) * 0x80))
#define QH_OFFSET_IN(n)  (EHCI_BASE + 0x1000 + (UN(n) * 0x80) + 0x40)
#define USBMODE     (EHCI_BASE + 0x1f8)
#define USBLISTADR  (EHCI_BASE + 0x148)
#define USBCMD      (EHCI_BASE + 0x130)
#define USBSTS      (EHCI_BASE + 0x134)
#define USBINTR     (EHCI_BASE + 0x138)
#define USBDEVADDR  (EHCI_BASE + 0x144)
#define USBDEVLC    (EHCI_BASE + 0x1b4)
#define EPTSETUPST  (EHCI_BASE + 0x208)
#define EPTPRIME    (EHCI_BASE + 0x20c)
#define EPTFLUSH    (EHCI_BASE + 0x210)
#define EPTREADY    (EHCI_BASE + 0x214)
#define EPTCOMPLETE (EHCI_BASE + 0x218)
#define EP_CTRL(n)  (EHCI_BASE + 0x21c + (UN(n) * 4))

#define USBCMD_ITC_DEFAULT (8 << 16)
#define USBCMD_SETUP_TRIPW BIT(13)
#define USBCMD_RESET       BIT(1)
#define USBCMD_RUN         BIT(0)
#define USBSTS_SLI         BIT(8)
#define USBSTS_RESET       BIT(6)
#define USBSTS_PORT_CHANGE BIT(2)
#define USBSTS_ERROR       BIT(1)
#define USBDEVLC_MODE(x)   X(x, 25, 26)
#define USBDEVLC_MODE_FULL (0)
#define USBDEVLC_MODE_LOW  (1)
#define USBDEVLC_MODE_HIGH (2)
#define USBMODE_DEVICE     (2)
#define USBMODE_MASK       (3)
#define USBDEVADDR_ADVANCE BIT(24)
#define USBDEVADDR_SHIFT   (25)
#define EP_CTRL_RXS        BIT(0)
#define EP_CTRL_RXR        BIT(6)
#define EP_CTRL_RXE        BIT(7)
#define EP_CTRL_TXS        BIT(16)
#define EP_CTRL_TXR        BIT(22)
#define EP_CTRL_TXE        BIT(23)
#define MAX_REQS 16
static usbd_req *reqs[MAX_REQS * 2];


static void
flush(usbd *context, int ep, bool_t in)
{
	uint32_t bits = 0;

	if (ep == -1) {
		bits = 0xffffffff;
	} else if (ep == 0) {
		bits = BIT(16) | BIT(0);
	} else if (in) {
		bits = BIT(16 + ep);
	} else {
		bits = BIT(ep);
	}

	do {
		OUT32(bits, EPTFLUSH);
	} while (IN32(EPTREADY) & bits);
}

usbd_status
usbd_init(usbd *context)
{
	if (UN(&context->ep0_out_qtd) & (DTD_ALIGNMENT - 1)) {
		return USBD_BAD_ALIGNMENT;
	}

	if (UN(&context->ep0_in_qtd) & (DTD_ALIGNMENT - 1)) {
		return USBD_BAD_ALIGNMENT;
	}

	memset(&context->ep0_out, 0, sizeof(context->ep0_out));
	context->ep0_out.qtd = &context->ep0_out_qtd;
	context->ep0_out.ep = 0;
	context->ep0_out.send = false;
	context->ep0_out.ctx = context->ctx;
	context->ep0_out.buffer = context->ep0_out.small_buffer;

	memset(&context->ep0_in, 0, sizeof(context->ep0_in));
	context->ep0_in.qtd = &context->ep0_in_qtd;
	context->ep0_in.ep = 0;
	context->ep0_in.send = true;
	context->ep0_in.ctx = context->ctx;
	context->ep0_in.buffer = context->ep0_in.small_buffer;

	context->hs = false;
	context->descs = NULL;
	context->current_config = 0;

	OUT32(USBCMD_ITC_DEFAULT | USBCMD_RESET, USBCMD);
	while((IN32(USBCMD) & USBCMD_RESET) != 0);

	OUT32(USBMODE_DEVICE, USBMODE);
	while((IN32(USBMODE) & USBMODE_MASK) != USBMODE_DEVICE);

	flush(context, -1, false);

	OUT32(QH_OFFSET_OUT(0), USBLISTADR);
	OUT32(USBCMD_ITC_DEFAULT | USBCMD_RUN, USBCMD);
	return USBD_SUCCESS;
}

int
usbd_ep_init(usbd *context, int ep,
	     usbd_ep_type_t rx_type, usbd_ep_type_t tx_type)
{
	uint32_t bits = 0;

	if (ep == 0) {
		/* Always on and CTLR. */
		return 0;
	}

	if (rx_type != EP_TYPE_NONE) {
		rx_type--;
		bits |= I(rx_type, 3, 2) | EP_CTRL_RXE | EP_CTRL_RXR;
	}

	if (tx_type != EP_TYPE_NONE) {
		tx_type--;
		bits |= I(tx_type, 19, 18) | EP_CTRL_TXE | EP_CTRL_TXR;
	}

	OUT32(bits, EP_CTRL(ep));
	return 0;
}

static void
usbd_ep_stall(usbd *context, int ep)
{
	OUT32(EP_CTRL_RXS | EP_CTRL_TXS, EP_CTRL(ep));
}

static usbd_status
usbd_port_change(usbd *context)
{
	int mode;

	mode = USBDEVLC_MODE(IN32(USBDEVLC));
	switch (mode) {
	case USBDEVLC_MODE_FULL:
		context->hs = false;
		context->descs = context->fs_descs;
		break;
	case USBDEVLC_MODE_HIGH:
		context->hs = true;
		context->descs = context->hs_descs;
		break;
	default:
		return USBD_PORT_CHANGE_ERROR;
	}

	return USBD_SUCCESS;
}

static usbd_status
usbd_port_reset(usbd *context)
{
	int i;

	OUT32(IN32(EPTCOMPLETE), EPTCOMPLETE);
	OUT32(IN32(EPTSETUPST), EPTSETUPST);
	flush(context, -1, false);

	for (i = 0; i < ELES(reqs); i++) {
		usbd_req *req = reqs[i];

		if (req != NULL) {
			reqs[i] = NULL;
			req->error = true;
			if (req->complete != NULL) {
				req->complete(context, req);
			}
		}
	}

	for (i = 0; i < MAX_REQS; i++) {
		usbd_ep_init(context, i, EP_TYPE_NONE,
			     EP_TYPE_NONE);
	}

	if (context->port_reset == NULL) {
		return USBD_SUCCESS;
	}
	return context->port_reset(context);
}

static void
init_qh(int ep,
	ep_queue_head *qh,
	ep_td_struct *td,
	uint32_t packet_len,
	bool_t send)
{
	memset(qh, 0, sizeof(*qh));
	qh->max_pkt_length =
		(packet_len << EP_QUEUE_HEAD_MAX_PKT_LEN_POS) |
		EP_QUEUE_HEAD_ZLT_SEL;

	if (!send && ep == 0) {
		qh->max_pkt_length |= EP_QUEUE_HEAD_IOS;
	}

	qh->curr_dtd_ptr = DTD_ADDR_MASK | DTD_NEXT_TERMINATE;
	qh->next_dtd_ptr = (uint32_t) (uintptr_t) td;
}

static void
init_td(ep_td_struct *td,
	uint32_t size,
	void *buf)
{
	memset(td, 0, sizeof(*td));
	td->next_td_ptr = DTD_ADDR_MASK | DTD_NEXT_TERMINATE;
	td->size_ioc_sts = (size << 16) | DTD_STATUS_ACTIVE;
	td->buff_ptr0 = (uint32_t) UN(buf);
	if (size <= 0x1000) {
		return;
	}
	td->buff_ptr1 = (td->buff_ptr0 & 0xfffff000) + 0x1000;
	if (size <= 0x2000) {
		return;
	}
	td->buff_ptr2 = td->buff_ptr1 + 0x1000;
	if (size <= 0x3000) {
		return;
	}
	td->buff_ptr3 = td->buff_ptr2 + 0x1000;
	if (size <= 0x4000) {
		return;
	}
	td->buff_ptr4 = td->buff_ptr3 + 0x1000;
}

static void
usbd_ep_prime(usbd *context, int ep, bool_t in)
{
	uint32_t bits = 0;

	if (in) {
		bits = BIT(16 + ep);
	} else {
		bits = BIT(ep);
	}

	OUT32(bits, EPTPRIME);
}

static void
usbd_req_complete(usbd *context, usbd_req *ep)
{
	if (ep->complete == NULL) {
		return;
	}

	ep->io_done = ep->buffer_length -
		((ep->qtd->size_ioc_sts & DTD_PACKET_SIZE) >>
		 DTD_LENGTH_BIT_POS);
	ep->error = (ep->qtd->size_ioc_sts & DTD_ERROR_MASK) != 0;
	ep->complete(context, ep);
}

static usbd_ep_type_t
usbd_get_ep_type(usbd *context, int ep, bool_t send)
{
	uint32_t bits;

	if (ep == 0) {
		return EP_TYPE_CTLR;
	}

	bits = IN32(EP_CTRL(ep));
	if (send) {
		return X(bits, 19, 18) + 1;
	}

	return X(bits, 3, 2) + 1;
}

static uint32_t
usbd_get_ep_max_packet(usbd *context, int ep, bool_t send)
{
	usbd_ep_type_t type = usbd_get_ep_type(context, ep, send);

	BUG_ON (type == EP_TYPE_NONE);

	if (type == EP_TYPE_CTLR) {
		return USBD_CONTROL_MAX;
	} else if (type == EP_TYPE_BULK) {
		return context->hs ? USBD_HS_BULK_MAX : USBD_FS_BULK_MAX;
	} else if (type == EP_TYPE_INTR) {
		return context->hs ? USBD_HS_INTR_MAX : USBD_FS_INTR_MAX;
	}

	return context->hs ? USBD_HS_ISO_MAX : USBD_FS_ISO_MAX;
}

usbd_status
usbd_req_submit(usbd *context, usbd_req *req)
{
	int ix = req->ep;
	req->buffer_length =  min(req->buffer_length,
				  (uint32_t) 0x5000);
	ep_queue_head *qh;

	if (req->send) {
		qh = (ep_queue_head *) QH_OFFSET_IN(req->ep);
		ix += MAX_REQS;
	} else {
		qh = (ep_queue_head *) QH_OFFSET_OUT(req->ep);
	}

	BUG_ON(reqs[ix] != NULL);

	flush(context, req->ep, req->send);

	if (req->buffer_length != 0) {
		if (req->send) {
			DSB_ST();
		} else {
			DSB_LD();
		}
	}

	init_td(req->qtd, req->buffer_length, req->buffer);
	DSB_ST();

	init_qh(req->ep, qh, req->qtd,
		usbd_get_ep_max_packet(context,
				       req->ep, req->send),
		req->send);
	DSB_ST();

	usbd_ep_prime(context, req->ep, req->send);
	reqs[ix] = req;

	return USBD_SUCCESS;
}

static void
usbd_ep0_setup_ack(usbd *context)
{
	context->ep0_in.buffer_length = 0;
	context->ep0_in.complete = NULL;

	usbd_req_submit(context, &context->ep0_in);
}

static void
usbd_ep0_in_complete(usbd *context, usbd_req *req)
{
	if (req->error) {
		return;
	}

	context->ep0_out.buffer_length = 0;
	context->ep0_out.complete = NULL;
	usbd_req_submit(context, &context->ep0_out);
}

static void
usbd_ep0_setup_tx(usbd *context, void *buf, uint32_t buffer_length)
{
	context->ep0_in.buffer_length = buffer_length;
	memcpy(context->ep0_in.buffer, buf, buffer_length);
	context->ep0_in.complete = usbd_ep0_in_complete;;

	usbd_req_submit(context, &context->ep0_in);
}

static usbd_status
usbd_ep0_setup(usbd *context, usb_ctrlrequest *request)
{
	usbd_desc_table *d;

	printk("saw bRT %u bR %u wV 0x%x wI 0x%x wL 0x%x\n",
	       request->bRequestType,
	       request->bRequest,
	       request->wValue,
	       request->wIndex,
	       request->wLength);

#define ST(bRT, bR) (((bRT) << 8) | (bR))
	switch (ST(request->bRequestType,
		   request->bRequest)) {
	case ST(USB_REQ_DEV_W, USB_REQ_SET_ADDRESS):
		OUT32(USBDEVADDR_ADVANCE |
		      (request->wValue << USBDEVADDR_SHIFT),
		      USBDEVADDR);
		usbd_ep0_setup_ack(context);
		return USBD_SUCCESS;
	case ST(USB_REQ_DEV_R, USB_REQ_GET_DESCRIPTOR):
		if (context->descs == NULL) {
			break;
		}
		d = context->descs;

		while (d->data) {
			if (request->wValue != d->id) {
				d++;
				continue;
			}

			usbd_ep0_setup_tx(context, d->data,
					  min(d->length, request->wLength));
			return USBD_SUCCESS;
		}

		break;
	case ST(USB_REQ_DEV_W, USB_REQ_SET_CONFIGURATION):
		if (request->wValue != 0) {
			context->current_config = request->wValue;
			usbd_ep0_setup_ack(context);
			return USBD_SUCCESS;
		}
		break;
	}
#undef ST

	return USBD_SETUP_PACKET_UNSUPPORTED;
}

static usbd_status
usbd_port_setup(usbd *context, uint32_t setupst)
{
	int ep_ix;
	ep_queue_head *qh;
	usbd_status setup_status;

	for (ep_ix = 0; setupst; setupst >>= 1, ep_ix++) {
		usb_ctrlrequest request;

		if ((setupst & 1) != 1) {
			continue;
		}

		qh = (ep_queue_head *) QH_OFFSET_OUT(ep_ix);
			memcpy(&request, qh->setup_buffer,
			       sizeof(usb_ctrlrequest));

		OUT32(BIT(ep_ix), EPTSETUPST);
		while ((IN32(EPTSETUPST) & BIT(ep_ix)) != 0);

		setup_status = USBD_SETUP_PACKET_UNSUPPORTED;
		if (ep_ix == 0) {
			setup_status = usbd_ep0_setup(context, &request);
		} else if (context->port_setup != 0) {
			setup_status = context->port_setup(context,
							   ep_ix, &request);
		}

		if (setup_status != USBD_SUCCESS) {
			printk("Stalling\n");
			usbd_ep_stall(context, ep_ix);
		}
	}

	return USBD_SUCCESS;
}

static void
usbd_completions(usbd *context, uint32_t complete)
{
	int ep_ix;

	for (ep_ix = 0; complete; complete >>= 1, ep_ix++) {
		usbd_req *ep = reqs[ep_ix];

		if ((complete & 1) == 0) {
			continue;
		}

		reqs[ep_ix] = NULL;
		DSB_LD();
		usbd_req_complete(context, ep);
	}
}

usbd_status
usbd_poll(usbd *context)
{
	uint32_t status;
	uint32_t setupst;
	uint32_t complete;

	status = IN32(USBSTS);
	if (status != 0) {
		OUT32(status, USBSTS);

		if ((status & USBSTS_SLI) != 0) {
			return usbd_init(context);
		} else if ((status & USBSTS_RESET) != 0) {
			return usbd_port_reset(context);
		} else if ((status && USBSTS_PORT_CHANGE) != 0) {

			return usbd_port_change(context);
		} else {
			printk("USB error 0x%x\n", status);
			return USBD_USBSTS_ERROR;
		}
	}

	setupst = IN32(EPTSETUPST);
	if (setupst != 0) {
		return usbd_port_setup(context, setupst);
	}

	complete = IN32(EPTCOMPLETE);
	if (complete != 0) {
		OUT32(complete, EPTCOMPLETE);
		usbd_completions(context, complete);
	}

	return USBD_SUCCESS;
}
