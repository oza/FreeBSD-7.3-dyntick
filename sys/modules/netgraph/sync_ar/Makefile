# $FreeBSD: src/sys/modules/netgraph/sync_ar/Makefile,v 1.3.10.1 2010/02/10 00:26:20 kensmith Exp $
 
.PATH:  ${.CURDIR}/../../../dev/ar
KMOD    = ng_sync_ar
SRCS    = if_ar.c if_ar_isa.c if_ar_pci.c
SRCS    += device_if.h bus_if.h pci_if.h isa_if.h opt_netgraph.h
  
.if !defined(KERNBUILDDIR)
opt_netgraph.h:
	echo "#define NETGRAPH" > opt_netgraph.h
.endif

.include <bsd.kmod.mk>
