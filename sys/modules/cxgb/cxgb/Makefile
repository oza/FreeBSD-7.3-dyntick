# $FreeBSD: src/sys/modules/cxgb/cxgb/Makefile,v 1.8.2.4.2.1 2010/02/10 00:26:20 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} ${CXGB}/common ${CXGB}/sys

KMOD=	if_cxgb
SRCS=   cxgb_mc5.c cxgb_vsc8211.c cxgb_ael1002.c cxgb_mv88e1xxx.c 
SRCS+=	cxgb_xgmac.c cxgb_vsc7323.c cxgb_t3_hw.c cxgb_main.c cxgb_aq100x.c
SRCS+=  cxgb_sge.c cxgb_offload.c cxgb_tn1010.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_zero.h opt_sched.h
SRCS+=	uipc_mvec.c cxgb_support.c cxgb_multiq.c 
CFLAGS+= -DCONFIG_CHELSIO_T3_CORE -g -DCONFIG_DEFINED -DDEFAULT_JUMBO -I${CXGB} -DSMP
CFLAGS+= -DDISABLE_MBUF_IOVEC
#CFLAGS+= -DIFNET_MULTIQUEUE
#CFLAGS+= -DDISABLE_MBUF_IOVEC
#CFLAGS+= -DDEBUG -DDEBUG_PRINT
#CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS 
#CFLAGS+= -DWITNESS
#CFLAGS += -DLOCK_PROFILING
#CFLAGS+= -DWITNESS
#CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DWITNESS


.include <bsd.kmod.mk>
