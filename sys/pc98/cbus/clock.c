/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/pc98/cbus/clock.c,v 1.162.10.1 2010/02/10 00:26:20 kensmith Exp $");

/*
 * Routines to handle clock hardware.
 */

/*
 * inittodr, settodr and support routines written
 * by Christoph Robitschko <chmr@edvz.tu-graz.ac.at>
 *
 * reintroduced and updated by Chris Stenton <chris@gnome.co.uk> 8/10/94
 */

/*
 * modified for PC98 by Kakefuda
 */

#include "opt_apic.h"
#include "opt_clock.h"
#include "opt_isa.h"
#include "opt_mca.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/cons.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#ifdef DEV_APIC
#include <machine/apicvar.h>
#endif
#include <machine/specialreg.h>
#include <machine/ppireg.h>
#include <machine/timerreg.h>

#include <i386/isa/icu.h>
#include <pc98/cbus/cbus.h>
#include <pc98/pc98/pc98_machdep.h>
#ifdef DEV_ISA
#include <isa/isavar.h>
#endif

#define	TIMER_DIV(x) ((timer_freq + (x) / 2) / (x))

int	clkintr_pending;
int	pscnt = 1;
int	psdiv = 1;
int	statclock_disable;
#ifndef TIMER_FREQ
#define TIMER_FREQ   2457600
#endif
u_int	timer_freq = TIMER_FREQ;
int	timer0_max_count;
int	timer0_real_max_count;

static	int	beeping = 0;
static	struct mtx clock_lock;
static	struct intsrc *i8254_intsrc;
static	u_int32_t i8254_lastcount;
static	u_int32_t i8254_offset;
static	int	(*i8254_pending)(struct intsrc *);
static	int	i8254_ticked;
static	int	using_lapic_timer;

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer1_state;
static	u_char	timer2_state;
static void rtc_serialcombit(int);
static void rtc_serialcom(int);
static int rtc_inb(void);
static void rtc_outb(int);

static	unsigned i8254_get_timecount(struct timecounter *tc);
static	unsigned i8254_simple_get_timecount(struct timecounter *tc);
static	void	set_timer_freq(u_int freq, int intr_freq);

static struct timecounter i8254_timecounter = {
	i8254_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"i8254",		/* name */
	0			/* quality */
};

static int
clkintr(struct trapframe *frame)
{

	if (timecounter->tc_get_timecount == i8254_get_timecount) {
		mtx_lock_spin(&clock_lock);
		if (i8254_ticked)
			i8254_ticked = 0;
		else {
			i8254_offset += timer0_max_count;
			i8254_lastcount = 0;
		}
		clkintr_pending = 0;
		mtx_unlock_spin(&clock_lock);
	}
	KASSERT(!using_lapic_timer, ("clk interrupt enabled with lapic timer"));
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	return (FILTER_HANDLED);
}

int
acquire_timer1(int mode)
{

	if (timer1_state != RELEASED)
		return (-1);
	timer1_state = ACQUIRED;

	/*
	 * This access to the timer registers is as atomic as possible
	 * because it is a single instruction.  We could do better if we
	 * knew the rate.  Use of splclock() limits glitches to 10-100us,
	 * and this is probably good enough for timer2, so we aren't as
	 * careful with it as with timer0.
	 */
	outb(TIMER_MODE, TIMER_SEL1 | (mode & 0x3f));

	return (0);
}

int
acquire_timer2(int mode)
{

	if (timer2_state != RELEASED)
		return (-1);
	timer2_state = ACQUIRED;

	/*
	 * This access to the timer registers is as atomic as possible
	 * because it is a single instruction.  We could do better if we
	 * knew the rate.  Use of splclock() limits glitches to 10-100us,
	 * and this is probably good enough for timer2, so we aren't as
	 * careful with it as with timer0.
	 */
	outb(TIMER_MODE, TIMER_SEL2 | (mode & 0x3f));

	return (0);
}

int
release_timer1()
{

	if (timer1_state != ACQUIRED)
		return (-1);
	timer1_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL1 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}

int
release_timer2()
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
	return (0);
}


static int
getit(void)
{
	int high, low;

	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);

	mtx_unlock_spin(&clock_lock);
	return ((high << 8) | low);
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (timer_freq / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int delta, prev_tick, tick, ticks_left;

#ifdef DELAYDEBUG
	int getit_calls = 1;
	int n1;
	static int state = 0;

	if (state == 0) {
		state = 1;
		for (n1 = 1; n1 <= 10000000; n1 *= 10)
			DELAY(n1);
		state = 2;
	}
	if (state == 1)
		printf("DELAY(%d)...", n);
#endif
	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 *
	 * However, if ddb is active then use a fake counter since reading
	 * the i8254 counter involves acquiring a lock.  ddb must not do
	 * locking for many reasons, but it calls here for at least atkbd
	 * input.
	 */
#ifdef KDB
	if (kdb_active)
		prev_tick = 1;
	else
#endif
		prev_tick = getit();
	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (timer_freq / 1e6)) without using floating point
	 * and without any avoidable overflows.
	 */
	if (n <= 0)
		ticks_left = 0;
	else if (n < 256)
		/*
		 * Use fixed point to avoid a slow division by 1000000.
		 * 39099 = 1193182 * 2^15 / 10^6 rounded to nearest.
		 * 2^15 is the first power of 2 that gives exact results
		 * for n between 0 and 256.
		 */
		ticks_left = ((u_int)n * 39099 + (1 << 15) - 1) >> 15;
	else
		/*
		 * Don't bother using fixed point, although gcc-2.7.2
		 * generates particularly poor code for the long long
		 * division, since even the slow way will complete long
		 * before the delay is up (unless we're interrupted).
		 */
		ticks_left = ((u_int)n * (long long)timer_freq + 999999)
			     / 1000000;

	while (ticks_left > 0) {
#ifdef KDB
		if (kdb_active) {
			outb(0x5f, 0);
			tick = prev_tick - 1;
			if (tick <= 0)
				tick = timer0_max_count;
		} else
#endif
			tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = prev_tick - tick;
		prev_tick = tick;
		if (delta < 0) {
			delta += timer0_max_count;
			/*
			 * Guard against timer0_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_timer_freq() is
			 * traced.
			 */
			if (delta < 0)
				delta = 0;
		}
		ticks_left -= delta;
	}
#ifdef DELAYDEBUG
	if (state == 1)
		printf(" %d calls to getit() at %d usec each\n",
		       getit_calls, (n + 5) / getit_calls);
#endif
}

static void
sysbeepstop(void *chan)
{
	ppi_spkr_off();		/* disable counter1 output to speaker */
	timer_spkr_release();
	beeping = 0;
}

int
sysbeep(int pitch, int period)
{
	int x = splclock();

	if (timer_spkr_acquire())
		if (!beeping) {
			/* Something else owns it. */
			splx(x);
			return (-1); /* XXX Should be EBUSY, but nobody cares anyway. */
		}
	mtx_lock_spin(&clock_lock);
	spkr_set_pitch(pitch);
	mtx_unlock_spin(&clock_lock);
	if (!beeping) {
		/* enable counter1 output to speaker */
		ppi_spkr_on();
		beeping = period;
		timeout(sysbeepstop, (void *)NULL, period);
	}
	splx(x);
	return (0);
}

static u_int
calibrate_clocks(void)
{
	int timeout;
	u_int count, prev_count, tot_count;
	u_short	sec, start_sec;

	if (bootverbose)
	        printf("Calibrating clock(s) ... ");
	/* Check ARTIC. */
	if (!(PC98_SYSTEM_PARAMETER(0x458) & 0x80) &&
	    !(PC98_SYSTEM_PARAMETER(0x45b) & 0x04))
		goto fail;
	timeout = 100000000;

	/* Read the ARTIC. */
	sec = inw(0x5e);

	/* Wait for the ARTIC to changes. */
	start_sec = sec;
	for (;;) {
		sec = inw(0x5e);
		if (sec != start_sec)
			break;
		if (--timeout == 0)
			goto fail;
	}

	/* Start keeping track of the i8254 counter. */
	prev_count = getit();
	if (prev_count == 0 || prev_count > timer0_max_count)
		goto fail;
	tot_count = 0;

	start_sec = sec;
	for (;;) {
		sec = inw(0x5e);
		count = getit();
		if (count == 0 || count > timer0_max_count)
			goto fail;
		if (count > prev_count)
			tot_count += prev_count - (count - timer0_max_count);
		else
			tot_count += prev_count - count;
		prev_count = count;
		if ((sec == start_sec + 1200) || /* 1200 = 307.2KHz >> 8 */
		    (sec < start_sec &&
		        (u_int)sec + 0x10000 == (u_int)start_sec + 1200))
			break;
		if (--timeout == 0)
			goto fail;
	}

	if (bootverbose) {
	        printf("i8254 clock: %u Hz\n", tot_count);
	}
	return (tot_count);

fail:
	if (bootverbose)
	        printf("failed, using default i8254 clock of %u Hz\n",
		       timer_freq);
	return (timer_freq);
}

static void
set_timer_freq(u_int freq, int intr_freq)
{
	int new_timer0_real_max_count;

	i8254_timecounter.tc_frequency = freq;
	mtx_lock_spin(&clock_lock);
	timer_freq = freq;
	if (using_lapic_timer)
		new_timer0_real_max_count = 0x10000;
	else
		new_timer0_real_max_count = TIMER_DIV(intr_freq);
	if (new_timer0_real_max_count != timer0_real_max_count) {
		timer0_real_max_count = new_timer0_real_max_count;
		if (timer0_real_max_count == 0x10000)
			timer0_max_count = 0xffff;
		else
			timer0_max_count = timer0_real_max_count;
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, timer0_real_max_count & 0xff);
		outb(TIMER_CNTR0, timer0_real_max_count >> 8);
	}
	mtx_unlock_spin(&clock_lock);
}

static void
i8254_restore(void)
{

	mtx_lock_spin(&clock_lock);
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb(TIMER_CNTR0, timer0_real_max_count & 0xff);
	outb(TIMER_CNTR0, timer0_real_max_count >> 8);
	mtx_unlock_spin(&clock_lock);
}


/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 */
void
timer_restore(void)
{

	i8254_restore();		/* restore timer_freq and hz */
}

/* This is separate from startrtclock() so that it can be called early. */
void
i8254_init(void)
{

	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_NOPROFILE);

	if (pc98_machine_type & M_8M)
		timer_freq = 1996800L; /* 1.9968 MHz */
	else
		timer_freq = 2457600L; /* 2.4576 MHz */

	set_timer_freq(timer_freq, hz);
}

void
startrtclock()
{
	u_int delta, freq;

	freq = calibrate_clocks();
#ifdef CLK_CALIBRATION_LOOP
	if (bootverbose) {
		printf(
		"Press a key on the console to abort clock calibration\n");
		while (cncheckc() == -1)
			calibrate_clocks();
	}
#endif

	/*
	 * Use the calibrated i8254 frequency if it seems reasonable.
	 * Otherwise use the default, and don't use the calibrated i586
	 * frequency.
	 */
	delta = freq > timer_freq ? freq - timer_freq : timer_freq - freq;
	if (delta < timer_freq / 100) {
#ifndef CLK_USE_I8254_CALIBRATION
		if (bootverbose)
			printf(
"CLK_USE_I8254_CALIBRATION not specified - using default frequency\n");
		freq = timer_freq;
#endif
		timer_freq = freq;
	} else {
		if (bootverbose)
			printf(
		    "%d Hz differs from default of %d Hz by more than 1%%\n",
			       freq, timer_freq);
	}

	set_timer_freq(timer_freq, hz);
	tc_init(&i8254_timecounter);

	init_TSC();
}

static void
rtc_serialcombit(int i)
{
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x17);
	DELAY(1);
	outb(IO_RTC, ((i&0x01)<<5)|0x07);
	DELAY(1);
}

static void
rtc_serialcom(int i)
{
	rtc_serialcombit(i&0x01);
	rtc_serialcombit((i&0x02)>>1);
	rtc_serialcombit((i&0x04)>>2);
	rtc_serialcombit((i&0x08)>>3);
	outb(IO_RTC, 0x07);
	DELAY(1);
	outb(IO_RTC, 0x0f);
	DELAY(1);
	outb(IO_RTC, 0x07);
 	DELAY(1);
}

static void
rtc_outb(int val)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa = ((val >> s) & 0x01) ? 0x27 : 0x07;
	    outb(IO_RTC, sa);		/* set DI & CLK 0 */
	    DELAY(1);
	    outb(IO_RTC, sa | 0x10);	/* CLK 1 */
	    DELAY(1);
	}
	outb(IO_RTC, sa & 0xef);	/* CLK 0 */
}

static int
rtc_inb(void)
{
	int s;
	int sa = 0;

	for (s=0;s<8;s++) {
	    sa |= ((inb(0x33) & 0x01) << s);
	    outb(IO_RTC, 0x17);	/* CLK 1 */
	    DELAY(1);
	    outb(IO_RTC, 0x07);	/* CLK 0 */
	    DELAY(2);
	}
	return sa;
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	struct timespec ts;
	struct clocktime ct;
	int i;

	if (base) {
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}

	rtc_serialcom(0x03);	/* Time Read */
	rtc_serialcom(0x01);	/* Register shift command. */
	DELAY(20);

	ct.nsec = 0;
	ct.sec = bcd2bin(rtc_inb() & 0xff);		/* sec */
	ct.min = bcd2bin(rtc_inb() & 0xff);		/* min */
	ct.hour = bcd2bin(rtc_inb() & 0xff);		/* hour */
	ct.day = bcd2bin(rtc_inb() & 0xff);		/* date */
	i = rtc_inb();
	ct.dow = i & 0x0f;				/* dow */
	ct.mon = (i >> 4) & 0x0f;			/* month */
	ct.year = bcd2bin(rtc_inb() & 0xff) + 1900;	/* year */
	if (ct.year < 1995)
		ct.year += 100;
	/* Set dow = -1 because some clocks don't set it correctly. */
	ct.dow = -1;
	if (clock_ct_to_ts(&ct, &ts)) {
		printf("Invalid time in clock: check and reset the date!\n");
		return;
	}
	ts.tv_sec += utc_offset();
	tc_setclock(&ts);
}

/*
 * Write system time back to RTC
 */
void
resettodr()
{
	struct timespec	ts;
	struct clocktime ct;

	if (disable_rtc_set)
		return;

	getnanotime(&ts);
	ts.tv_sec -= utc_offset();
	clock_ts_to_ct(&ts, &ct);

	rtc_serialcom(0x01);	/* Register shift command. */

	rtc_outb(bin2bcd(ct.sec)); 		/* Write back Seconds */
	rtc_outb(bin2bcd(ct.min)); 		/* Write back Minutes */
	rtc_outb(bin2bcd(ct.hour)); 		/* Write back Hours   */

	rtc_outb(bin2bcd(ct.day));		/* Write back Day     */
	rtc_outb((ct.mon << 4) | ct.dow);	/* Write back Month and DOW */
	rtc_outb(bin2bcd(ct.year % 100));	/* Write back Year    */

	rtc_serialcom(0x02);	/* Time set & Counter hold command. */
	rtc_serialcom(0x00);	/* Register hold command. */
}


/*
 * Start both clocks running.
 */
void
cpu_initclocks()
{

#ifdef DEV_APIC
	using_lapic_timer = lapic_setup_clock();
#endif
	/*
	 * If we aren't using the local APIC timer to drive the kernel
	 * clocks, setup the interrupt handler for the 8254 timer 0 so
	 * that it can drive hardclock().  Otherwise, change the 8254
	 * timecounter to user a simpler algorithm.
	 */
	if (!using_lapic_timer) {
		intr_add_handler("clk", 0, (driver_filter_t *)clkintr, NULL,
		    NULL, INTR_TYPE_CLK, NULL);
		i8254_intsrc = intr_lookup_source(0);
		if (i8254_intsrc != NULL)
			i8254_pending =
			    i8254_intsrc->is_pic->pic_source_pending;
	} else {
		i8254_timecounter.tc_get_timecount =
		    i8254_simple_get_timecount;
		i8254_timecounter.tc_counter_mask = 0xffff;
		set_timer_freq(timer_freq, hz);
	}

	init_TSC_tc();
}

void
cpu_startprofclock(void)
{
}

void
cpu_stopprofclock(void)
{
}

static int
sysctl_machdep_i8254_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int freq;

	/*
	 * Use `i8254' instead of `timer' in external names because `timer'
	 * is is too generic.  Should use it everywhere.
	 */
	freq = timer_freq;
	error = sysctl_handle_int(oidp, &freq, 0, req);
	if (error == 0 && req->newptr != NULL)
		set_timer_freq(freq, hz);
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_i8254_freq, "IU", "");

static unsigned
i8254_simple_get_timecount(struct timecounter *tc)
{

	return (timer0_max_count - getit());
}

static unsigned
i8254_get_timecount(struct timecounter *tc)
{
	u_int count;
	u_int high, low;
	u_int eflags;

	eflags = read_eflags();
	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = timer0_max_count - ((high << 8) | low);
	if (count < i8254_lastcount ||
	    (!i8254_ticked && (clkintr_pending ||
	    ((count < 20 || (!(eflags & PSL_I) && count < timer0_max_count / 2u)) &&
	    i8254_pending != NULL && i8254_pending(i8254_intsrc))))) {
		i8254_ticked = 1;
		i8254_offset += timer0_max_count;
	}
	i8254_lastcount = count;
	count += i8254_offset;
	mtx_unlock_spin(&clock_lock);
	return (count);
}

#ifdef DEV_ISA
/*
 * Attach to the ISA PnP descriptors for the timer and realtime clock.
 */
static struct isa_pnp_id attimer_ids[] = {
	{ 0x0001d041 /* PNP0100 */, "AT timer" },
	{ 0x000bd041 /* PNP0B00 */, "AT realtime clock" },
	{ 0 }
};

static int
attimer_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, attimer_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

static int
attimer_attach(device_t dev)
{
	return(0);
}

static device_method_t attimer_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		attimer_probe),
	DEVMETHOD(device_attach,	attimer_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),	/* XXX stop statclock? */
	DEVMETHOD(device_resume,	bus_generic_resume),	/* XXX restart statclock? */
	{ 0, 0 }
};

static driver_t attimer_driver = {
	"attimer",
	attimer_methods,
	1,		/* no softc */
};

static devclass_t attimer_devclass;

DRIVER_MODULE(attimer, isa, attimer_driver, attimer_devclass, 0, 0);
#endif /* DEV_ISA */
