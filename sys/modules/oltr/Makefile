# $FreeBSD: src/sys/modules/oltr/Makefile,v 1.5.22.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/dev/oltr
KMOD=	if_oltr
SRCS=	if_oltr.c trlldbm.c trlldhm.c trlldmac.c \
	device_if.h bus_if.h
SRCS+=	if_oltr_isa.c isa_if.h
SRCS+=	if_oltr_pci.c pci_if.h
OBJS+=	trlld.o

trlld.o:
	uudecode < ${.CURDIR}/../../contrib/dev/oltr/i386-elf.trlld.o.uu

.include <bsd.kmod.mk>
