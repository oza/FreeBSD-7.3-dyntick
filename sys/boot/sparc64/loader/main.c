/*-
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file.
 */
/*-
 * Copyright (c) 2008 Marius Strobl <marius@FreeBSD.org>
 * All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/boot/sparc64/loader/main.c,v 1.32.2.9.2.1 2010/02/10 00:26:20 kensmith Exp $");

/*
 * FreeBSD/sparc64 kernel loader - machine dependent part
 *
 *  - implements copyin and readin functions that map kernel
 *    pages on demand.  The machine independent code does not
 *    know the size of the kernel early enough to pre-enter
 *    TTEs and install just one 4MB mapping seemed to limiting
 *    to me.
 */

#include <stand.h>
#include <sys/exec.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/linker.h>
#include <sys/types.h>

#include <vm/vm.h>
#include <machine/asi.h>
#include <machine/cpufunc.h>
#include <machine/elf.h>
#include <machine/lsu.h>
#include <machine/metadata.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/upa.h>
#include <machine/ver.h>
#include <machine/vmparam.h>

#include "bootstrap.h"
#include "libofw.h"
#include "dev_net.h"

extern char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

enum {
	HEAPVA		= 0x800000,
	HEAPSZ		= 0x1000000,
	LOADSZ		= 0x1000000	/* for kernel and modules */
};

static struct mmu_ops {
	void (*tlb_init)(void);
	int (*mmu_mapin)(vm_offset_t va, vm_size_t len);
} *mmu_ops;

typedef void kernel_entry_t(vm_offset_t mdp, u_long o1, u_long o2, u_long o3,
    void *openfirmware);

static inline u_long dtlb_get_data_sun4u(int slot);
static void dtlb_enter_sun4u(u_long vpn, u_long data);
static vm_offset_t dtlb_va_to_pa_sun4u(vm_offset_t);
static inline u_long itlb_get_data_sun4u(int slot);
static void itlb_enter_sun4u(u_long vpn, u_long data);
static vm_offset_t itlb_va_to_pa_sun4u(vm_offset_t);
static void itlb_relocate_locked0_sun4u(void);
extern vm_offset_t md_load(char *, vm_offset_t *);
static int sparc64_autoload(void);
static ssize_t sparc64_readin(const int, vm_offset_t, const size_t);
static ssize_t sparc64_copyin(const void *, vm_offset_t, size_t);
static void sparc64_maphint(vm_offset_t, size_t);
static vm_offset_t claim_virt(vm_offset_t, size_t, int);
static vm_offset_t alloc_phys(size_t, int);
static int map_phys(int, size_t, vm_offset_t, vm_offset_t);
static void release_phys(vm_offset_t, u_int);
static int __elfN(exec)(struct preloaded_file *);
static int mmu_mapin_sun4u(vm_offset_t, vm_size_t);
static int mmu_mapin_sun4v(vm_offset_t, vm_size_t);
static vm_offset_t init_heap(void);
static void tlb_init_sun4u(void);
static void tlb_init_sun4v(void);

#ifdef LOADER_DEBUG
typedef u_int64_t tte_t;

static void pmap_print_tlb_sun4u(void);
static void pmap_print_tte_sun4u(tte_t, tte_t);
#endif

static struct mmu_ops mmu_ops_sun4u = { tlb_init_sun4u, mmu_mapin_sun4u };
static struct mmu_ops mmu_ops_sun4v = { tlb_init_sun4v, mmu_mapin_sun4v };

/* sun4u */
struct tlb_entry *dtlb_store;
struct tlb_entry *itlb_store;
int dtlb_slot;
int itlb_slot;
int cpu_impl;
static int dtlb_slot_max;
static int itlb_slot_max;

/* sun4v */
static struct tlb_entry *tlb_store;
static int is_sun4v = 0;
/*
 * no direct TLB access on sun4v
 * we somewhat arbitrarily declare enough
 * slots to cover a 4GB AS with 4MB pages
 */
#define	SUN4V_TLB_SLOT_MAX	(1 << 10)

static vm_offset_t curkva = 0;
static vm_offset_t heapva;

static phandle_t root;

/*
 * Machine dependent structures that the machine independent
 * loader part uses.
 */
struct devsw *devsw[] = {
#ifdef LOADER_DISK_SUPPORT
	&ofwdisk,
#endif
#ifdef LOADER_NET_SUPPORT
	&netdev,
#endif
	0
};
struct arch_switch archsw;

static struct file_format sparc64_elf = {
	__elfN(loadfile),
	__elfN(exec)
};
struct file_format *file_formats[] = {
	&sparc64_elf,
	0
};
struct fs_ops *file_system[] = {
#ifdef LOADER_UFS_SUPPORT
	&ufs_fsops,
#endif
#ifdef LOADER_CD9660_SUPPORT
	&cd9660_fsops,
#endif
#ifdef LOADER_ZIP_SUPPORT
	&zipfs_fsops,
#endif
#ifdef LOADER_GZIP_SUPPORT
	&gzipfs_fsops,
#endif
#ifdef LOADER_BZIP2_SUPPORT
	&bzipfs_fsops,
#endif
#ifdef LOADER_NFS_SUPPORT
	&nfs_fsops,
#endif
#ifdef LOADER_TFTP_SUPPORT
	&tftp_fsops,
#endif
	0
};
struct netif_driver *netif_drivers[] = {
#ifdef LOADER_NET_SUPPORT
	&ofwnet,
#endif
	0
};

extern struct console ofwconsole;
struct console *consoles[] = {
	&ofwconsole,
	0
};

#ifdef LOADER_DEBUG
static int
watch_phys_set_mask(vm_offset_t pa, u_long mask)
{
	u_long lsucr;

	stxa(AA_DMMU_PWPR, ASI_DMMU, pa & (((2UL << 38) - 1) << 3));
	lsucr = ldxa(0, ASI_LSU_CTL_REG);
	lsucr = ((lsucr | LSU_PW) & ~LSU_PM_MASK) |
	    (mask << LSU_PM_SHIFT);
	stxa(0, ASI_LSU_CTL_REG, lsucr);
	return (0);
}

static int
watch_phys_set(vm_offset_t pa, int sz)
{
	u_long off;

	off = (u_long)pa & 7;
	/* Test for misaligned watch points. */
	if (off + sz > 8)
		return (-1);
	return (watch_phys_set_mask(pa, ((1 << sz) - 1) << off));
}


static int
watch_virt_set_mask(vm_offset_t va, u_long mask)
{
	u_long lsucr;

	stxa(AA_DMMU_VWPR, ASI_DMMU, va & (((2UL << 41) - 1) << 3));
	lsucr = ldxa(0, ASI_LSU_CTL_REG);
	lsucr = ((lsucr | LSU_VW) & ~LSU_VM_MASK) |
	    (mask << LSU_VM_SHIFT);
	stxa(0, ASI_LSU_CTL_REG, lsucr);
	return (0);
}

static int
watch_virt_set(vm_offset_t va, int sz)
{
	u_long off;

	off = (u_long)va & 7;
	/* Test for misaligned watch points. */
	if (off + sz > 8)
		return (-1);
	return (watch_virt_set_mask(va, ((1 << sz) - 1) << off));
}
#endif

/*
 * archsw functions
 */
static int
sparc64_autoload(void)
{

	setenv("hw.ata.atapi_dma", "0", 0);
	return (0);
}

static ssize_t
sparc64_readin(const int fd, vm_offset_t va, const size_t len)
{

	mmu_ops->mmu_mapin(va, len);
	return (read(fd, (void *)va, len));
}

static ssize_t
sparc64_copyin(const void *src, vm_offset_t dest, size_t len)
{

	mmu_ops->mmu_mapin(dest, len);
	memcpy((void *)dest, src, len);
	return (len);
}

static void
sparc64_maphint(vm_offset_t va, size_t len)
{
	vm_paddr_t pa;
	vm_offset_t mva;
	size_t size;
	int i, free_excess = 0;

	if (!is_sun4v)
		return;

	if (tlb_store[va >> 22].te_pa != -1)
		return;

	/* round up to nearest 4MB page */
	size = (len + PAGE_MASK_4M) & ~PAGE_MASK_4M;
#if 0
	pa = alloc_phys(PAGE_SIZE_256M, PAGE_SIZE_256M);

	if (pa != -1)
		free_excess = 1;
	else
#endif
		pa = alloc_phys(size, PAGE_SIZE_256M);
	if (pa == -1)
		pa = alloc_phys(size, PAGE_SIZE_4M);
	if (pa == -1)
		panic("%s: out of memory", __func__);

	for (i = 0; i < size; i += PAGE_SIZE_4M) {
		mva = claim_virt(va + i, PAGE_SIZE_4M, 0);
		if (mva != (va + i))
			panic("%s: can't claim virtual page "
			    "(wanted %#lx, got %#lx)",
			    __func__, va, mva);

		tlb_store[mva >> 22].te_pa = pa + i;
		if (map_phys(-1, PAGE_SIZE_4M, mva, pa + i) != 0)
			printf("%s: can't map physical page\n", __func__);
	}
	if (free_excess)
		release_phys(pa, PAGE_SIZE_256M);
}

/*
 * other MD functions
 */
static vm_offset_t
claim_virt(vm_offset_t virt, size_t size, int align)
{
	vm_offset_t mva;

	if (OF_call_method("claim", mmu, 3, 1, virt, size, align, &mva) == -1)
		return ((vm_offset_t)-1);
	return (mva);
}

static vm_offset_t
alloc_phys(size_t size, int align)
{
	cell_t phys_hi, phys_low;

	if (OF_call_method("claim", memory, 2, 2, size, align, &phys_low,
	    &phys_hi) == -1)
		return ((vm_offset_t)-1);
	return ((vm_offset_t)phys_hi << 32 | phys_low);
}

static int
map_phys(int mode, size_t size, vm_offset_t virt, vm_offset_t phys)
{

	return (OF_call_method("map", mmu, 5, 0, (uint32_t)phys,
	    (uint32_t)(phys >> 32), virt, size, mode));
}

static void
release_phys(vm_offset_t phys, u_int size)
{

	(void)OF_call_method("release", memory, 3, 0, (uint32_t)phys,
	    (uint32_t)(phys >> 32), size);
}

static int
__elfN(exec)(struct preloaded_file *fp)
{
	struct file_metadata *fmp;
	vm_offset_t mdp;
	Elf_Addr entry;
	Elf_Ehdr *e;
	int error;

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == 0)
		return (EFTYPE);
	e = (Elf_Ehdr *)&fmp->md_data;

	if ((error = md_load(fp->f_args, &mdp)) != 0)
		return (error);

	printf("jumping to kernel entry at %#lx.\n", e->e_entry);
#ifdef LOADER_DEBUG
	pmap_print_tlb_sun4u();
#endif

	dev_cleanup();

	entry = e->e_entry;

	OF_release((void *)heapva, HEAPSZ);

	((kernel_entry_t *)entry)(mdp, 0, 0, 0, openfirmware);

	panic("%s: exec returned", __func__);
}

static inline u_long
dtlb_get_data_sun4u(int slot)
{

	/*
	 * We read ASI_DTLB_DATA_ACCESS_REG twice in order to work
	 * around errata of USIII and beyond.
	 */
	(void)ldxa(TLB_DAR_SLOT(slot), ASI_DTLB_DATA_ACCESS_REG);
	return (ldxa(TLB_DAR_SLOT(slot), ASI_DTLB_DATA_ACCESS_REG));
}

static inline u_long
itlb_get_data_sun4u(int slot)
{

	/*
	 * We read ASI_ITLB_DATA_ACCESS_REG twice in order to work
	 * around errata of USIII and beyond.
	 */
	(void)ldxa(TLB_DAR_SLOT(slot), ASI_ITLB_DATA_ACCESS_REG);
	return (ldxa(TLB_DAR_SLOT(slot), ASI_ITLB_DATA_ACCESS_REG));
}

static vm_offset_t
dtlb_va_to_pa_sun4u(vm_offset_t va)
{
	u_long pstate, reg;
	int i;

	pstate = rdpr(pstate);
	wrpr(pstate, pstate & ~PSTATE_IE, 0);
	for (i = 0; i < dtlb_slot_max; i++) {
		reg = ldxa(TLB_DAR_SLOT(i), ASI_DTLB_TAG_READ_REG);
		if (TLB_TAR_VA(reg) != va)
			continue;
		reg = dtlb_get_data_sun4u(i);
		wrpr(pstate, pstate, 0);
		reg >>= TD_PA_SHIFT;
		if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
			return (reg & TD_PA_CH_MASK);
		return (reg & TD_PA_SF_MASK);
	}
	wrpr(pstate, pstate, 0);
	return (-1);
}

static vm_offset_t
itlb_va_to_pa_sun4u(vm_offset_t va)
{
	u_long pstate, reg;
	int i;

	pstate = rdpr(pstate);
	wrpr(pstate, pstate & ~PSTATE_IE, 0);
	for (i = 0; i < itlb_slot_max; i++) {
		reg = ldxa(TLB_DAR_SLOT(i), ASI_ITLB_TAG_READ_REG);
		if (TLB_TAR_VA(reg) != va)
			continue;
		reg = itlb_get_data_sun4u(i);
		wrpr(pstate, pstate, 0);
		reg >>= TD_PA_SHIFT;
		if (cpu_impl >= CPU_IMPL_ULTRASPARCIII)
			return (reg & TD_PA_CH_MASK);
		return (reg & TD_PA_SF_MASK);
	}
	wrpr(pstate, pstate, 0);
	return (-1);
}

static void
dtlb_enter_sun4u(u_long vpn, u_long data)
{
	u_long reg;

	reg = rdpr(pstate);
	wrpr(pstate, reg & ~PSTATE_IE, 0);
	stxa(AA_DMMU_TAR, ASI_DMMU,
	    TLB_TAR_VA(vpn) | TLB_TAR_CTX(TLB_CTX_KERNEL));
	stxa(0, ASI_DTLB_DATA_IN_REG, data);
	membar(Sync);
	wrpr(pstate, reg, 0);
}

static void
itlb_enter_sun4u(u_long vpn, u_long data)
{
	u_long reg;
	int i;

	reg = rdpr(pstate);
	wrpr(pstate, reg & ~PSTATE_IE, 0);

	if (cpu_impl == CPU_IMPL_ULTRASPARCIIIp) {
		/*
		 * Search an unused slot != 0 and explicitly enter the data
		 * and tag there in order to avoid Cheetah+ erratum 34.
		 */
		for (i = 1; i < itlb_slot_max; i++) {
			if ((itlb_get_data_sun4u(i) & TD_V) != 0)
				continue;

			stxa(AA_IMMU_TAR, ASI_IMMU,
			    TLB_TAR_VA(vpn) | TLB_TAR_CTX(TLB_CTX_KERNEL));
			stxa(TLB_DAR_SLOT(i), ASI_ITLB_DATA_ACCESS_REG, data);
			flush(PROMBASE);
			break;
		}
		wrpr(pstate, reg, 0);
		if (i == itlb_slot_max)
			panic("%s: could not find an unused slot", __func__);
		return;
	}

	stxa(AA_IMMU_TAR, ASI_IMMU,
	    TLB_TAR_VA(vpn) | TLB_TAR_CTX(TLB_CTX_KERNEL));
	stxa(0, ASI_ITLB_DATA_IN_REG, data);
	flush(PROMBASE);
	wrpr(pstate, reg, 0);
}

static void
itlb_relocate_locked0_sun4u(void)
{
	u_long data, pstate, tag;
	int i;

	if (cpu_impl != CPU_IMPL_ULTRASPARCIIIp)
		return;

	pstate = rdpr(pstate);
	wrpr(pstate, pstate & ~PSTATE_IE, 0);

	data = itlb_get_data_sun4u(0);
	if ((data & (TD_V | TD_L)) != (TD_V | TD_L)) {
		wrpr(pstate, pstate, 0);
		return;
	}

	/* Flush the mapping of slot 0. */
	tag = ldxa(TLB_DAR_SLOT(0), ASI_ITLB_TAG_READ_REG);
	stxa(TLB_DEMAP_VA(TLB_TAR_VA(tag)) | TLB_DEMAP_PRIMARY |
	    TLB_DEMAP_PAGE, ASI_IMMU_DEMAP, 0);
	flush(0);	/* The USIII-family ignores the address. */

	/*
	 * Search a replacement slot != 0 and enter the data and tag
	 * that formerly were in slot 0.
	 */
	for (i = 1; i < itlb_slot_max; i++) {
		if ((itlb_get_data_sun4u(i) & TD_V) != 0)
			continue;

		stxa(AA_IMMU_TAR, ASI_IMMU, tag);
		stxa(TLB_DAR_SLOT(i), ASI_ITLB_DATA_ACCESS_REG, data);
		flush(0);	/* The USIII-family ignores the address. */
		break;
	}
	wrpr(pstate, pstate, 0);
	if (i == itlb_slot_max)
		panic("%s: could not find a replacement slot", __func__);
}

static int
mmu_mapin_sun4u(vm_offset_t va, vm_size_t len)
{
	vm_offset_t pa, mva;
	u_long data;

	if (va + len > curkva)
		curkva = va + len;

	pa = (vm_offset_t)-1;
	len += va & PAGE_MASK_4M;
	va &= ~PAGE_MASK_4M;
	while (len) {
		if (dtlb_va_to_pa_sun4u(va) == (vm_offset_t)-1 ||
		    itlb_va_to_pa_sun4u(va) == (vm_offset_t)-1) {
			/* Allocate a physical page, claim the virtual area. */
			if (pa == (vm_offset_t)-1) {
				pa = alloc_phys(PAGE_SIZE_4M, PAGE_SIZE_4M);
				if (pa == (vm_offset_t)-1)
					panic("%s: out of memory", __func__);
				mva = claim_virt(va, PAGE_SIZE_4M, 0);
				if (mva != va)
					panic("%s: can't claim virtual page "
					    "(wanted %#lx, got %#lx)",
					    __func__, va, mva);
				/*
				 * The mappings may have changed, be paranoid.
				 */
				continue;
			}
			/*
			 * Actually, we can only allocate two pages less at
			 * most (depending on the kernel TSB size).
			 */
			if (dtlb_slot >= dtlb_slot_max)
				panic("%s: out of dtlb_slots", __func__);
			if (itlb_slot >= itlb_slot_max)
				panic("%s: out of itlb_slots", __func__);
			data = TD_V | TD_4M | TD_PA(pa) | TD_L | TD_CP |
			    TD_CV | TD_P | TD_W;
			dtlb_store[dtlb_slot].te_pa = pa;
			dtlb_store[dtlb_slot].te_va = va;
			itlb_store[itlb_slot].te_pa = pa;
			itlb_store[itlb_slot].te_va = va;
			dtlb_slot++;
			itlb_slot++;
			dtlb_enter_sun4u(va, data);
			itlb_enter_sun4u(va, data);
			pa = (vm_offset_t)-1;
		}
		len -= len > PAGE_SIZE_4M ? PAGE_SIZE_4M : len;
		va += PAGE_SIZE_4M;
	}
	if (pa != (vm_offset_t)-1)
		release_phys(pa, PAGE_SIZE_4M);
	return (0);
}

static int
mmu_mapin_sun4v(vm_offset_t va, vm_size_t len)
{
	vm_offset_t pa, mva;

	if (va + len > curkva)
		curkva = va + len;

	pa = (vm_offset_t)-1;
	len += va & PAGE_MASK_4M;
	va &= ~PAGE_MASK_4M;
	while (len) {
		if ((va >> 22) > SUN4V_TLB_SLOT_MAX)
			panic("%s: trying to map more than 4GB", __func__);
		if (tlb_store[va >> 22].te_pa == -1) {
			/* Allocate a physical page, claim the virtual area */
			if (pa == (vm_offset_t)-1) {
				pa = alloc_phys(PAGE_SIZE_4M, PAGE_SIZE_4M);
				if (pa == (vm_offset_t)-1)
				    panic("%s: out of memory", __func__);
				mva = claim_virt(va, PAGE_SIZE_4M, 0);
				if (mva != va)
					panic("%s: can't claim virtual page "
					    "(wanted %#lx, got %#lx)",
					    __func__, va, mva);
			}

			tlb_store[va >> 22].te_pa = pa;
			if (map_phys(-1, PAGE_SIZE_4M, va, pa) == -1)
				printf("%s: can't map physical page\n",
				    __func__);
			pa = (vm_offset_t)-1;
		}
		len -= len > PAGE_SIZE_4M ? PAGE_SIZE_4M : len;
		va += PAGE_SIZE_4M;
	}
	if (pa != (vm_offset_t)-1)
		release_phys(pa, PAGE_SIZE_4M);
	return (0);
}

static vm_offset_t
init_heap(void)
{

	/* There is no need for continuous physical heap memory. */
	heapva = (vm_offset_t)OF_claim((void *)HEAPVA, HEAPSZ, 32);
	return (heapva);
}

static void
tlb_init_sun4u(void)
{
	phandle_t child;
	char buf[128];
	u_int bootcpu;
	u_int cpu;

	cpu_impl = VER_IMPL(rdpr(ver));
	bootcpu = UPA_CR_GET_MID(ldxa(0, ASI_UPA_CONFIG_REG));
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (OF_getprop(child, "device_type", buf, sizeof(buf)) <= 0)
			continue;
		if (strcmp(buf, "cpu") != 0)
			continue;
		if (OF_getprop(child, cpu_impl < CPU_IMPL_ULTRASPARCIII ?
		    "upa-portid" : "portid", &cpu, sizeof(cpu)) <= 0)
			continue;
		if (cpu == bootcpu)
			break;
	}
	if (cpu != bootcpu)
		panic("%s: no node for bootcpu?!?!", __func__);

	if (OF_getprop(child, "#dtlb-entries", &dtlb_slot_max,
	    sizeof(dtlb_slot_max)) == -1 ||
	    OF_getprop(child, "#itlb-entries", &itlb_slot_max,
	    sizeof(itlb_slot_max)) == -1)
		panic("%s: can't get TLB slot max.", __func__);

	if (cpu_impl == CPU_IMPL_ULTRASPARCIIIp) {
#ifdef LOADER_DEBUG
		printf("pre fixup:\n");
		pmap_print_tlb_sun4u();
#endif

		/*
		 * Relocate the locked entry in it16 slot 0 (if existent)
		 * as part of working around Cheetah+ erratum 34.
		 */
		itlb_relocate_locked0_sun4u();

#ifdef LOADER_DEBUG
		printf("post fixup:\n");
		pmap_print_tlb_sun4u();
#endif
	}

	dtlb_store = malloc(dtlb_slot_max * sizeof(*dtlb_store));
	itlb_store = malloc(itlb_slot_max * sizeof(*itlb_store));
	if (dtlb_store == NULL || itlb_store == NULL)
		panic("%s: can't allocate TLB store", __func__);
}

static void
tlb_init_sun4v(void)
{

	tlb_store = malloc(SUN4V_TLB_SLOT_MAX * sizeof(*tlb_store));
	memset(tlb_store, 0xFF, SUN4V_TLB_SLOT_MAX * sizeof(*tlb_store));
}

int
main(int (*openfirm)(void *))
{
	char bootpath[64];
	char compatible[32];
	struct devsw **dp;

	/*
	 * Tell the Open Firmware functions where they find the OFW gate.
	 */
	OF_init(openfirm);

	archsw.arch_getdev = ofw_getdev;
	archsw.arch_copyin = sparc64_copyin;
	archsw.arch_copyout = ofw_copyout;
	archsw.arch_readin = sparc64_readin;
	archsw.arch_autoload = sparc64_autoload;
	archsw.arch_maphint = sparc64_maphint;

	init_heap();
	setheap((void *)heapva, (void *)(heapva + HEAPSZ));

	/*
	 * Probe for a console.
	 */
	cons_probe();

	if ((root = OF_peer(0)) == -1)
		panic("%s: can't get root phandle", __func__);
	OF_getprop(root, "compatible", compatible, sizeof(compatible));
	if (!strcmp(compatible, "sun4v")) {
		printf("\nBooting with sun4v support.\n");
		mmu_ops = &mmu_ops_sun4v;
		is_sun4v = 1;
	} else {
		printf("\nBooting with sun4u support.\n");
		mmu_ops = &mmu_ops_sun4u;
	}

	mmu_ops->tlb_init();

	/*
	 * Initialize devices.
	 */
	for (dp = devsw; *dp != 0; dp++) {
		if ((*dp)->dv_init != 0)
			(*dp)->dv_init();
	}

	/*
	 * Set up the current device.
	 */
	OF_getprop(chosen, "bootpath", bootpath, sizeof(bootpath));

	/*
	 * Sun compatible bootable CD-ROMs have a disk label placed
	 * before the cd9660 data, with the actual filesystem being
	 * in the first partition, while the other partitions contain
	 * pseudo disk labels with embedded boot blocks for different
	 * architectures, which may be followed by UFS filesystems.
	 * The firmware will set the boot path to the partition it
	 * boots from ('f' in the sun4u case), but we want the kernel
	 * to be loaded from the cd9660 fs ('a'), so the boot path
	 * needs to be altered.
	 */
	if (bootpath[strlen(bootpath) - 2] == ':' &&
	    bootpath[strlen(bootpath) - 1] == 'f') {
		bootpath[strlen(bootpath) - 1] = 'a';
		printf("Boot path set to %s\n", bootpath);
	}

	env_setenv("currdev", EV_VOLATILE, bootpath,
	    ofw_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, bootpath,
	    env_noset, env_nounset);

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
	printf("bootpath=\"%s\"\n", bootpath);

	/* Give control to the machine independent loader code. */
	interact();
	return (1);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	printf("Rebooting...\n");
	OF_exit();
}

/* provide this for panic, as it's not in the startup code */
void
exit(int code)
{

	OF_exit();
}

#ifdef LOADER_DEBUG
static const char *const page_sizes[] = {
	"  8k", " 64k", "512k", "  4m"
};

static void
pmap_print_tte_sun4u(tte_t tag, tte_t tte)
{

	printf("%s %s ",
	    page_sizes[(tte >> TD_SIZE_SHIFT) & TD_SIZE_MASK],
	    tag & TD_G ? "G" : " ");
	printf(tte & TD_W ? "W " : "  ");
	printf(tte & TD_P ? "\e[33mP\e[0m " : "  ");
	printf(tte & TD_E ? "E " : "  ");
	printf(tte & TD_CV ? "CV " : "   ");
	printf(tte & TD_CP ? "CP " : "   ");
	printf(tte & TD_L ? "\e[32mL\e[0m " : "  ");
	printf(tte & TD_IE ? "IE " : "   ");
	printf(tte & TD_NFO ? "NFO " : "    ");
	printf("pa=0x%lx va=0x%lx ctx=%ld\n",
	    TD_PA(tte), TLB_TAR_VA(tag), TLB_TAR_CTX(tag));
}

static void
pmap_print_tlb_sun4u(void)
{
	tte_t tag, tte;
	u_long pstate;
	int i;

	pstate = rdpr(pstate);
	for (i = 0; i < itlb_slot_max; i++) {
		wrpr(pstate, pstate & ~PSTATE_IE, 0);
		tte = itlb_get_data_sun4u(i);
		wrpr(pstate, pstate, 0);
		if (!(tte & TD_V))
			continue;
		tag = ldxa(TLB_DAR_SLOT(i), ASI_ITLB_TAG_READ_REG);
		printf("iTLB-%2u: ", i);
		pmap_print_tte_sun4u(tag, tte);
	}
	for (i = 0; i < dtlb_slot_max; i++) {
		wrpr(pstate, pstate & ~PSTATE_IE, 0);
		tte = dtlb_get_data_sun4u(i);
		wrpr(pstate, pstate, 0);
		if (!(tte & TD_V))
			continue;
		tag = ldxa(TLB_DAR_SLOT(i), ASI_DTLB_TAG_READ_REG);
		printf("dTLB-%2u: ", i);
		pmap_print_tte_sun4u(tag, tte);
	}
}
#endif
