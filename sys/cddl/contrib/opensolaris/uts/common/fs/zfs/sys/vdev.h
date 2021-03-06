/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_VDEV_H
#define	_SYS_VDEV_H

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/dmu.h>
#include <sys/space_map.h>
#include <sys/fs/zfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern boolean_t zfs_nocacheflush;

extern int vdev_open(vdev_t *);
extern int vdev_validate(vdev_t *);
extern void vdev_close(vdev_t *);
extern int vdev_create(vdev_t *, uint64_t txg, boolean_t isreplace);
extern void vdev_init(vdev_t *, uint64_t txg);
extern void vdev_reopen(vdev_t *);
extern int vdev_validate_aux(vdev_t *vd);
extern zio_t *vdev_probe(vdev_t *vd, zio_t *pio);

extern boolean_t vdev_is_bootable(vdev_t *vd);
extern vdev_t *vdev_lookup_top(spa_t *spa, uint64_t vdev);
extern vdev_t *vdev_lookup_by_guid(vdev_t *vd, uint64_t guid);
extern void vdev_dtl_dirty(space_map_t *sm, uint64_t txg, uint64_t size);
extern int vdev_dtl_contains(space_map_t *sm, uint64_t txg, uint64_t size);
extern void vdev_dtl_reassess(vdev_t *vd, uint64_t txg, uint64_t scrub_txg,
    int scrub_done);
extern boolean_t vdev_resilver_needed(vdev_t *vd,
    uint64_t *minp, uint64_t *maxp);

extern int vdev_metaslab_init(vdev_t *vd, uint64_t txg);
extern void vdev_metaslab_fini(vdev_t *vd);

extern void vdev_get_stats(vdev_t *vd, vdev_stat_t *vs);
extern void vdev_clear_stats(vdev_t *vd);
extern void vdev_stat_update(zio_t *zio, uint64_t psize);
extern void vdev_scrub_stat_update(vdev_t *vd, pool_scrub_type_t type,
    boolean_t complete);
extern int vdev_getspec(spa_t *spa, uint64_t vdev, char **vdev_spec);
extern void vdev_propagate_state(vdev_t *vd);
extern void vdev_set_state(vdev_t *vd, boolean_t isopen, vdev_state_t state,
    vdev_aux_t aux);

extern void vdev_space_update(vdev_t *vd, int64_t space_delta,
    int64_t alloc_delta, boolean_t update_root);

extern uint64_t vdev_psize_to_asize(vdev_t *vd, uint64_t psize);

extern int vdev_fault(spa_t *spa, uint64_t guid);
extern int vdev_degrade(spa_t *spa, uint64_t guid);
extern int vdev_online(spa_t *spa, uint64_t guid, uint64_t flags,
    vdev_state_t *);
extern int vdev_offline(spa_t *spa, uint64_t guid, uint64_t flags);
extern void vdev_clear(spa_t *spa, vdev_t *vd);

extern boolean_t vdev_is_dead(vdev_t *vd);
extern boolean_t vdev_readable(vdev_t *vd);
extern boolean_t vdev_writeable(vdev_t *vd);
extern boolean_t vdev_accessible(vdev_t *vd, zio_t *zio);

extern void vdev_cache_init(vdev_t *vd);
extern void vdev_cache_fini(vdev_t *vd);
extern int vdev_cache_read(zio_t *zio);
extern void vdev_cache_write(zio_t *zio);
extern void vdev_cache_purge(vdev_t *vd);

extern void vdev_queue_init(vdev_t *vd);
extern void vdev_queue_fini(vdev_t *vd);
extern zio_t *vdev_queue_io(zio_t *zio);
extern void vdev_queue_io_done(zio_t *zio);

extern void vdev_config_dirty(vdev_t *vd);
extern void vdev_config_clean(vdev_t *vd);
extern int vdev_config_sync(vdev_t **svd, int svdcount, uint64_t txg);

extern void vdev_state_dirty(vdev_t *vd);
extern void vdev_state_clean(vdev_t *vd);

extern nvlist_t *vdev_config_generate(spa_t *spa, vdev_t *vd,
    boolean_t getstats, boolean_t isspare, boolean_t isl2cache);

/*
 * Label routines
 */
struct uberblock;
extern uint64_t vdev_label_offset(uint64_t psize, int l, uint64_t offset);
extern int vdev_label_number(uint64_t psise, uint64_t offset);
extern nvlist_t *vdev_label_read_config(vdev_t *vd);
extern void vdev_uberblock_load(zio_t *zio, vdev_t *vd, struct uberblock *ub);

typedef enum {
	VDEV_LABEL_CREATE,	/* create/add a new device */
	VDEV_LABEL_REPLACE,	/* replace an existing device */
	VDEV_LABEL_SPARE,	/* add a new hot spare */
	VDEV_LABEL_REMOVE,	/* remove an existing device */
	VDEV_LABEL_L2CACHE	/* add an L2ARC cache device */
} vdev_labeltype_t;

extern int vdev_label_init(vdev_t *vd, uint64_t txg, vdev_labeltype_t reason);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VDEV_H */
