# $FreeBSD: src/sys/modules/cyclic_test/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../cddl/dev/cyclic

KMOD=		cyclic_test
SRCS=		cyclic_test.c
		
#SRCS+=		bus_if.h device_if.h vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
