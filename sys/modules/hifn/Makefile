# $FreeBSD: src/sys/modules/hifn/Makefile,v 1.6.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/hifn
KMOD	= hifn
SRCS	= hifn7751.c opt_hifn.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bus.h cryptodev_if.h

.if !defined(KERNBUILDDIR)
opt_hifn.h:
	echo "#define HIFN_DEBUG 1" > opt_hifn.h
.endif

.include <bsd.kmod.mk>
