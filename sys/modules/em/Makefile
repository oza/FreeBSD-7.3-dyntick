# $FreeBSD: src/sys/modules/em/Makefile,v 1.9.2.2.6.1 2010/02/10 00:26:20 kensmith Exp $
.PATH:  ${.CURDIR}/../../dev/e1000
KMOD    = if_em
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += if_em.c $(SHARED_SRCS)
SHARED_SRCS = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c e1000_manage.c
SHARED_SRCS += e1000_80003es2lan.c e1000_82542.c e1000_82541.c e1000_82543.c
SHARED_SRCS += e1000_82540.c e1000_ich8lan.c e1000_82571.c e1000_osdep.c
SHARED_SRCS += e1000_82575.c 

CFLAGS+= -I${.CURDIR}/../../dev/e1000

# DEVICE_POLLING gives you Legacy interrupt handling
#CFLAGS  += -DDEVICE_POLLING

clean:
	rm -f device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine
	rm -f ${CLEANFILES}

.include <bsd.kmod.mk>
