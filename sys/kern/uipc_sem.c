/*-
 * Copyright (c) 2002 Alfred Perlstein <alfred@FreeBSD.org>
 * Copyright (c) 2003-2005 SPARTA, Inc.
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/uipc_sem.c,v 1.28.2.5.4.1 2010/02/10 00:26:20 kensmith Exp $");

#include "opt_mac.h"
#include "opt_posix.h"

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>
#include <sys/kernel.h>
#include <sys/ksem.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/posix4.h>
#include <sys/refcount.h>
#include <sys/semaphore.h>
#include <sys/_semaphore.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/vnode.h>

#include <security/mac/mac_framework.h>

/*
 * TODO
 *
 * - Resource limits?
 * - Update fstat(1)
 * - Replace global sem_lock with mtx_pool locks?
 * - Add a MAC check_create() hook for creating new named semaphores.
 */

#ifndef SEM_MAX
#define	SEM_MAX	30
#endif

#ifdef SEM_DEBUG
#define	DP(x)	printf x
#else
#define	DP(x)
#endif

struct ksem_mapping {
	char		*km_path;
	Fnv32_t		km_fnv;
	struct ksem	*km_ksem;
	LIST_ENTRY(ksem_mapping) km_link;
};

static MALLOC_DEFINE(M_KSEM, "ksem", "semaphore file descriptor");
static LIST_HEAD(, ksem_mapping) *ksem_dictionary;
static struct sx ksem_dict_lock;
static struct mtx ksem_count_lock;
static struct mtx sem_lock;
static u_long ksem_hash;
static int ksem_dead;

#define	KSEM_HASH(fnv)	(&ksem_dictionary[(fnv) & ksem_hash])

static int nsems = 0;
SYSCTL_DECL(_p1003_1b);
SYSCTL_INT(_p1003_1b, OID_AUTO, nsems, CTLFLAG_RD, &nsems, 0,
    "Number of active kernel POSIX semaphores");

static int	kern_sem_wait(struct thread *td, semid_t id, int tryflag,
		    struct timespec *abstime);
static int	ksem_access(struct ksem *ks, struct ucred *ucred);
static struct ksem *ksem_alloc(struct ucred *ucred, mode_t mode,
		    unsigned int value);
static int	ksem_create(struct thread *td, const char *path,
		    semid_t *semidp, mode_t mode, unsigned int value,
		    int flags);
static void	ksem_drop(struct ksem *ks);
static int	ksem_get(struct thread *td, semid_t id, struct file **fpp);
static struct ksem *ksem_hold(struct ksem *ks);
static void	ksem_insert(char *path, Fnv32_t fnv, struct ksem *ks);
static struct ksem *ksem_lookup(char *path, Fnv32_t fnv);
static void	ksem_module_destroy(void);
static int	ksem_module_init(void);
static int	ksem_remove(char *path, Fnv32_t fnv, struct ucred *ucred);
static int	sem_modload(struct module *module, int cmd, void *arg);

static fo_rdwr_t	ksem_read;
static fo_rdwr_t	ksem_write;
static fo_ioctl_t	ksem_ioctl;
static fo_poll_t	ksem_poll;
static fo_kqfilter_t	ksem_kqfilter;
static fo_stat_t	ksem_stat;
static fo_close_t	ksem_closef;

/* File descriptor operations. */
static struct fileops ksem_ops = {
	.fo_read = ksem_read,
	.fo_write = ksem_write,
	.fo_ioctl = ksem_ioctl,
	.fo_poll = ksem_poll,
	.fo_kqfilter = ksem_kqfilter,
	.fo_stat = ksem_stat,
	.fo_close = ksem_closef,
	.fo_flags = DFLAG_PASSABLE
};

FEATURE(posix_sem, "POSIX semaphores");

static int
ksem_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
ksem_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
ksem_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
ksem_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
ksem_kqfilter(struct file *fp, struct knote *kn)
{

	return (EOPNOTSUPP);
}

static int
ksem_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{
	struct ksem *ks;
#ifdef MAC
	int error;
#endif

	ks = fp->f_data;

#ifdef MAC
	/*
	 * XXX: This isn't quite right, but adding a new stat() hook
	 * would break the ABI.  This seems to be the closest match of
	 * the existing MAC hooks for POSIX semaphores in 7.
	 */
	error = mac_check_posix_sem_getvalue(active_cred, ks);
	if (error)
		return (error);
#endif
	
	/*
	 * Attempt to return sanish values for fstat() on a semaphore
	 * file descriptor.
	 */
	bzero(sb, sizeof(*sb));
	sb->st_mode = S_IFREG | ks->ks_mode;		/* XXX */

	sb->st_atimespec = ks->ks_atime;
	sb->st_ctimespec = ks->ks_ctime;
	sb->st_mtimespec = ks->ks_mtime;
	sb->st_birthtimespec = ks->ks_birthtime;	
	sb->st_uid = ks->ks_uid;
	sb->st_gid = ks->ks_gid;

	return (0);
}

static int
ksem_closef(struct file *fp, struct thread *td)
{
	struct ksem *ks;

	ks = fp->f_data;
	fp->f_data = NULL;
	ksem_drop(ks);

	return (0);
}

/*
 * ksem object management including creation and reference counting
 * routines.
 */
static struct ksem *
ksem_alloc(struct ucred *ucred, mode_t mode, unsigned int value)
{
	struct ksem *ks;

	mtx_lock(&ksem_count_lock);
	if (nsems == p31b_getcfg(CTL_P1003_1B_SEM_NSEMS_MAX) || ksem_dead) {
		mtx_unlock(&ksem_count_lock);
		return (NULL);
	}
	nsems++;
	mtx_unlock(&ksem_count_lock);
	ks = malloc(sizeof(*ks), M_KSEM, M_WAITOK | M_ZERO);
	ks->ks_uid = ucred->cr_uid;
	ks->ks_gid = ucred->cr_gid;
	ks->ks_mode = mode;
	ks->ks_value = value;
	cv_init(&ks->ks_cv, "ksem");
	vfs_timestamp(&ks->ks_birthtime);
	ks->ks_atime = ks->ks_mtime = ks->ks_ctime = ks->ks_birthtime;
	refcount_init(&ks->ks_ref, 1);
#ifdef MAC
	mac_init_posix_sem(ks);
	mac_create_posix_sem(ucred, ks);
#endif

	return (ks);
}

static struct ksem *
ksem_hold(struct ksem *ks)
{

	refcount_acquire(&ks->ks_ref);
	return (ks);
}

static void
ksem_drop(struct ksem *ks)
{

	if (refcount_release(&ks->ks_ref)) {
#ifdef MAC
		mac_destroy_posix_sem(ks);
#endif
		cv_destroy(&ks->ks_cv);
		free(ks, M_KSEM);
		mtx_lock(&ksem_count_lock);
		nsems--;
		mtx_unlock(&ksem_count_lock);
	}
}

/*
 * Determine if the credentials have sufficient permissions for read
 * and write access.
 */
static int
ksem_access(struct ksem *ks, struct ucred *ucred)
{
	int error;

	error = vaccess(VREG, ks->ks_mode, ks->ks_uid, ks->ks_gid,
	    VREAD | VWRITE, ucred, NULL);
	if (error)
		error = priv_check_cred(ucred, PRIV_SEM_WRITE, 0);
	return (error);
}

/*
 * Dictionary management.  We maintain an in-kernel dictionary to map
 * paths to semaphore objects.  We use the FNV hash on the path to
 * store the mappings in a hash table.
 */
static struct ksem *
ksem_lookup(char *path, Fnv32_t fnv)
{
	struct ksem_mapping *map;

	LIST_FOREACH(map, KSEM_HASH(fnv), km_link) {
		if (map->km_fnv != fnv)
			continue;
		if (strcmp(map->km_path, path) == 0)
			return (map->km_ksem);
	}

	return (NULL);
}

static void
ksem_insert(char *path, Fnv32_t fnv, struct ksem *ks)
{
	struct ksem_mapping *map;

	map = malloc(sizeof(struct ksem_mapping), M_KSEM, M_WAITOK);
	map->km_path = path;
	map->km_fnv = fnv;
	map->km_ksem = ksem_hold(ks);
	LIST_INSERT_HEAD(KSEM_HASH(fnv), map, km_link);
}

static int
ksem_remove(char *path, Fnv32_t fnv, struct ucred *ucred)
{
	struct ksem_mapping *map;
	int error;

	LIST_FOREACH(map, KSEM_HASH(fnv), km_link) {
		if (map->km_fnv != fnv)
			continue;
		if (strcmp(map->km_path, path) == 0) {
#ifdef MAC
			error = mac_check_posix_sem_unlink(ucred, map->km_ksem);
			if (error)
				return (error);
#endif
			error = ksem_access(map->km_ksem, ucred);
			if (error)
				return (error);
			LIST_REMOVE(map, km_link);
			ksem_drop(map->km_ksem);
			free(map->km_path, M_KSEM);
			free(map, M_KSEM);
			return (0);
		}
	}

	return (ENOENT);
}

/* Other helper routines. */
static int
ksem_create(struct thread *td, const char *name, semid_t *semidp, mode_t mode,
    unsigned int value, int flags)
{
	struct filedesc *fdp;
	struct ksem *ks;
	struct file *fp;
	char *path;
	semid_t semid;
	Fnv32_t fnv;
	int error, fd;

	if (value > SEM_VALUE_MAX)
		return (EINVAL);

	fdp = td->td_proc->p_fd;
	mode = (mode & ~fdp->fd_cmask) & ACCESSPERMS;
	error = falloc(td, &fp, &fd);
	if (error) {
		if (name == NULL)
			error = ENOSPC;
		return (error);
	}

	/*
	 * Go ahead and copyout the file descriptor now.  This is a bit
	 * premature, but it is a lot easier to handle errors as opposed
	 * to later when we've possibly created a new semaphore, etc.
	 */
	semid = fd;
	error = copyout(&semid, semidp, sizeof(semid));
	if (error) {
		fdclose(fdp, fp, fd, td);
		fdrop(fp, td);
		return (error);
	}

	if (name == NULL) {
		/* Create an anonymous semaphore. */
		ks = ksem_alloc(td->td_ucred, mode, value);
		if (ks == NULL)
			error = ENOSPC;
		else
			ks->ks_flags |= KS_ANONYMOUS;
	} else {
		path = malloc(MAXPATHLEN, M_KSEM, M_WAITOK);
		error = copyinstr(name, path, MAXPATHLEN, NULL);

		/* Require paths to start with a '/' character. */
		if (error == 0 && path[0] != '/')
			error = EINVAL;
		if (error) {
			fdclose(fdp, fp, fd, td);
			fdrop(fp, td);
			free(path, M_KSEM);
			return (error);
		}

		fnv = fnv_32_str(path, FNV1_32_INIT);
		sx_xlock(&ksem_dict_lock);
		ks = ksem_lookup(path, fnv);
		if (ks == NULL) {
			/* Object does not exist, create it if requested. */
			if (flags & O_CREAT) {
				ks = ksem_alloc(td->td_ucred, mode, value);
				if (ks == NULL)
					error = ENFILE;
				else {
					ksem_insert(path, fnv, ks);
					path = NULL;
				}
			} else
				error = ENOENT;
		} else {
			/*
			 * Object already exists, obtain a new
			 * reference if requested and permitted.
			 */
			if ((flags & (O_CREAT | O_EXCL)) ==
			    (O_CREAT | O_EXCL))
				error = EEXIST;
			else {
#ifdef MAC
				error = mac_check_posix_sem_open(td->td_ucred,
				    ks);
				if (error == 0)
#endif
				error = ksem_access(ks, td->td_ucred);
			}
			if (error == 0)
				ksem_hold(ks);
#ifdef INVARIANTS
			else
				ks = NULL;
#endif
		}
		sx_xunlock(&ksem_dict_lock);
		if (path)
			free(path, M_KSEM);
	}

	if (error) {
		KASSERT(ks == NULL, ("ksem_create error with a ksem"));
		fdclose(fdp, fp, fd, td);
		fdrop(fp, td);
		return (error);
	}
	KASSERT(ks != NULL, ("ksem_create w/o a ksem"));

	fp->f_data = ks;
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_SEM;
	fp->f_ops = &ksem_ops;

	FILEDESC_XLOCK(fdp);
	if (fdp->fd_ofiles[fd] == fp)
		fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
	FILEDESC_XUNLOCK(fdp);
	fdrop(fp, td);

	return (0);
}

static int
ksem_get(struct thread *td, semid_t id, struct file **fpp)
{
	struct ksem *ks;
	struct file *fp;
	int error;

	error = fget(td, id, &fp);
	if (error)
		return (EINVAL);
	if (fp->f_type != DTYPE_SEM) {
		fdrop(fp, td);
		return (EINVAL);
	}
	ks = fp->f_data;
	if (ks->ks_flags & KS_DEAD) {
		fdrop(fp, td);
		return (EINVAL);
	}
	*fpp = fp;
	return (0);
}

/* System calls. */
#ifndef _SYS_SYSPROTO_H_
struct ksem_init_args {
	unsigned int	value;
	semid_t		*idp;
};
#endif
int
ksem_init(struct thread *td, struct ksem_init_args *uap)
{

	return (ksem_create(td, NULL, uap->idp, S_IRWXU | S_IRWXG, uap->value,
	    0));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_open_args {
	char		*name;
	int		oflag;
	mode_t		mode;
	unsigned int	value;
	semid_t		*idp;	
};
#endif
int
ksem_open(struct thread *td, struct ksem_open_args *uap)
{

	if ((uap->oflag & ~(O_CREAT | O_EXCL)) != 0)
		return (EINVAL);
	return (ksem_create(td, uap->name, uap->idp, uap->mode, uap->value,
	    uap->oflag));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_unlink_args {
	char		*name;
};
#endif
int
ksem_unlink(struct thread *td, struct ksem_unlink_args *uap)
{
	char *path;
	Fnv32_t fnv;
	int error;

	path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->name, path, MAXPATHLEN, NULL);
	if (error) {
		free(path, M_TEMP);
		return (error);
	}

	fnv = fnv_32_str(path, FNV1_32_INIT);
	sx_xlock(&ksem_dict_lock);
	error = ksem_remove(path, fnv, td->td_ucred);
	sx_xunlock(&ksem_dict_lock);
	free(path, M_TEMP);

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_close_args {
	semid_t		id;
};
#endif
int
ksem_close(struct thread *td, struct ksem_close_args *uap)
{
	struct ksem *ks;
	struct file *fp;
	int error;

	error = ksem_get(td, uap->id, &fp);
	if (error)
		return (error);
	ks = fp->f_data;
	if (ks->ks_flags & KS_ANONYMOUS) {
		fdrop(fp, td);
		return (EINVAL);
	}
	error = kern_close(td, uap->id);
	fdrop(fp, td);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_post_args {
	semid_t	id;
};
#endif
int
ksem_post(struct thread *td, struct ksem_post_args *uap)
{
	struct file *fp;
	struct ksem *ks;
	int error;

	error = ksem_get(td, uap->id, &fp);
	if (error)
		return (error);
	ks = fp->f_data;

	mtx_lock(&sem_lock);
#ifdef MAC
	error = mac_check_posix_sem_post(td->td_ucred, ks);
	if (error)
		goto err;
#endif
	if (ks->ks_value == SEM_VALUE_MAX) {
		error = EOVERFLOW;
		goto err;
	}
	++ks->ks_value;
	if (ks->ks_waiters > 0)
		cv_signal(&ks->ks_cv);
	error = 0;
	vfs_timestamp(&ks->ks_ctime);
err:
	mtx_unlock(&sem_lock);
	fdrop(fp, td);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_wait_args {
	semid_t		id;
};
#endif
int
ksem_wait(struct thread *td, struct ksem_wait_args *uap)
{

	return (kern_sem_wait(td, uap->id, 0, NULL));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_timedwait_args {
	semid_t		id;
	const struct timespec *abstime;
};
#endif
int
ksem_timedwait(struct thread *td, struct ksem_timedwait_args *uap)
{
	struct timespec abstime;
	struct timespec *ts;
	int error;

	/*
	 * We allow a null timespec (wait forever).
	 */
	if (uap->abstime == NULL)
		ts = NULL;
	else {
		error = copyin(uap->abstime, &abstime, sizeof(abstime));
		if (error != 0)
			return (error);
		if (abstime.tv_nsec >= 1000000000 || abstime.tv_nsec < 0)
			return (EINVAL);
		ts = &abstime;
	}
	return (kern_sem_wait(td, uap->id, 0, ts));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_trywait_args {
	semid_t		id;
};
#endif
int
ksem_trywait(struct thread *td, struct ksem_trywait_args *uap)
{

	return (kern_sem_wait(td, uap->id, 1, NULL));
}

static int
kern_sem_wait(struct thread *td, semid_t id, int tryflag,
    struct timespec *abstime)
{
	struct timespec ts1, ts2;
	struct timeval tv;
	struct file *fp;
	struct ksem *ks;
	int error;

	DP((">>> kern_sem_wait entered!\n"));
	error = ksem_get(td, id, &fp);
	if (error)
		return (error);
	ks = fp->f_data;
	mtx_lock(&sem_lock);
#ifdef MAC
	error = mac_check_posix_sem_wait(td->td_ucred, ks);
	if (error) {
		DP(("kern_sem_wait mac failed\n"));
		goto err;
	}
#endif
	DP(("kern_sem_wait value = %d, tryflag %d\n", ks->ks_value, tryflag));
	vfs_timestamp(&ks->ks_atime);
	while (ks->ks_value == 0) {
		ks->ks_waiters++;
		if (tryflag != 0)
			error = EAGAIN;
		else if (abstime == NULL)
			error = cv_wait_sig(&ks->ks_cv, &sem_lock);
		else {
			for (;;) {
				ts1 = *abstime;
				getnanotime(&ts2);
				timespecsub(&ts1, &ts2);
				TIMESPEC_TO_TIMEVAL(&tv, &ts1);
				if (tv.tv_sec < 0) {
					error = ETIMEDOUT;
					break;
				}
				error = cv_timedwait_sig(&ks->ks_cv,
				    &sem_lock, tvtohz(&tv));
				if (error != EWOULDBLOCK)
					break;
			}
		}
		ks->ks_waiters--;
		if (error)
			goto err;
	}
	ks->ks_value--;
	error = 0;
err:
	mtx_unlock(&sem_lock);
	fdrop(fp, td);
	DP(("<<< kern_sem_wait leaving, error = %d\n", error));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_getvalue_args {
	semid_t		id;
	int		*val;
};
#endif
int
ksem_getvalue(struct thread *td, struct ksem_getvalue_args *uap)
{
	struct file *fp;
	struct ksem *ks;
	int error, val;

	error = ksem_get(td, uap->id, &fp);
	if (error)
		return (error);
	ks = fp->f_data;

	mtx_lock(&sem_lock);
#ifdef MAC
	error = mac_check_posix_sem_getvalue(td->td_ucred, ks);
	if (error) {
		mtx_unlock(&sem_lock);
		fdrop(fp, td);
		return (error);
	}
#endif
	val = ks->ks_value;
	vfs_timestamp(&ks->ks_atime);
	mtx_unlock(&sem_lock);
	fdrop(fp, td);
	error = copyout(&val, uap->val, sizeof(val));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_destroy_args {
	semid_t		id;
};
#endif
int
ksem_destroy(struct thread *td, struct ksem_destroy_args *uap)
{
	struct file *fp;
	struct ksem *ks;
	int error;

	error = ksem_get(td, uap->id, &fp);
	if (error)
		return (error);
	ks = fp->f_data;
	if (!(ks->ks_flags & KS_ANONYMOUS)) {
		fdrop(fp, td);
		return (EINVAL);
	}
	mtx_lock(&sem_lock);
	if (ks->ks_waiters != 0) {
		mtx_unlock(&sem_lock);
		error = EBUSY;
		goto err;
	}
	ks->ks_flags |= KS_DEAD;
	mtx_unlock(&sem_lock);

	error = kern_close(td, uap->id);
err:
	fdrop(fp, td);
	return (error);
}

#define	SYSCALL_DATA(syscallname)				\
static int syscallname##_syscall = SYS_##syscallname;		\
static int syscallname##_registered;				\
static struct sysent syscallname##_old_sysent;			\
MAKE_SYSENT(syscallname);

#define	SYSCALL_REGISTER(syscallname) do {				\
	error = syscall_register(& syscallname##_syscall,		\
	    & syscallname##_sysent, & syscallname##_old_sysent);	\
	if (error)							\
		return (error);						\
	syscallname##_registered = 1;					\
} while(0)

#define	SYSCALL_DEREGISTER(syscallname) do {				\
	if (syscallname##_registered) {					\
		syscallname##_registered = 0;				\
		syscall_deregister(& syscallname##_syscall,		\
		    & syscallname##_old_sysent);			\
	}								\
} while(0)

SYSCALL_DATA(ksem_init);
SYSCALL_DATA(ksem_open);
SYSCALL_DATA(ksem_unlink);
SYSCALL_DATA(ksem_close);
SYSCALL_DATA(ksem_post);
SYSCALL_DATA(ksem_wait);
SYSCALL_DATA(ksem_timedwait);
SYSCALL_DATA(ksem_trywait);
SYSCALL_DATA(ksem_getvalue);
SYSCALL_DATA(ksem_destroy);

static int
ksem_module_init(void)
{
	int error;

	mtx_init(&sem_lock, "sem", NULL, MTX_DEF);
	mtx_init(&ksem_count_lock, "ksem count", NULL, MTX_DEF);
	sx_init(&ksem_dict_lock, "ksem dictionary");
	ksem_dictionary = hashinit(1024, M_KSEM, &ksem_hash);
	p31b_setcfg(CTL_P1003_1B_SEM_NSEMS_MAX, SEM_MAX);
	p31b_setcfg(CTL_P1003_1B_SEM_VALUE_MAX, SEM_VALUE_MAX);

	SYSCALL_REGISTER(ksem_init);
	SYSCALL_REGISTER(ksem_open);
	SYSCALL_REGISTER(ksem_unlink);
	SYSCALL_REGISTER(ksem_close);
	SYSCALL_REGISTER(ksem_post);
	SYSCALL_REGISTER(ksem_wait);
	SYSCALL_REGISTER(ksem_timedwait);
	SYSCALL_REGISTER(ksem_trywait);
	SYSCALL_REGISTER(ksem_getvalue);
	SYSCALL_REGISTER(ksem_destroy);
	return (0);
}

static void
ksem_module_destroy(void)
{

	SYSCALL_DEREGISTER(ksem_init);
	SYSCALL_DEREGISTER(ksem_open);
	SYSCALL_DEREGISTER(ksem_unlink);
	SYSCALL_DEREGISTER(ksem_close);
	SYSCALL_DEREGISTER(ksem_post);
	SYSCALL_DEREGISTER(ksem_wait);
	SYSCALL_DEREGISTER(ksem_timedwait);
	SYSCALL_DEREGISTER(ksem_trywait);
	SYSCALL_DEREGISTER(ksem_getvalue);
	SYSCALL_DEREGISTER(ksem_destroy);

	hashdestroy(ksem_dictionary, M_KSEM, ksem_hash);
	sx_destroy(&ksem_dict_lock);
	mtx_destroy(&ksem_count_lock);
	mtx_destroy(&sem_lock);
}

static int
sem_modload(struct module *module, int cmd, void *arg)
{
        int error = 0;

        switch (cmd) {
        case MOD_LOAD:
		error = ksem_module_init();
		if (error)
			ksem_module_destroy();
                break;

        case MOD_UNLOAD:
		mtx_lock(&ksem_count_lock);
		if (nsems != 0) {
			error = EOPNOTSUPP;
			mtx_unlock(&ksem_count_lock);
			break;
		}
		ksem_dead = 1;
		mtx_unlock(&ksem_count_lock);
		ksem_module_destroy();
                break;

        case MOD_SHUTDOWN:
                break;
        default:
                error = EINVAL;
                break;
        }
        return (error);
}

static moduledata_t sem_mod = {
        "sem",
        &sem_modload,
        NULL
};

DECLARE_MODULE(sem, sem_mod, SI_SUB_SYSV_SEM, SI_ORDER_FIRST);
MODULE_VERSION(sem, 1);
