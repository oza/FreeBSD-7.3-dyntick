# $FreeBSD: src/sys/modules/krpc/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../rpc ${.CURDIR}/../../xdr
KMOD=	krpc
SRCS=	auth_none.c \
	auth_unix.c \
	authunix_prot.c \
	clnt_dg.c \
	clnt_rc.c \
	clnt_vc.c \
	getnetconfig.c \
	inet_ntop.c \
	inet_pton.c \
	rpc_callmsg.c \
	rpc_generic.c \
	rpc_prot.c \
	rpcb_clnt.c \
	rpcb_prot.c \
	svc.c \
	svc_auth.c \
	svc_auth_unix.c \
	svc_dg.c \
	svc_generic.c \
	svc_vc.c \

SRCS+=	xdr.c \
	xdr_array.c \
	xdr_mbuf.c \
	xdr_mem.c \
	xdr_reference.c \
	xdr_sizeof.c

SRCS+=	opt_inet6.h

.if !defined(KERNBUILDDIR)
NFS_INET6?=	1	# 0/1 - requires INET6 to be configured in kernel

.if ${NFS_INET6} > 0
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
