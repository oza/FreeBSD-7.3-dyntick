#
# $FreeBSD: src/sys/modules/hwpmc/Makefile,v 1.4.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $
#

.PATH: ${.CURDIR}/../../dev/hwpmc

KMOD=	hwpmc

SRCS=	hwpmc_mod.c hwpmc_logging.c vnode_if.h

WARNS?=	2

.if ${MACHINE_ARCH} == "amd64"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c hwpmc_piv.c hwpmc_tsc.c
SRCS+=	hwpmc_x86.c
SRCS+=	device_if.h bus_if.h
.endif

.if ${MACHINE_ARCH} == "arm"
SRCS+=	hwpmc_arm.c
.endif

.if ${MACHINE_ARCH} == "i386"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c hwpmc_piv.c hwpmc_ppro.c
SRCS+=	hwpmc_pentium.c hwpmc_tsc.c hwpmc_x86.c
SRCS+=	device_if.h bus_if.h
.endif

.if ${MACHINE_ARCH} == "ia64"
SRCS+=	hwpmc_ia64.c
.endif

.if ${MACHINE_ARCH} == "powerpc"
SRCS+=	hwpmc_powerpc.c
.endif

.if ${MACHINE_ARCH} == "sparc64"
SRCS+=	hwpmc_sparc64.c
.endif

.include <bsd.kmod.mk>
