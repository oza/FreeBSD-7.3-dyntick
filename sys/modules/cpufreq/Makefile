# $FreeBSD: src/sys/modules/cpufreq/Makefile,v 1.9.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/cpufreq		\
	${.CURDIR}/../../${MACHINE_ARCH}/cpufreq

KMOD=	cpufreq
SRCS=	ichss.c
SRCS+=	bus_if.h cpufreq_if.h device_if.h pci_if.h

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
.PATH:	${.CURDIR}/../../i386/cpufreq
CFLAGS+= -I${.CURDIR}/../../contrib/dev/acpica

SRCS+=	acpi_if.h opt_acpi.h
SRCS+=	est.c hwpstate.c p4tcc.c powernow.c
.endif

.if ${MACHINE} == "i386"
SRCS+=	smist.c
.endif

.include <bsd.kmod.mk>
