# $FreeBSD: src/sys/modules/sound/driver/hda/Makefile,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/sound/pci/hda

KMOD=	snd_hda
SRCS=	device_if.h bus_if.h pci_if.h channel_if.h mixer_if.h
SRCS+=	hdac.c hdac_private.h hdac_reg.h hda_reg.h hdac.h

.include <bsd.kmod.mk>
