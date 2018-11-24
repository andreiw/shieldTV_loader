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
#include <usb_descriptors.h>

static usbd_td fb_ep_qtds[4] __align(USBD_TD_ALIGNMENT);

static usbd_ep fb_ep1_out = {
  .num = 1,
  .send = false,
  .type = EP_TYPE_BULK,
};

static usbd_ep fb_ep1_in = {
  .num = 1,
  .send = true,
  .type = EP_TYPE_BULK,
};

static usbd_ep *fb_eps[] = {
  &fb_ep1_out,
  &fb_ep1_in,
  NULL
};

static usbd_req fb_ep1_out_req;
static usbd_req fb_ep1_in_req;

static void fb_rx_cmd(struct usbd *context);

static void
fb_tx_status_complete(usbd *context,
		      usbd_req *req)
{
	fb_rx_cmd(context);
}

static void
fb_tx_status(struct usbd *context, char *status)
{
	fb_ep1_in_req.buffer_length = strlen(status) + 1;
	memcpy(fb_ep1_in_req.buffer, status, strlen(status) + 1);
	fb_ep1_in_req.complete = fb_tx_status_complete;
	usbd_req_submit(context, &fb_ep1_in_req);
}

static void
fb_rx_cmd_complete(usbd *context,
		   usbd_req *req)
{
	if (!req->error) {
		printk("%s\n", req->buffer);
		fb_tx_status(context, "FAILinvalid command");
	}
}

static void
fb_rx_cmd(struct usbd *context)
{
	fb_ep1_out_req.buffer_length = 64;
	fb_ep1_out_req.complete = fb_rx_cmd_complete;
	usbd_req_submit(context, &fb_ep1_out_req);
}

static usbd_status
fb_set_config(struct usbd *context,
	      uint8_t config)
{
	if (config > 1) {
		return USBD_CONFIG_UNSUPPORTED;
	}

	if (config == 1) {
		usbd_ep_enable(context, &fb_ep1_out, &fb_ep1_in);
		fb_rx_cmd(context);
	} else {
		usbd_ep_disable(context, &fb_ep1_out, &fb_ep1_in);
	}

	return USBD_SUCCESS;
}

static usbd fb_uctx = {
	.eps = fb_eps,
	.fs_descs = descr_fs,
	.hs_descs = descr_hs,
	.set_config = fb_set_config,
};

void
fb_launch(void)
{
	usbd_status usbd_stat;

	usbd_stat = usbd_init(&fb_uctx, fb_ep_qtds, ELES(fb_ep_qtds));
	BUG_ON (usbd_stat != USBD_SUCCESS);

	usbd_req_init(&fb_ep1_out_req, &fb_ep1_out);
	usbd_req_init(&fb_ep1_in_req, &fb_ep1_in);

	while(1) {
		usbd_poll(&fb_uctx);
	}
}
