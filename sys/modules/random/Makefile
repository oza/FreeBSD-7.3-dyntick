# $FreeBSD: src/sys/modules/random/Makefile,v 1.15.32.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/random
.PATH: ${.CURDIR}/../../crypto/rijndael
.PATH: ${.CURDIR}/../../crypto/sha2

KMOD=	random
SRCS=	randomdev.c probe.c
.if ${MACHINE} == "i386"
SRCS+=	nehemiah.c
.endif
SRCS+=	randomdev_soft.c yarrow.c hash.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c sha2.c
SRCS+=	bus_if.h device_if.h vnode_if.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
