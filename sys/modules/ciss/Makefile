# $FreeBSD: src/sys/modules/ciss/Makefile,v 1.2.38.1 2010/02/10 00:26:20 kensmith Exp $

KMOD		=  ciss
.PATH:		${.CURDIR}/../../dev/${KMOD}
SRCS		=  ciss.c
SRCS		+= opt_scsi.h opt_cam.h
SRCS		+= device_if.h bus_if.h pci_if.h

#CFLAGS		+=-DCISS_DEBUG

.include <bsd.kmod.mk>
