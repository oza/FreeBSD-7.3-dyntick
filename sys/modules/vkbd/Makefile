# $Id: Makefile,v 1.1 2004/08/13 18:30:24 max Exp $
# $FreeBSD: src/sys/modules/vkbd/Makefile,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/vkbd

KMOD=	vkbd
SRCS=	vkbd.c opt_compat.h opt_kbd.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
