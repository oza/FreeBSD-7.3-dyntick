# $FreeBSD: src/sys/modules/i2c/controllers/amdsmb/Makefile,v 1.1.14.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../../pci

KMOD=	amdsmb
SRCS=	amdsmb.c
SRCS+=	device_if.h smbus_if.h pci_if.h bus_if.h

.include <bsd.kmod.mk>
