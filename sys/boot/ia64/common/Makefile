# $FreeBSD: src/sys/boot/ia64/common/Makefile,v 1.1.10.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

LIB=		ia64
INTERNALLIB=

SRCS=		autoload.c bootinfo.c copy.c devicename.c exec.c

CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

.if ${MK_FORTH} != "no"
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_ARCH}
.endif

.PATH: ${.CURDIR}/../../common
.include "${.CURDIR}/../../common/Makefile.inc"

CFLAGS+=	-I${.CURDIR}/../../common

FILES+=		loader.help
CLEANFILES+=	loader.help
loader.help: help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES+=		loader.4th support.4th loader.conf
.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=		loader.rc
.endif
FILESDIR_loader.conf= /boot/defaults

.include <bsd.lib.mk>
