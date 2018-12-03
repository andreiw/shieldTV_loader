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

#define MAX_DMA_ADDR 0xffffffff
#define EHCI_BASE    (context->ehci_udc_base)

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

#define MAX_EPS  16
#define MAX_REQS (MAX_EPS * 2)
static usbd_req *usbd_reqs[MAX_REQS];

static const char * const usbd_ep_type_names[] = {
	"EP_TYPE_NONE",
	"EP_TYPE_CTLR",
	"EP_TYPE_ISO",
	"EP_TYPE_BULK",
	"EP_TYPE_INTR"
};

static void
usbd_hw_ep_flush(usbd *context,
		 int ep,
		 bool_t in)
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

void
usbd_req_init(usbd_req *req,
	      usbd_ep *ep)
{
	memset(req, 0, sizeof(*req));
	req->ep = ep;

	req->buffer = req->small_buffer;
	BUG_ON (UN(req->buffer) > MAX_DMA_ADDR);
}

static usbd_ep *
usbd_get_ep(usbd *context,
	    int ep,
	    bool_t send)
{
	unsigned i;
	struct usbd_ep **pe = context->eps;

	if (ep == 0) {
		if (send) {
			return &context->ep0_in;
		}

		return &context->ep0_out;
	}

	if (pe == NULL) {
		return NULL;
	}

	for (i = 0; pe[i] != NULL; i++) {
		if (pe[i]->num != ep ||
		    pe[i]->send != send) {
			continue;
		}
		return pe[i];
	}

	return NULL;
}

usbd_status
usbd_init(usbd *context,
	  usbd_td *qtds,
	  size_t qtd_count)

{
	size_t i;

	for (i = 0; i < qtd_count; i++) {
		BUG_ON ((UN(qtds + i) & (USBD_TD_ALIGNMENT - 1)) != 0);
		BUG_ON (UN(qtds + i) > MAX_DMA_ADDR);
	}

	context->qtds = qtds;
	context->qtd_count = qtd_count;

	context->ep0_out.num = 0;
	context->ep0_out.send = false;
	context->ep0_out.type = EP_TYPE_CTLR;

	context->ep0_in.num = 0;
	context->ep0_in.send = true;
	context->ep0_in.type = EP_TYPE_CTLR;

	for (i = 0; i < MAX_REQS; i++) {
		usbd_ep *ep;
		int num = i / 2;
		bool_t in = (i & 1) != 0;

		ep = usbd_get_ep(context, num, in);
		if (ep != NULL) {
			BUG_ON (num != ep->num);
			BUG_ON (ep->send != in);
			BUG_ON (qtd_count == 0);
			ep->qtd = qtds++;
			qtd_count--;
		}
	}

	usbd_req_init(&context->ep0_out_req, &context->ep0_out);
	usbd_req_init(&context->ep0_in_req, &context->ep0_in);

	context->hs = false;
	context->descs = NULL;
	context->current_config = 0;

	OUT32(USBCMD_ITC_DEFAULT | USBCMD_RESET, USBCMD);
	while((IN32(USBCMD) & USBCMD_RESET) != 0);

	OUT32(USBMODE_DEVICE, USBMODE);
	while((IN32(USBMODE) & USBMODE_MASK) != USBMODE_DEVICE);

	usbd_hw_ep_flush(context, -1, false);

	OUT32(QH_OFFSET_OUT(0), USBLISTADR);
	OUT32(USBCMD_ITC_DEFAULT | USBCMD_RUN, USBCMD);
	return USBD_SUCCESS;
}

static void
usbd_hw_ep_init(usbd *context,
	       int ep,
	       usbd_ep_type rx_type,
	       usbd_ep_type tx_type)
{
	uint32_t bits = 0;

	if (ep == 0) {
		/* Always on and CTLR. */
		return;
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
}

static uint32_t
usbd_ep_get_max_packet(usbd *context,
		       usbd_ep *e)
{
	if (e->type == EP_TYPE_CTLR) {
		return USBD_CONTROL_MAX;
	} else if (e->type == EP_TYPE_BULK) {
		return context->hs ? USBD_HS_BULK_MAX : USBD_FS_BULK_MAX;
	} else if (e->type == EP_TYPE_INTR) {
		return context->hs ? USBD_HS_INTR_MAX : USBD_FS_INTR_MAX;
	}

	return context->hs ? USBD_HS_ISO_MAX : USBD_FS_ISO_MAX;
}

void
usbd_ep_enable(usbd *context,
	       usbd_ep *ep_out,
	       usbd_ep *ep_in)
{
	usbd_hw_ep_init(context, ep_out->num, ep_out->type, ep_in->type);
}

void
usbd_ep_disable(usbd *context,
		usbd_ep *ep_out,
		usbd_ep *ep_in)
{
	BUG_ON (ep_out->num != ep_in->num);
	usbd_hw_ep_init(context, ep_out->num, EP_TYPE_NONE, EP_TYPE_NONE);
}

static void
usbd_hw_ep_stall(usbd *context,
		 int ep)
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
	usbd_hw_ep_flush(context, -1, false);

	for (i = 0; i < ELES(usbd_reqs); i++) {
		usbd_req *req = usbd_reqs[i];

		if (req != NULL) {
			usbd_reqs[i] = NULL;
			req->error = true;
			if (req->complete != NULL) {
				req->complete(context, req);
			}
		}
	}

	for (i = 0; i < MAX_EPS; i++) {
		usbd_hw_ep_init(context, i,
				EP_TYPE_NONE,
				EP_TYPE_NONE);
	}

	if (context->port_reset == NULL) {
		return USBD_SUCCESS;
	}
	return context->port_reset(context);
}

static void
usbd_qh_init(usbd *context,
	     usbd_ep *ep,
	     usbd_qh *qh)
{
	uint32_t packet_len = usbd_ep_get_max_packet(context, ep);
	BUG_ON (packet_len == 0);

	memset(qh, 0, sizeof(*qh));
	qh->max_pkt_length =
		(packet_len << USBD_QH_MAX_PKT_LEN_POS) |
		USBD_QH_ZLT_SEL;

	if (!ep->send && ep->num == 0) {
		qh->max_pkt_length |= USBD_QH_IOS;
	}

	qh->curr_dtd_ptr = USBD_TD_ADDR_MASK | USBD_TD_NEXT_TERMINATE;
	qh->next_dtd_ptr = (uint32_t) UN(ep->qtd);
}

static void
usbd_td_init(usbd_td *td,
	     uint32_t size,
	     void *buf)
{
	memset(td, 0, sizeof(*td));
	td->next_td_ptr = USBD_TD_ADDR_MASK | USBD_TD_NEXT_TERMINATE;
	td->size_ioc_sts = (size << 16) | USBD_TD_STATUS_ACTIVE;
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
usbd_ep_prime(usbd *context,
	      usbd_ep *ep)
{
	uint32_t bits = 0;

	if (ep->send) {
		bits = BIT(16 + ep->num);
	} else {
		bits = BIT(ep->num);
	}

	OUT32(bits, EPTPRIME);
}

static void
usbd_req_complete(usbd *context,
		  usbd_req *req)
{
	usbd_ep *ep = req->ep;

	BUG_ON(ep == NULL);

	if (req->complete == NULL) {
		return;
	}

	req->io_done = req->buffer_length -
		((ep->qtd->size_ioc_sts & USBD_TD_PACKET_SIZE) >>
		 USBD_TD_LENGTH_BIT_POS);
	req->error = (ep->qtd->size_ioc_sts & USBD_TD_ERROR_MASK) != 0;
	req->complete(context, req);
}

void
usbd_req_cancel(usbd *context,
                usbd_req *req)
{
	int ix;
	usbd_ep *ep = req->ep;

	BUG_ON(ep == NULL);
	ix = ep->num;

	if (ep->send) {
		ix += MAX_EPS;
	}

	if (usbd_reqs[ix] != req) {
		return;
	}

	usbd_hw_ep_flush(context, ep->num, ep->send);

	usbd_reqs[ix] = NULL;
	req->error = true;
	if (req->complete != NULL) {
		req->complete(context, req);
	}
}

usbd_status
usbd_req_submit(usbd *context,
		usbd_req *req)
{
	int ix;
	usbd_qh *qh;
	usbd_ep *ep = req->ep;

	BUG_ON(ep == NULL);
	ix = ep->num;

	req->buffer_length =  min(req->buffer_length,
				  (uint32_t) 0x5000);

	if (ep->send) {
		qh = (usbd_qh *) QH_OFFSET_IN(ep->num);
		ix += MAX_EPS;
	} else {
		qh = (usbd_qh *) QH_OFFSET_OUT(ep->num);
	}

	BUG_ON(usbd_reqs[ix] != NULL);

	usbd_hw_ep_flush(context, ep->num, ep->send);

	if (req->buffer_length != 0) {
		if (ep->send) {
			DSB_ST();
		} else {
			DSB_LD();
		}
	}

	usbd_td_init(ep->qtd, req->buffer_length, req->buffer);
	DSB_ST();

	usbd_qh_init(context, ep, qh);
	DSB_ST();

	usbd_ep_prime(context, ep);
	usbd_reqs[ix] = req;

	return USBD_SUCCESS;
}

static void
usbd_ep0_setup_ack(usbd *context)
{
	context->ep0_in_req.buffer_length = 0;
	context->ep0_in_req.complete = NULL;

	usbd_req_submit(context, &context->ep0_in_req);
}

static void
usbd_ep0_in_req_complete(usbd *context,
			 usbd_req *req)
{
	if (req->error) {
		return;
	}

	context->ep0_out_req.buffer_length = 0;
	context->ep0_out_req.complete = NULL;
	usbd_req_submit(context, &context->ep0_out_req);
}

static void
usbd_ep0_setup_tx(usbd *context,
		  void *buf,
		  uint32_t buffer_length)
{
	context->ep0_in_req.buffer_length = buffer_length;
	memcpy(context->ep0_in_req.buffer, buf, buffer_length);
	context->ep0_in_req.complete = usbd_ep0_in_req_complete;;

	usbd_req_submit(context, &context->ep0_in_req);
}

static usbd_status
usbd_ep0_setup(usbd *context,
	       usb_ctrlrequest *request)
{
	usbd_desc_table *d;
	usbd_status status = USBD_SETUP_PACKET_UNSUPPORTED;

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
		if (request->wValue != context->current_config) {
			if (context->set_config != NULL) {
				/*
				 * Could return USBD_CONFIG_UNSUPPORTED.
				 */
				status = context->set_config(context,
							     (uint8_t) request->wValue);
				if (status != USBD_SUCCESS) {
					break;
				}
			}
		}

		context->current_config = request->wValue;
		usbd_ep0_setup_ack(context);
		return USBD_SUCCESS;
		break;
	case ST(USB_REQ_DEV_R, USB_REQ_GET_CONFIGURATION):
		usbd_ep0_setup_tx(context, &context->current_config,
				  sizeof(context->current_config));
		return USBD_SUCCESS;
	}
#undef ST

	return status;
}

static usbd_status
usbd_port_setup(usbd *context,
		uint32_t setupst)
{
	int ep_ix;
	usbd_qh *qh;
	usbd_status setup_status;

	for (ep_ix = 0; setupst; setupst >>= 1, ep_ix++) {
		usb_ctrlrequest request;

		if ((setupst & 1) != 1) {
			continue;
		}

		qh = (usbd_qh *) QH_OFFSET_OUT(ep_ix);
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
			printk("Stalling bRT %u bR %u wV 0x%x wI 0x%x wL 0x%x\n",
			       request.bRequestType,
			       request.bRequest,
			       request.wValue,
			       request.wIndex,
			       request.wLength);
			usbd_hw_ep_stall(context, ep_ix);
		}
	}

	return USBD_SUCCESS;
}

static void
usbd_completions(usbd *context,
		 uint32_t complete)
{
	int ep_ix;

	for (ep_ix = 0; complete; complete >>= 1, ep_ix++) {
		usbd_req *ep = usbd_reqs[ep_ix];

		if ((complete & 1) == 0) {
			continue;
		}

		usbd_reqs[ep_ix] = NULL;
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
			return usbd_init(context, context->qtds,
					 context->qtd_count);
		} else if ((status & USBSTS_RESET) != 0) {
			return usbd_port_reset(context);
		} else if ((status && USBSTS_PORT_CHANGE) != 0) {
			return usbd_port_change(context);
		} else {
			printk("USB error 0x%x\n", status);
			return USBD_USBSTS_ERROR;
		}
	}

	complete = IN32(EPTCOMPLETE);
	if (complete != 0) {
		OUT32(complete, EPTCOMPLETE);
		usbd_completions(context, complete);
	}

	setupst = IN32(EPTSETUPST);
	if (setupst != 0) {
		return usbd_port_setup(context, setupst);
	}

	return USBD_SUCCESS;
}
