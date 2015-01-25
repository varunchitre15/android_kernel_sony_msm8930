/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 * Copyright (C) 2012 Sony Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/pm8xxx/misc.h>

#include <asm/mach-types.h>

#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/irqs.h>
#include <mach/scm.h>
#include "msm_watchdog.h"
#include "timer.h"

#include <mach/subsystem_restart.h>
extern long system_flag;
#ifdef CONFIG_CCI_KLOG
long* powerpt = (long*)POWER_OFF_SPECIAL_ADDR;
long* unknowflag = (long*)UNKONW_CRASH_SPECIAL_ADDR;
long* backupcrashflag = (long*)CRASH_SPECIAL_ADDR;
#endif
#define ABNORAML_NONE		0x0
#define ABNORAML_REBOOT		0x1
#define ABNORAML_CRASH		0x2
#define ABNORAML_POWEROFF	0x3

static long abnormalflag = ABNORAML_NONE;



#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C


#ifdef CONFIG_CCI_KLOG
#include <linux/cciklog.h>
#endif // #ifdef CONFIG_CCI_KLOG


#define PSHOLD_CTL_SU (MSM_TLMM_BASE + 0x820)

#define RESTART_REASON_ADDR 0x65C
#define DLOAD_MODE_ADDR     0x0

#define SCM_IO_DISABLE_PMIC_ARBITER	1

#define CONFIG_WARMBOOT_MAGIC_ADDR  0xAA0
#define CONFIG_WARMBOOT_MAGIC_VAL   0xdeadbeef
 
#define CONFIG_WARMBOOT_ADDR        0x2A03F65C
#define CONFIG_WARMBOOT_CLEAR       0xabadbabe
#define CONFIG_WARMBOOT_NONE        0x00000000
#define CONFIG_WARMBOOT_S1          0x6f656d53
#define CONFIG_WARMBOOT_FB          0x77665500
#define CONFIG_WARMBOOT_FOTA        0x6f656d46
#define CONFIG_WARMBOOT_NORMAL     	0x77665501
#define CONFIG_WARMBOOT_CRASH       0xc0dedead
static void* warm_boot_addr;
void set_warmboot(void);

#ifdef CONFIG_LGE_CRASH_HANDLER
#define LGE_ERROR_HANDLER_MAGIC_NUM	0xA97F2C46
#define LGE_ERROR_HANDLER_MAGIC_ADDR	0x18
void *lge_error_handler_cookie_addr;
static int ssr_magic_number = 0;
#endif

static int restart_mode;
void *restart_reason;

int pmic_reset_irq;
static void __iomem *msm_tmr0_base;

#ifdef CONFIG_MSM_DLOAD_MODE
static int in_panic;
static void *dload_mode_addr;

/* Download mode master kill-switch */
static int dload_set(const char *val, struct kernel_param *kp);
static int download_mode = 1;
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);

#ifdef CONFIG_CCI_KLOG
extern void record_shutdown_time(int state);
#endif // #ifdef CONFIG_CCI_KLOG


static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void set_dload_mode(int on)
{
	if (dload_mode_addr) {
#if 0
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
#ifdef CONFIG_LGE_CRASH_HANDLER
		__raw_writel(on ? LGE_ERROR_HANDLER_MAGIC_NUM : 0,
				lge_error_handler_cookie_addr);
#endif
		mb();
#endif
	}
}

void set_warmboot()
{
	if (warm_boot_addr) {
		__raw_writel(CONFIG_WARMBOOT_MAGIC_VAL, warm_boot_addr);	   
		mb();
	}
}

static int dload_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);
#ifdef CONFIG_LGE_CRASH_HANDLER
	ssr_magic_number = 0;
#endif

	return 0;
}
#else
#define set_dload_mode(x) do {} while (0)
#endif

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
#ifdef CONFIG_LGE_CRASH_HANDLER
	if (download_mode == 1 && (mode & 0xFFFF0000) == 0x6D630000)
		panic("LGE crash handler detected panic");
#endif
}
EXPORT_SYMBOL(msm_set_restart_mode);

static void __msm_power_off(int lower_pshold)
{
	if ((abnormalflag == ABNORAML_CRASH) || (abnormalflag == ABNORAML_REBOOT))
	{
		printk(KERN_NOTICE "Not allow power off after panic/fatal/reset happen\n");
		goto reset2;
	}
	
	abnormalflag = ABNORAML_POWEROFF;
	if(system_flag == inactive)
	{
		system_flag = poweroff;
#ifdef CONFIG_CCI_KLOG	
		*powerpt = (POWERONOFFRECORD + system_flag);
#endif		
		mb();
	}	

#ifdef CONFIG_CCI_KLOG
	cklc_save_magic(KLOG_MAGIC_POWER_OFF, KLOG_STATE_NONE);
	record_shutdown_time(0x06);
#endif // #ifdef CONFIG_CCI_KLOG

	printk(KERN_CRIT "Powering off the SoC\n");
#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(0);
#endif
	pm8xxx_reset_pwr_off(0);

	if (lower_pshold) {
		__raw_writel(0, PSHOLD_CTL_SU);
		mdelay(10000);
		printk(KERN_ERR "Powering off has failed\n");
	}
reset2: 	
	return;
}

static void msm_power_off(void)
{
	__raw_writel(CONFIG_WARMBOOT_NONE, restart_reason);
#ifdef CONFIG_CCI_KLOG	
	*unknowflag = 0;
	*backupcrashflag = 0;
#endif
	/* MSM initiated power off, lower ps_hold */
	__msm_power_off(1);
}

static void cpu_power_off(void *data)
{
	int rc;
	__raw_writel(CONFIG_WARMBOOT_NONE, restart_reason);
#ifdef CONFIG_CCI_KLOG	
	*unknowflag = 0;
	*backupcrashflag = 0;
#endif
	pr_err("PMIC Initiated shutdown %s cpu=%d\n", __func__,
						smp_processor_id());
	if (smp_processor_id() == 0) {
		/*
		 * PMIC initiated power off, do not lower ps_hold, pmic will
		 * shut msm down
		 */
		__msm_power_off(0);

		pet_watchdog();
		pr_err("Calling scm to disable arbiter\n");
		/* call secure manager to disable arbiter and never return */
		rc = scm_call_atomic1(SCM_SVC_PWR,
						SCM_IO_DISABLE_PMIC_ARBITER, 1);

		pr_err("SCM returned even when asked to busy loop rc=%d\n", rc);
		pr_err("waiting on pmic to shut msm down\n");
	}

	preempt_disable();
	while (1)
		;
}

static irqreturn_t resout_irq_handler(int irq, void *dev_id)
{
	pr_warn("%s PMIC Initiated shutdown\n", __func__);
	oops_in_progress = 1;
	smp_call_function_many(cpu_online_mask, cpu_power_off, NULL, 0);
	if (smp_processor_id() == 0)
		cpu_power_off(NULL);
	preempt_disable();
	while (1)
		;
	return IRQ_HANDLED;
}

#ifdef CONFIG_LGE_CRASH_HANDLER
#define SUBSYS_NAME_MAX_LENGTH	40

int get_ssr_magic_number(void)
{
	return ssr_magic_number;
}

void set_ssr_magic_number(const char* subsys_name)
{
	int i;
	const char *subsys_list[] = {
		"modem", "riva", "dsps", "lpass",
		"external_modem", "gss",
	};

	ssr_magic_number = (0x6d630000 | 0x0000f000);

	for (i=0; i < ARRAY_SIZE(subsys_list); i++) {
		if (!strncmp(subsys_list[i], subsys_name,
					SUBSYS_NAME_MAX_LENGTH)) {
			ssr_magic_number = (0x6d630000 | ((i+1)<<12));
			break;
		}
	}
}

void set_kernel_crash_magic_number(void)
{
	pet_watchdog();
	if (ssr_magic_number == 0)
		__raw_writel(0x6d630100, restart_reason);
	else
		__raw_writel(restart_mode, restart_reason);
}
#endif /* CONFIG_LGE_CRASH_HANDLER */

void msm_restart(char mode, const char *cmd)
{

#ifdef CONFIG_CCI_KLOG
	char buf[7] = {0};
#endif // #ifdef CONFIG_CCI_KLOG

	__raw_writel(CONFIG_WARMBOOT_NONE, restart_reason);
#ifdef CONFIG_CCI_KLOG	
	*unknowflag = 0;
	*backupcrashflag = 0;
#endif
#ifdef CONFIG_MSM_DLOAD_MODE

#ifndef CCI_KLOG_ALLOW_FORCE_PANIC
	if ((abnormalflag == ABNORAML_POWEROFF) || (abnormalflag == ABNORAML_REBOOT))
		goto reset3;		
#endif
	/* This looks like a normal reboot at this point. */
	set_dload_mode(0);

	/* Write download mode flags if we're panic'ing */
	set_dload_mode(in_panic);
	if(in_panic)
	{
#ifndef CCI_KLOG_ALLOW_FORCE_PANIC
		if (abnormalflag == ABNORAML_NONE)
#endif
		abnormalflag = ABNORAML_CRASH;
		set_warmboot();
#ifdef CONFIG_CCI_KLOG
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC			
		__raw_writel(CONFIG_WARMBOOT_CRASH, restart_reason);
#else
		__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
		*backupcrashflag = CONFIG_WARMBOOT_CRASH;
#endif
#endif	
	}
	/* Write download mode flags if restart_mode says so */
	if (restart_mode == RESTART_DLOAD) {
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
		if (abnormalflag == ABNORAML_NONE)
		{
#endif
			abnormalflag = ABNORAML_REBOOT;

#ifdef CONFIG_CCI_KLOG
		cklc_save_magic(KLOG_MAGIC_DOWNLOAD_MODE, KLOG_STATE_DOWNLOAD_MODE);
#endif // #ifdef CONFIG_CCI_KLOG

		if(system_flag == inactive)	
			system_flag = adloadmode;

		set_dload_mode(1);
		set_warmboot();
		__raw_writel(CONFIG_WARMBOOT_S1 , restart_reason);	
#ifdef CONFIG_LGE_CRASH_HANDLER
		writel(0x6d63c421, restart_reason);
		goto reset;
#endif
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
		}
		else
		{
			pm8xxx_reset_pwr_off(1);
			printk(KERN_NOTICE "Not allow reset after panic/fatal/poweroff happen\n");
			goto reset2;
		}
#endif
	}

	/* Kill download mode if master-kill switch is set */
	if (!download_mode)
		set_dload_mode(0);
#endif

	printk(KERN_NOTICE "Going down for restart now\n");

	pm8xxx_reset_pwr_off(1);

#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
	if (abnormalflag == ABNORAML_CRASH)
	{
		printk(KERN_NOTICE "Not allow reset after panic/fatal happen\n");
		goto reset2;
	}
#endif

	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {

#ifdef CONFIG_CCI_KLOG
			cklc_save_magic(KLOG_MAGIC_BOOTLOADER, KLOG_STATE_NONE);
#endif // #ifdef CONFIG_CCI_KLOG

#if 0
			__raw_writel(0x77665500, restart_reason);
#else
			set_warmboot();
			__raw_writel(CONFIG_WARMBOOT_FB, restart_reason);
            if(system_flag == inactive)
	            system_flag = normalreboot_bootloader;		
#endif
		} else if (!strncmp(cmd, "recovery", 8)) {

#ifdef CONFIG_CCI_KLOG
			cklc_save_magic(KLOG_MAGIC_RECOVERY, KLOG_STATE_NONE);
#endif // #ifdef CONFIG_CCI_KLOG

#if 0
			__raw_writel(0x77665502, restart_reason);
#else
			__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
            if(system_flag == inactive)
	            system_flag = normalreboot_recovery;
#endif
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			code = simple_strtoul(cmd + 4, NULL, 16) & 0xff;
#if 0
			__raw_writel(0x6f656d00 | code, restart_reason);
#else
			__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
#endif

#ifdef CONFIG_CCI_KLOG
			snprintf(buf, KLOG_MAGIC_LENGTH + 1, "%s%lX", KLOG_MAGIC_OEM_COMMAND, code);
			kprintk("OEM command:code=%lX, buf=%s\n", code, buf);
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
			switch(code)
			{
				case 0x60 + KLOG_INDEX_INIT % 0x10://0x60, simulate klog init magic
					cklc_save_magic(KLOG_MAGIC_INIT, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_MARM_FATAL % 0x10://0x61, simulate mARM fatal magic
					cklc_save_magic(KLOG_MAGIC_AARM_PANIC, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_AARM_PANIC % 0x10://0x62, simulate aARM panic magic
					cklc_save_magic(KLOG_MAGIC_AARM_PANIC, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_RPM_CRASH % 0x10://0x63, simulate RPM crash magic
					cklc_save_magic(KLOG_MAGIC_RPM_CRASH, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_FIQ_HANG % 0x10://0x64, simulate FIQ hang magic
					cklc_save_magic(KLOG_MAGIC_FIQ_HANG, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_DOWNLOAD_MODE % 0x10://0x65, simulate normal download mode magic
					cklc_save_magic(KLOG_MAGIC_DOWNLOAD_MODE, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_POWER_OFF % 0x10://0x66, simulate power off magic
					cklc_save_magic(KLOG_MAGIC_POWER_OFF, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_REBOOT % 0x10://0x67, simulate normal reboot magic
					cklc_save_magic(KLOG_MAGIC_REBOOT, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_BOOTLOADER % 0x10://0x68, simulate bootloader mode magic
					cklc_save_magic(KLOG_MAGIC_BOOTLOADER, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_RECOVERY % 0x10://0x69, simulate recovery mode magic
					cklc_save_magic(KLOG_MAGIC_RECOVERY, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_OEM_COMMAND % 0x10://0x6A, simulate oem-command magic with OEM-6A
					cklc_save_magic(KLOG_MAGIC_OEM_COMMAND"6A", KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_APPSBL % 0x10://0x6B, simulate AppSBL magic
					cklc_save_magic(KLOG_MAGIC_APPSBL, KLOG_STATE_NONE);
					break;

				case 0x60 + KLOG_INDEX_FORCE_CLEAR % 0x10://0x6F, simulate force clear magic
					cklc_save_magic(KLOG_MAGIC_FORCE_CLEAR, KLOG_STATE_NONE);
					break;

				default:
					cklc_save_magic(buf, KLOG_STATE_NONE);
					break;
			}
#else // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC
			cklc_save_magic(buf, KLOG_STATE_NONE);
#endif // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC
            if(system_flag == inactive)
	            system_flag = normalreboot_oem;	
#endif // #ifdef CONFIG_CCI_KLOG

		} 
		else if (!strncmp(cmd, "oemS", 4)) 
		{
			set_warmboot();
			__raw_writel(CONFIG_WARMBOOT_S1, restart_reason);
			if(system_flag == inactive)		
				system_flag = adloadmode;
		}
		else if (!strncmp(cmd, "oemF", 4)) 
		{
			set_warmboot();
			__raw_writel(CONFIG_WARMBOOT_FOTA , restart_reason);
			if(system_flag == inactive)
				system_flag = normalreboot_recovery;
		}
		else 
		{

#ifdef CONFIG_CCI_KLOG
			cklc_save_magic(KLOG_MAGIC_REBOOT, KLOG_STATE_NONE);
#endif // #ifdef CONFIG_CCI_KLOG

#if 0
			__raw_writel(0x77665501, restart_reason);
#else
			abnormalflag = ABNORAML_REBOOT;
			set_warmboot();
			__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
			if(system_flag == inactive)
				system_flag = normalreboot;	
#endif
		}
	} 		
	else 
	{

#ifdef CONFIG_CCI_KLOG
		cklc_save_magic(KLOG_MAGIC_REBOOT, KLOG_STATE_NONE);
#endif // #ifdef CONFIG_CCI_KLOG

#if 0
		__raw_writel(0x77665501, restart_reason);
#else
		abnormalflag = ABNORAML_REBOOT;
		set_warmboot();
		__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
        if(system_flag == inactive)
	        system_flag = normalreboot;	
#endif
	}
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
reset2:	
#endif	
#ifdef CONFIG_CCI_KLOG
	*powerpt = (POWERONOFFRECORD + system_flag);
#endif	

#ifdef CONFIG_LGE_CRASH_HANDLER
	if (in_panic == 1)
		set_kernel_crash_magic_number();
reset:
#endif /* CONFIG_LGE_CRASH_HANDLER */

	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	if (!(machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())) {
		mb();
		__raw_writel(0, PSHOLD_CTL_SU); /* Actually reset the chip */
		mdelay(5000);
		pr_notice("PS_HOLD didn't work, falling back to watchdog\n");
	}

	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5*0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);

	mdelay(10000);
	printk(KERN_ERR "Restarting has failed\n");
#ifndef CCI_KLOG_ALLOW_FORCE_PANIC
reset3:
	printk(KERN_NOTICE "Not allow another reset/panic/fatal after previous behavior \n");
#endif
}

static int __init msm_pmic_restart_init(void)
{
	int rc;

	if (pmic_reset_irq != 0) {
		rc = request_any_context_irq(pmic_reset_irq,
					resout_irq_handler, IRQF_TRIGGER_HIGH,
					"restart_from_pmic", NULL);
		if (rc < 0)
			pr_err("pmic restart irq fail rc = %d\n", rc);
	} else {
		pr_warn("no pmic restart interrupt specified\n");
	}

#ifdef CONFIG_LGE_CRASH_HANDLER
	__raw_writel(0x6d63ad00, restart_reason);
#endif

	return 0;
}

late_initcall(msm_pmic_restart_init);

static int __init msm_restart_init(void)
{
#ifdef CONFIG_MSM_DLOAD_MODE
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	dload_mode_addr = MSM_IMEM_BASE + DLOAD_MODE_ADDR;
	warm_boot_addr	= MSM_IMEM_BASE + CONFIG_WARMBOOT_MAGIC_ADDR;
#ifdef CONFIG_LGE_CRASH_HANDLER
	lge_error_handler_cookie_addr = MSM_IMEM_BASE +
		LGE_ERROR_HANDLER_MAGIC_ADDR;
#endif
	set_dload_mode(download_mode);
#endif
	msm_tmr0_base = msm_timer_get_timer0_base();
	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR;
	pm_power_off = msm_power_off;
	set_warmboot();
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
	__raw_writel(CONFIG_WARMBOOT_CRASH, restart_reason);
#else	
	__raw_writel(CONFIG_WARMBOOT_NORMAL, restart_reason);
#endif	
	return 0;
}
early_initcall(msm_restart_init);
