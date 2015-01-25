/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#ifdef CONFIG_CCI_KLOG
#include <linux/cciklog.h>
#include <mach/msm_iomap.h>
#endif // #ifdef CONFIG_CCI_KLOG


#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};

#ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
static int debug_mask = DEBUG_USER_STATE | DEBUG_SUSPEND;
#else // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
static int debug_mask = DEBUG_USER_STATE;
#endif // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG

module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DEFINE_SPINLOCK(state_lock);
enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
static int state;

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);


#ifdef CONFIG_CCI_KLOG
extern unsigned int resume_time;
extern struct timespec suspend_timestamp;
extern struct timespec resume_timestamp;
extern int suspend_resume_state;

extern void record_suspend_resume_time(int state, unsigned int time);
extern struct timespec klog_get_kernel_clock_timestamp(void);
#endif // #ifdef CONFIG_CCI_KLOG


static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("early_suspend: abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}


#ifdef CONFIG_CCI_KLOG
	if(suspend_resume_state == 0)
	{
		suspend_timestamp = klog_get_kernel_clock_timestamp();
#ifdef CCI_KLOG_DETAIL_LOG
		kprintk("suspend_resume_state(%d):suspend_timestamp=%u.%u\n", suspend_resume_state, (unsigned int)suspend_timestamp.tv_sec, (unsigned int)suspend_timestamp.tv_nsec);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
	}
	suspend_resume_state = 1;
#endif // #ifdef CONFIG_CCI_KLOG

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: call handlers\n");
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {

#ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
			printk("[SUSPEND]%s():%pF (0x%X)\n", __func__, (void*)pos->suspend, (int)pos->suspend);
#else // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("early_suspend: calling %pf\n", pos->suspend);
#endif // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG

			pos->suspend(pos);
		}
	}
	mutex_unlock(&early_suspend_lock);

	suspend_sys_sync_queue();
abort:
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED)
		wake_unlock(&main_wake_lock);
	spin_unlock_irqrestore(&state_lock, irqflags);
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

#ifdef CONFIG_CCI_KLOG
	struct timespec current_timestamp;
#endif // #ifdef CONFIG_CCI_KLOG


	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("late_resume: abort, state %d\n", state);
		goto abort;
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {

#ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
			printk("[RESUME]%s():%pF (0x%X)\n", __func__, (void *)pos->resume, (int)pos->resume);
#else // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("late_resume: calling %pf\n", pos->resume);
#endif // #ifdef CONFIG_CCI_PM_EARLYSUSPEND_LATERESUME_LOG


			pos->resume(pos);
		}
	}

#ifdef CONFIG_CCI_KLOG
	if(suspend_resume_state == 3)
	{
		current_timestamp = klog_get_kernel_clock_timestamp();
#ifdef CCI_KLOG_DETAIL_LOG
		kprintk("suspend_resume_state(%d):resume_timestamp=%u.%u\n", suspend_resume_state, (unsigned int)current_timestamp.tv_sec, (unsigned int)current_timestamp.tv_nsec);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
		resume_timestamp.tv_sec = current_timestamp.tv_sec - resume_timestamp.tv_sec;
		resume_time = (unsigned int)(resume_timestamp.tv_sec * 1000 + resume_timestamp.tv_nsec / 1000000);//ms
		record_suspend_resume_time(suspend_resume_state, resume_time);
#ifdef CCI_KLOG_DETAIL_LOG
		kprintk("suspend_resume_state(%d):resume_time=%u\n", suspend_resume_state, resume_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
	}
	suspend_resume_state = 0;
#endif // #ifdef CONFIG_CCI_KLOG

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");
abort:
	mutex_unlock(&early_suspend_lock);
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}
	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		wake_lock(&main_wake_lock);
		queue_work(suspend_work_queue, &late_resume_work);
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}
