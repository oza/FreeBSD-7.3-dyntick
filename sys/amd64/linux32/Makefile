# Makefile for syscall tables
#
# $FreeBSD: src/sys/amd64/linux32/Makefile,v 1.1.32.1 2010/02/10 00:26:20 kensmith Exp $

all:
	@echo "make sysent only"

sysent:  linux32_sysent.c linux32_syscall.h linux32_proto.h

linux32_sysent.c linux32_syscall.h linux32_proto.h: ../../kern/makesyscalls.sh \
		syscalls.master syscalls.conf
	-mv -f linux32_sysent.c linux32_sysent.c.bak
	-mv -f linux32_syscall.h linux32_syscall.h.bak
	-mv -f linux32_proto.h linux32_proto.h.bak
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
