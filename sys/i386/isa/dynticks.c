/*-
 * Copyright (c) 2003 OZAWA Tsuyoshi <ozawa@t-oza.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/amd64/isa/dynticks.c,v 0.1.0 2010/05/02 16:26:20 OZAWATsusyohi Exp $");

#include <sys/dynticks.h>
#include <sys/systm.h>

static int enable_dynticks = 0;
static int use_ext_clksrc = 0;
static void (*cur_handler)(struct trapframe *frame);
static int (*cur_ext_handler)(struct trapframe *frame);

static struct timer_ops *tops;

void timer_intr_handler(struct trapframe *frame)
{
	if(!cur_handler)
		panic("cur_handler is NULL!\n");

	cur_handler(frame);
}

int ext_timer_intr_handler(struct trapframe *frame)
{
	if(!cur_ext_handler)
		panic("cur_ext_handler is NULL!\n");

	return cur_ext_handler(frame);
}

void register_timer_intr_handlers(struct timer_ops *ops)
{
	tops = ops;

	if (ops->perticks_handler) {
		cur_handler = ops->perticks_handler;
		use_ext_clksrc = 0;
	}

	if ( tops->perticks_handler && tops->dynticks_handler) {
		enable_dynticks = 1;
	}
}

void register_ext_timer_intr_handlers(struct timer_ops *ops)
{
	tops = ops;

	if (tops->ext_perticks_handler) {
		cur_ext_handler = ops->ext_perticks_handler;
		use_ext_clksrc = 1;
	}

	if (tops->ext_perticks_handler && tops->ext_dynticks_handler) {
		enable_dynticks = 1;
	}
}

void switch_to_dynticks(void)
{
	if(!enable_dynticks)
		return;

	/* TODO: swtich external clock handler */
	if(use_ext_clksrc)
		return;

	critical_enter();
	cur_handler = tops->dynticks_handler;
	tops->set_next_timer_intr();
	critical_exit();
}

void switch_to_perticks(void)
{
	if(!enable_dynticks)
		return;

	/* TODO: swtich external clock handler */
	if(use_ext_clksrc)
		return;

	if(!tops->perticks_handler){
		panic("NULL perticks handler!\n");
		return;
	}

	critical_enter();
	cur_handler = tops->perticks_handler;
	tops->set_timer_periodic();
	critical_exit();
}
