# $FreeBSD: src/sys/modules/nfsserver/Makefile,v 1.26.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../nfsserver ${.CURDIR}/../../nfs
KMOD=	nfsserver
SRCS=	vnode_if.h \
	nfs_serv.c nfs_srvsock.c nfs_srvcache.c nfs_srvsubs.c nfs_syscalls.c \
	nfs_common.c \
	opt_mac.h \
	opt_nfs.h
SRCS+=	opt_inet6.h

.if !defined(KERNBUILDDIR)
NFS_INET6?=	1	# 0/1 - requires INET6 to be configured in kernel

.if ${NFS_INET6} > 0
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
