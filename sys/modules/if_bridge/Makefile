# $FreeBSD: src/sys/modules/if_bridge/Makefile,v 1.6.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../net
KMOD=	if_bridge
SRCS=	if_bridge.c opt_inet.h opt_inet6.h opt_carp.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
	
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
