/*-
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)inode.h	8.9 (Berkeley) 5/14/95
 * $FreeBSD: src/sys/gnu/fs/ext2fs/inode.h,v 1.44.10.1.6.1 2010/02/10 00:26:20 kensmith Exp $
 */

#ifndef _SYS_GNU_EXT2FS_INODE_H_
#define	_SYS_GNU_EXT2FS_INODE_H_

#include <sys/lock.h>
#include <sys/queue.h>

#define	ROOTINO	((ino_t)2)

#define	NDADDR	12			/* Direct addresses in inode. */
#define	NIADDR	3			/* Indirect addresses in inode. */

/*
 * This must agree with the definition in <ufs/ufs/dir.h>.
 */
#define	doff_t		int32_t

/*
 * The inode is used to describe each active (or recently active) file in the
 * EXT2FS filesystem. It is composed of two types of information. The first
 * part is the information that is needed only while the file is active (such
 * as the identity of the file and linkage to speed its lookup). The second
 * part is the permanent meta-data associated with the file which is read in
 * from the permanent dinode from long term storage when the file becomes
 * active, and is put back when the file is no longer being used.
 */
struct inode {
	struct	vnode  *i_vnode;/* Vnode associated with this inode. */
	struct	vnode  *i_devvp;/* Vnode for block I/O. */
	u_int32_t i_flag;	/* flags, see below */
	ino_t	  i_number;	/* The identity of the inode. */

	struct	ext2_sb_info *i_e2fs;	/* EXT2FS */
	u_quad_t i_modrev;	/* Revision level for NFS lease. */
	/*
	 * Side effects; used during directory lookup.
	 */
	int32_t	  i_count;	/* Size of free slot in directory. */
	doff_t	  i_endoff;	/* End of useful stuff in directory. */
	doff_t	  i_diroff;	/* Offset in dir, where we found last entry. */
	doff_t	  i_offset;	/* Offset of free space in directory. */
	ino_t	  i_ino;	/* Inode number of found directory. */
	u_int32_t i_reclen;	/* Size of found directory entry. */

	u_int32_t i_block_group;
	u_int32_t i_next_alloc_block;
	u_int32_t i_next_alloc_goal;
	u_int32_t i_prealloc_block;
	u_int32_t i_prealloc_count;

	/* Fields from struct dinode in UFS. */
	u_int16_t	i_mode;		/* IFMT, permissions; see below. */
	int16_t		i_nlink;	/* File link count. */
	u_int64_t	i_size;		/* File byte count. */
	int32_t		i_atime;	/* Last access time. */
	int32_t		i_atimensec;	/* Last access time. */
	int32_t		i_mtime;	/* Last modified time. */
	int32_t		i_mtimensec;	/* Last modified time. */
	int32_t		i_ctime;	/* Last inode change time. */
	int32_t		i_ctimensec;	/* Last inode change time. */
	int32_t		i_db[NDADDR];	/* Direct disk blocks. */
	int32_t		i_ib[NIADDR];	/* Indirect disk blocks. */
	u_int32_t	i_flags;	/* Status flags (chflags). */
	int32_t		i_blocks;	/* Blocks actually held. */
	int32_t		i_gen;		/* Generation number. */
	u_int32_t	i_uid;		/* File owner. */
	u_int32_t	i_gid;		/* File group. */
};

/*
 * The di_db fields may be overlaid with other information for
 * file types that do not have associated disk storage. Block
 * and character devices overlay the first data block with their
 * dev_t value. Short symbolic links place their path in the
 * di_db area.
 */
#define	i_shortlink	i_db
#define	i_rdev		i_db[0]
#define	MAXSYMLINKLEN	((NDADDR + NIADDR) * sizeof(int32_t))

/* File permissions. */
#define	IEXEC		0000100		/* Executable. */
#define	IWRITE		0000200		/* Writeable. */
#define	IREAD		0000400		/* Readable. */
#define	ISVTX		0001000		/* Sticky bit. */
#define	ISGID		0002000		/* Set-gid. */
#define	ISUID		0004000		/* Set-uid. */

/* File types. */
#define	IFMT		0170000		/* Mask of file type. */
#define	IFIFO		0010000		/* Named pipe (fifo). */
#define	IFCHR		0020000		/* Character device. */
#define	IFDIR		0040000		/* Directory file. */
#define	IFBLK		0060000		/* Block device. */
#define	IFREG		0100000		/* Regular file. */
#define	IFLNK		0120000		/* Symbolic link. */
#define	IFSOCK		0140000		/* UNIX domain socket. */
#define	IFWHT		0160000		/* Whiteout. */

/* These flags are kept in i_flag. */
#define	IN_ACCESS	0x0001		/* Access time update request. */
#define	IN_CHANGE	0x0002		/* Inode change time update request. */
#define	IN_UPDATE	0x0004		/* Modification time update request. */
#define	IN_MODIFIED	0x0008		/* Inode has been modified. */
#define	IN_RENAME	0x0010		/* Inode is being renamed. */
#define	IN_HASHED	0x0020		/* Inode is on hash list */
#define	IN_LAZYMOD	0x0040		/* Modified, but don't write yet. */
#define	IN_SPACECOUNTED	0x0080		/* Blocks to be freed in free count. */

#ifdef _KERNEL
/*
 * Structure used to pass around logical block paths generated by
 * ext2_getlbns and used by truncate and bmap code.
 */
struct indir {
	int32_t in_lbn;			/* Logical block number. */
	int	in_off;			/* Offset in buffer. */
	int	in_exists;		/* Flag if the block exists. */
};

/* Convert between inode pointers and vnode pointers. */
#define VTOI(vp)	((struct inode *)(vp)->v_data)
#define ITOV(ip)	((ip)->i_vnode)

/* This overlays the fid structure (see mount.h). */
struct ufid {
	u_int16_t ufid_len;	/* Length of structure. */
	u_int16_t ufid_pad;	/* Force 32-bit alignment. */
	ino_t	  ufid_ino;	/* File number (ino). */
	int32_t	  ufid_gen;	/* Generation number. */
};
#endif /* _KERNEL */

#endif /* !_SYS_GNU_EXT2FS_INODE_H_ */