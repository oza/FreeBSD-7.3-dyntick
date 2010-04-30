/*-
 * Copyright (c) 1997-2000 Nicolas Souchu
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 *
 * $FreeBSD: src/sys/dev/ppc/ppcvar.h,v 1.5.10.1 2010/02/10 00:26:20 kensmith Exp $
 *
 */

int ppc_probe(device_t dev, int rid);
int ppc_attach(device_t dev);
int ppc_detach(device_t dev);
int ppc_read_ivar(device_t bus, device_t dev, int index, uintptr_t *val);

int ppc_read(device_t, char *, int, int);
int ppc_write(device_t, char *, int, int);

u_char ppc_io(device_t, int, u_char *, int, u_char);
int ppc_exec_microseq(device_t, struct ppb_microseq **);

int ppc_setup_intr(device_t, device_t, struct resource *, int,
		driver_filter_t *filt, void (*)(void *), void *, void **);
int ppc_teardown_intr(device_t, device_t, struct resource *, void *);
void ppc_reset_epp(device_t);
void ppc_ecp_sync(device_t);
int ppc_setmode(device_t, int);

extern devclass_t ppc_devclass;
extern const char ppc_driver_name[];