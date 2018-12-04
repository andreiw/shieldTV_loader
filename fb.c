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
 * Gonna keep this here but I don't really think there's anything
 * here from any Google fastboot implementation, lol:
 *
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
#include <lmb.h>
#include <usb_descriptors.h>
#include <tegra.h>

#define FB_BAD_COMMAND "FAILBad command"
#define FB_UNKNOWN_COMMAND "FAILUnknown command"
#define FB_FAIL_COMMAND "FAILFailed command"
#define FB_OK NULL

typedef char *fb_status;

typedef struct fb_cmd_peek {
	phys_addr_t addr;
	size_t access;
	size_t items;
	bool_t ascii;
	char *buffer;
	size_t buffer_len;
} fb_cmd_peek;

typedef struct fb_mem {
	/*
	 * 2 per EP0, 2 per EP1.
	 */
	usbd_td qtds[4];
	usbd uctx;
	usbd_req ep1_out_req;
	usbd_req ep1_in_req;
	bool_t in_command;
	/*
	 * Command states.
	 */
	union {
		fb_cmd_peek peek;
	};
} fb_mem;

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

static void fb_rx_cmd(struct usbd *context,
		      usbd_req *unused);


static void
fb_end_command_complete(usbd *context,
			usbd_req *req)
{
	fb_mem *fb_ctx = context->ctx;
	fb_ctx->in_command = false;
}

static void
fb_end_command(struct usbd *context, char *status)
{
	fb_mem *fb_ctx = context->ctx;

	if (status == FB_OK) {
		status = "OKAY";
	}

	fb_ctx->ep1_in_req.buffer = fb_ctx->ep1_in_req.small_buffer;
	fb_ctx->ep1_in_req.buffer_length = strlen(status) + 1;
	BUG_ON (fb_ctx->ep1_in_req.buffer_length >
		sizeof(fb_ctx->ep1_in_req.small_buffer));
	memcpy(fb_ctx->ep1_in_req.buffer, status, strlen(status) + 1);
	fb_ctx->ep1_in_req.complete = fb_end_command_complete;
	usbd_req_submit(context, &(fb_ctx->ep1_in_req));
}

static void
fb_end_command_with_info_complete(usbd *context,
				 usbd_req *req)
{
	fb_end_command(context, FB_OK);
}

static void
fb_end_command_with_info(struct usbd *context, char *fmt, ...)
{
	va_list list;
	fb_mem *fb_ctx = context->ctx;

	va_start(list, fmt);
	fb_ctx->ep1_in_req.buffer = fb_ctx->ep1_in_req.small_buffer;

	memcpy(fb_ctx->ep1_in_req.buffer, "INFO", 4);
	fb_ctx->ep1_in_req.buffer_length =
		vscnprintf(fb_ctx->ep1_in_req.buffer + 4,
			   sizeof(fb_ctx->ep1_in_req.small_buffer) - 4,
			   fmt, list) + 4;

	fb_ctx->ep1_in_req.complete = fb_end_command_with_info_complete;
	usbd_req_submit(context, &(fb_ctx->ep1_in_req));

	va_end(list);
}

static void
fb_oem_cmd_peek_exe(usbd *context,
		    usbd_req *req)
{
	fb_mem *fb_ctx = context->ctx;
	fb_cmd_peek *peek = &(fb_ctx->peek);
	char *b = peek->buffer;

	if (req != NULL && req->error) {
		return;
	}

	/*
	 * Buffer starts with INFO, ends with NUL.
	 */
	size_t len_to_fill = peek->buffer_len - 4 - 1;
	/*
	 * Each item is hex followed by white space. If doing
	 * ascii, printable characters are aligned to the right.
	 */
	size_t item_sz = peek->access * 2 + 1;
	size_t items = len_to_fill / item_sz;

	items = min(items, peek->items);
	if (items == 0) {
		fb_end_command(context, FB_OK);
		return;
	}

	memcpy(b, "INFO", 4);
	b += 4;

	peek->items -= items;
	while (items--) {
		uint64_t value;

		if (peek->access == 1) {
			value = IN8(VP(peek->addr));
		} else if (peek->access == 2) {
			value = IN16(VP(peek->addr));
		} else if (peek->access == 4) {
			value = IN32(VP(peek->addr));
		} else {
			value = IN64(VP(peek->addr));
		}
		peek->addr += peek->access;

		if (peek->ascii && isascii(value) && isgraph(value)) {
			b += scnprintf(b, len_to_fill, " %c ", value);
		} else {
			b += scnprintf(b, len_to_fill, "%0*lx ",
				       peek->access * 2, value);
		}
		len_to_fill -= item_sz;
	}

	*b++ = '\0';
	BUG_ON (b - peek->buffer > peek->buffer_len);
	fb_ctx->ep1_in_req.buffer_length = b - peek->buffer;
	fb_ctx->ep1_in_req.complete = fb_oem_cmd_peek_exe;
	usbd_req_submit(context, &(fb_ctx->ep1_in_req));
}

static fb_status
fb_oem_cmd_peek(usbd *context,
		char *cmd)
{
	fb_mem *fb_ctx = context->ctx;
	fb_cmd_peek *peek = &fb_ctx->peek;

	peek->addr = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	if (*cmd == 'c') {
		peek->access = 1;
		peek->ascii = true;
		cmd++;
	} else {
		peek->ascii = false;
		peek->access = simple_strtoull(cmd, &cmd, 0);
		if (peek->access != 1 && peek->access != 2 &&
		    peek->access != 4 && peek->access != 8) {
			return FB_BAD_COMMAND;
		}
	}

	peek->items = 1;
	if (*cmd == ' ') {
		cmd++;
		/*
		 * Have items.
		 */
		peek->items = simple_strtoull(cmd, &cmd, 0);
	}

	if (*cmd != '\0') {
		return FB_BAD_COMMAND;
	}

	C_ASSERT(sizeof(fb_ctx->ep1_in_req.small_buffer) >= 64);
	peek->buffer = (char *) fb_ctx->ep1_in_req.small_buffer;
	peek->buffer_len = sizeof(fb_ctx->ep1_in_req.small_buffer);
	fb_oem_cmd_peek_exe(context, NULL);
	return FB_OK;
}

static fb_status
fb_oem_cmd_poke(usbd *context,
		char *cmd)
{
	size_t access;
	bool_t ascii;
	phys_addr_t addr;

	addr = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	if (*cmd == 'c') {
		access = 1;
		ascii = true;
		cmd++;
	} else {
		ascii = false;
		access = simple_strtoull(cmd, &cmd, 0);
		if (access != 1 && access != 2 &&
		    access != 4 && access != 8) {
			return FB_BAD_COMMAND;
		}
	}

	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	while (*cmd != '\0') {
		if (ascii) {
			OUT8(*(uint8_t *) cmd, addr);
			cmd++;
		} else {
			uint64_t value = simple_strtoull(cmd, &cmd, 0);
			if (*cmd == ' ') {
				cmd++;
			} else if (*cmd != '\0') {
				return FB_BAD_COMMAND;
			}

			if (access == 1) {
				OUT8((uint8_t) value, addr);
			} else if (access == 2) {
				OUT16((uint16_t) value, addr);
			} else if (access == 4) {
				OUT32((uint32_t) value, addr);
			} else if (access == 8) {
				OUT64((uint64_t) value, addr);
			}
		}

		addr += access;
	}

	fb_end_command(context, FB_OK);
	return FB_OK;
}

static fb_status
fb_oem_cmd_echo(usbd *context,
		char *cmd)
{
	printk("%s\n", cmd);
	fb_end_command(context, FB_OK);
	return FB_OK;
}

static fb_status
fb_oem_cmd_alloc_ex(usbd *context,
		    char *cmd,
		    phys_addr_t max_addr)
{
	size_t size;
	size_t align;
	lmb_type_t type;
	phys_addr_t addr;

	size = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	align = simple_strtoull(cmd, &cmd, 0);

	type = LMB_BOOT;
	if (*cmd == ' ') {
		cmd++;

		if (!memcmp(cmd, "boot", sizeof("boot"))) {
			type = LMB_BOOT;
		} else if (!memcmp(cmd, "runtime", sizeof("runtime"))) {
			/*
			 * Today, there is no difference between boot and
			 * runtime allocations. Tomorrow? We'll see.
			 */
			type = LMB_RUNTIME;
		} else {
			return FB_BAD_COMMAND;
		}
	}

	addr = lmb_alloc_base(&lmb, size, align, max_addr,
			      type, LMB_TAG("FBRQ"));
	if (addr == 0) {
		return FB_FAIL_COMMAND;
	}

	fb_end_command_with_info(context, "0x%lx", addr);
	return FB_OK;
}

static fb_status
fb_oem_cmd_alloc(usbd *context,
		 char *cmd)
{
	return fb_oem_cmd_alloc_ex(context, cmd, LMB_ALLOC_ANYWHERE);
}

static fb_status
fb_oem_cmd_alloc32(usbd *context,
		   char *cmd)
{
	return fb_oem_cmd_alloc_ex(context, cmd, LMB_ALLOC_32BIT);
}

static fb_status
fb_oem_cmd_free(usbd *context,
		char *cmd)
{
	phys_addr_t addr;
	size_t size;
	size_t align;

	addr = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	size = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != ' ') {
		return FB_BAD_COMMAND;
	}
	cmd++;

	align = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != '\0') {
		return FB_BAD_COMMAND;
	}

	lmb_free(&lmb, addr, size, align);

	fb_end_command(context, FB_OK);
	return FB_OK;
}

static fb_status
fb_oem_cmd_smccc(usbd *context,
		 char *cmd)
{
	int i;
	uint64_t inout[8];
	extern void smc_call(uint64_t *inout);

	memset(inout, 0, sizeof(inout));
	for (i = 0; i < ELES(inout); i++) {
		inout[i] = simple_strtoull(cmd, &cmd, 0);
		if (*cmd == '\0') {
			break;
		}

		if (*cmd != ' ') {
			return FB_BAD_COMMAND;
		}
		cmd++;
	}

	smc_call(inout);
	fb_end_command_with_info(context, "0x%lx 0x%lx 0x%lx 0x%lx",
				 inout[0], inout[1], inout[2], inout[3]);

	return FB_OK;
}

static fb_status
fb_cmd_reboot(usbd *context,
	      reboot_type type)
{
	fb_end_command(context, FB_OK);

	tegra_reboot(type);
	return FB_OK;
}

static fb_status
fb_oem_cmd_reboot(usbd *context,
		  char *cmd)
{
	uint32_t s;

	if (!strcmp(cmd, "normal")) {
		return fb_cmd_reboot(context, REBOOT_NORMAL);
	} else if (!strcmp(cmd, "bootloader")) {
		return fb_cmd_reboot(context, REBOOT_BOOTLOADER);
	} else if (!strcmp(cmd, "rcm")) {
		return fb_cmd_reboot(context, REBOOT_RCM);
	} else if (!strcmp(cmd, "recovery")) {
		return fb_cmd_reboot(context, REBOOT_RECOVERY);
	}

	s = simple_strtoull(cmd, &cmd, 0);
	if (*cmd != '\0') {
		return FB_BAD_COMMAND;
	}

	return fb_cmd_reboot(context, s);
}

static fb_status
fb_cmd_oem(usbd *context,
	   char *cmd)
{
	fb_status status = FB_UNKNOWN_COMMAND;

#define CMD_LIST					\
	CMD(peek)					\
	CMD(poke)					\
	CMD(echo)					\
	CMD(alloc)					\
	CMD(alloc32)					\
	CMD(free)					\
	CMD(smccc)					\
	CMD(reboot)					\

#define CMD(x) else if (!memcmp(cmd, S(x)" ", sizeof(S(x)" ") - 1)) {	\
		status = fb_oem_cmd_##x(context, cmd + sizeof(S(x)" ") - 1); \
	}

	if (0) {
	} CMD_LIST;

#undef CMD
#undef CMD_LIST

	return status;
}

static void
fb_rx_cmd_complete(usbd *context,
		   usbd_req *req)
{
	fb_status status = FB_UNKNOWN_COMMAND;
	fb_mem *fb_ctx = context->ctx;
	char *cbuf = req->buffer;

	/*
	 * Resubmit command.
	 */
	if (context->current_config != 0) {
		fb_rx_cmd(context, NULL);
	}

	if (req->error) {
		return;
	}

	if (fb_ctx->in_command) {
		/*
		 * Cancel previous command. Note that this TX
		 * request, submitted some time in the past,
		 * is probably received by fastboot, and we
		 * just haven't acknowledged the TX completion.
		 * usbd_req_cancel most likely will just cancel
		 * the completion ack. But this is fine...
		 */
		fb_ctx->in_command = false;
		usbd_req_cancel(context, &(fb_ctx->ep1_in_req));
	}
	fb_ctx->in_command = true;

	/*
	 * For ease of parsing, consider this to be an ASCIIZ buffer.
	 */
	cbuf[sizeof(fb_ctx->ep1_out_req.small_buffer) - 1] = '\0';

#define CMD_LIST				\
	CMD(oem)				\
	CMD_NO_PARAM(reboot)				\

#define CMD(x) else  \
		}

#define CMD_NO_PARAM(x) else if (!memcmp(req->buffer, S(x), sizeof(S(x)) - 1)) { \
			status = fb_cmd_##x(context,((char *) req->buffer) + sizeof(S(x)) - 1); \
		}

	if (!memcmp(req->buffer, "oem ", sizeof("oem ") - 1)) {
		status = fb_cmd_oem(context,
				    ((char *) req->buffer) + sizeof("oem ") - 1);
	} else if (!strcmp(req->buffer, "reboot")) {
		status = fb_cmd_reboot(context, REBOOT_NORMAL);
	} else if (!strcmp(req->buffer, "reboot-bootloader")) {
		status = fb_cmd_reboot(context, REBOOT_BOOTLOADER);
	}

#undef CMD
#undef CMD_NO_PARAM
#undef CMD_LIST

	if (status == FB_OK) {
		/*
		 * Status already reported.
		 */
		return;
	}

	fb_end_command(context, status);

}

static void
fb_rx_cmd(struct usbd *context, usbd_req *unused)
{
	fb_mem *fb_ctx = context->ctx;

	C_ASSERT(sizeof(fb_ctx->ep1_out_req.small_buffer) >= 64);
	fb_ctx->ep1_out_req.buffer = fb_ctx->ep1_out_req.small_buffer;
	fb_ctx->ep1_out_req.buffer_length = 64;
	fb_ctx->ep1_out_req.complete = fb_rx_cmd_complete;
	usbd_req_submit(context, &(fb_ctx->ep1_out_req));
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
		fb_rx_cmd(context, NULL);
	} else {
		fb_mem *fb_ctx = context->ctx;
		usbd_ep_disable(context, &fb_ep1_out, &fb_ep1_in);
		fb_ctx->in_command = false;
		usbd_req_cancel(context, &(fb_ctx->ep1_in_req));
		usbd_req_cancel(context, &(fb_ctx->ep1_out_req));
	}

	return USBD_SUCCESS;
}

void
fb_launch(void)
{
	fb_mem *fb_ctx;
	usbd_status usbd_stat;
	phys_addr_t usb_dma_memory;

	usb_dma_memory = lmb_alloc_base(&lmb, sizeof(fb_mem),
					USBD_TD_ALIGNMENT,
					LMB_ALLOC_32BIT, LMB_BOOT,
					LMB_TAG("UDMA"));

	lmb_dump_all(&lmb);

	fb_ctx = VP(usb_dma_memory);
	memset(fb_ctx, 0, sizeof(fb_mem));
	fb_ctx->uctx.ehci_udc_base = TEGRA_EHCI_BASE;
	fb_ctx->uctx.ctx = fb_ctx;
	fb_ctx->uctx.eps = fb_eps;
	fb_ctx->uctx.fs_descs = descr_fs;
	fb_ctx->uctx.hs_descs = descr_hs;
	fb_ctx->uctx.set_config = fb_set_config;

	usbd_stat = usbd_init(&(fb_ctx->uctx), fb_ctx->qtds, ELES(fb_ctx->qtds));
	BUG_ON (usbd_stat != USBD_SUCCESS);

	usbd_req_init(&(fb_ctx->ep1_out_req), &fb_ep1_out);
	usbd_req_init(&(fb_ctx->ep1_in_req), &fb_ep1_in);

	while(1) {
		usbd_poll(&(fb_ctx->uctx));
	}
}
