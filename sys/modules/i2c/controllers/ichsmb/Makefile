#$FreeBSD: src/sys/modules/i2c/controllers/ichsmb/Makefile,v 1.1.26.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/ichsmb
KMOD		= ichsmb
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ichsmb.c ichsmb_pci.c ichsmb_reg.h ichsmb_var.h

.include <bsd.kmod.mk>
