# $Id: Makefile,v 1.1 2002/11/24 20:40:11 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/l2cap/Makefile,v 1.5.32.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/l2cap

CFLAGS+=	-I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-I${.CURDIR}/../../../../netgraph/bluetooth/l2cap

KMOD=		ng_l2cap
SRCS=		ng_l2cap_main.c ng_l2cap_cmds.c ng_l2cap_evnt.c \
		ng_l2cap_ulpi.c ng_l2cap_llpi.c ng_l2cap_misc.c

.include <bsd.kmod.mk>
