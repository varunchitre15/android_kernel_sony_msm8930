#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ptrace.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <asm/cputime.h>
#include <asm/io.h>
#include <mach/msm_iomap.h>
#include <mach/subsystem_restart.h>
#include <linux/cciklog.h>
#ifdef IMEM_CERT_RECORD
#include <linux/ccistuff.h>
#endif // #ifdef IMEM_CERT_RECORD
#ifdef CCI_HW_ID
#include <mach/gpio.h>
#endif // #ifdef CCI_HW_ID

/*******************************************************************************
* Local Variable/Structure Declaration
*******************************************************************************/

#define KLOG_VERSION		"1.11.0"

//make sure the category is able to write
#define CHK_CATEGORY(x) \
				do\
				{\
					if(unlikely(x < KLOG_KERNEL || x >= KLOG_IGNORE))\
					{\
						kprintk("%s:%s():%u --> Func Caller:<%p> sent an invalid KLog category:%u\n",\
							__FILE__, __func__, __LINE__,\
							__builtin_return_address(0),\
							x );\
						return;/* FIXME: Kevin_Chiang@CCI: Too coupling coding style */\
					}\
				}\
				while(0)

#define MSM_KLOG_MAGIC		MSM_KLOG_BASE
#define MSM_KLOG_HEADER		MSM_KLOG_BASE
#define MSM_KLOG_MAIN		(MSM_KLOG_BASE + CCI_KLOG_HEADER_SIZE)
#define MSM_KLOG_FOOTER		(MSM_KLOG_MAIN + KLOG_CATEGORY_TOTAL_SIZE)

#define MSM_SHARED_RAM_PHYS	0x80000000

#ifdef CCI_KLOG_SUPPORT_ATTRIBUTE
#define KLOG_RO_DEV_ATTR(name)	static DEVICE_ATTR(name, S_IRUSR, klog_show_##name, NULL)
#define KLOG_WO_DEV_ATTR(name)	static DEVICE_ATTR(name, S_IWUSR, NULL, klog_store_##name)
#define KLOG_RW_DEV_ATTR(name)	static DEVICE_ATTR(name, S_IRUSR | S_IWUSR, klog_show_##name, klog_store_##name)
#endif // #ifdef CCI_KLOG_SUPPORT_ATTRIBUTE

static struct klog_magic_list kml[] =
{
	{KLOG_INDEX_FORCE_CLEAR, KLOG_MAGIC_FORCE_CLEAR, KLOG_PRIORITY_FORCE_CLEAR},
	{KLOG_INDEX_INIT, KLOG_MAGIC_INIT, KLOG_PRIORITY_KLOG_INIT},
	{KLOG_INDEX_MARM_FATAL, KLOG_MAGIC_MARM_FATAL, KLOG_PRIORITY_MARM_FATAL},
	{KLOG_INDEX_AARM_PANIC, KLOG_MAGIC_AARM_PANIC, KLOG_PRIORITY_AARM_PANIC},
	{KLOG_INDEX_RPM_CRASH, KLOG_MAGIC_RPM_CRASH, KLOG_PRIORITY_RPM_CRASH},
	{KLOG_INDEX_FIQ_HANG, KLOG_MAGIC_FIQ_HANG, KLOG_PRIORITY_FIQ_HANG},
	{KLOG_INDEX_DOWNLOAD_MODE, KLOG_MAGIC_DOWNLOAD_MODE, KLOG_PRIORITY_DOWNLOAD_MODE},
	{KLOG_INDEX_POWER_OFF, KLOG_MAGIC_POWER_OFF, KLOG_PRIORITY_POWER_OFF},
	{KLOG_INDEX_REBOOT, KLOG_MAGIC_REBOOT, KLOG_PRIORITY_NATIVE_COMMAND},
	{KLOG_INDEX_BOOTLOADER, KLOG_MAGIC_BOOTLOADER, KLOG_PRIORITY_NATIVE_COMMAND},
	{KLOG_INDEX_RECOVERY, KLOG_MAGIC_RECOVERY, KLOG_PRIORITY_NATIVE_COMMAND},
	{KLOG_INDEX_OEM_COMMAND, KLOG_MAGIC_OEM_COMMAND, KLOG_PRIORITY_OEM_COMMAND},
};

static struct klog_category *pklog_category[] =
{
#if CCI_KLOG_CRASH_SIZE
	MSM_KLOG_MAIN,
#endif // #if CCI_KLOG_CRASH_SIZE
#if CCI_KLOG_APPSBL_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE,
#endif // #if CCI_KLOG_APPSBL_SIZE
#if CCI_KLOG_KERNEL_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE + CCI_KLOG_APPSBL_SIZE,
#endif // #if CCI_KLOG_KERNEL_SIZE
#if CCI_KLOG_ANDROID_MAIN_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE + CCI_KLOG_APPSBL_SIZE + CCI_KLOG_KERNEL_SIZE,
#endif // #if CCI_KLOG_ANDROID_MAIN_SIZE
#if CCI_KLOG_ANDROID_SYSTEM_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE + CCI_KLOG_APPSBL_SIZE + CCI_KLOG_KERNEL_SIZE + CCI_KLOG_ANDROID_MAIN_SIZE,
#endif // #if CCI_KLOG_ANDROID_SYSTEM_SIZE
#if CCI_KLOG_ANDROID_RADIO_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE + CCI_KLOG_APPSBL_SIZE + CCI_KLOG_KERNEL_SIZE + CCI_KLOG_ANDROID_MAIN_SIZE + CCI_KLOG_ANDROID_SYSTEM_SIZE,
#endif // #if CCI_KLOG_ANDROID_RADIO_SIZE
#if CCI_KLOG_ANDROID_EVENTS_SIZE
	MSM_KLOG_MAIN + CCI_KLOG_CRASH_SIZE + CCI_KLOG_APPSBL_SIZE + CCI_KLOG_KERNEL_SIZE + CCI_KLOG_ANDROID_MAIN_SIZE + CCI_KLOG_ANDROID_SYSTEM_SIZE + CCI_KLOG_ANDROID_RADIO_SIZE,
#endif // #if CCI_KLOG_ANDROID_EVENTS_SIZE
};

static struct system_information sysinfo;
#ifdef IMEM_CERT_RECORD
static unsigned int *cci_imem_magic = (void*)MSM_IMEM_BASE + 0xB00;
#endif // #ifdef IMEM_CERT_RECORD
#ifdef CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
static unsigned int *sbl_bootup_time = (void*)MSM_IMEM_BASE + 0xB10;
#endif // #ifdef CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
static struct klog_time system_bootup_time;
static struct klog_time system_shutdown_time;
unsigned int suspend_time = 0;
unsigned int resume_time = 0;
struct timespec suspend_timestamp;
struct timespec resume_timestamp;
int suspend_resume_state = 0;

#ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE
#ifndef CCI_ENGMODE_WORKAROUND
static unsigned int *power_on_off_history = (void*)MSM_IMEM_BASE + 0xB20;
static char *warranty_data = (void*)MSM_IMEM_BASE + 0xB20 + 84 * 4;
static int *factory_record = (void*)MSM_IMEM_BASE + 0xB20 + 84 * 4 + 0x20 * 3;
#endif // #ifndef CCI_ENGMODE_WORKAROUND
static int bootinfo_inited = 0;
static int deviceinfo_inited = 0;
static int debuginfo_inited = 0;
static int warrantyinfo_inited = 0;
static int factoryinfo_inited = 0;

static struct boot_information bootinfo =
{
//boot mode
	1,//emergency_download_mode
	NOT_SUPPORTED_VALUE,//qct_download_mode
	NOT_SUPPORTED_VALUE,//cci_download_mode
	1,//mass_storage_download_mode
	1,//sd_download_mode
	1,//usb_ram_dump_mode
	1,//sd_ram_dump_mode, related to allow to enter download mode or not
	1,//normal_boot_mode
	NOT_SUPPORTED_VALUE,//sd_boot_mode
	1,//fastboot_mode
	1,//recovery_mode
	1,//simple_test_mode, related to region/carrier
	1,//charging_only_mode
	1,//android_safe_mode
//secure boot mode
	NOT_SUPPORTED_VALUE,//qct_secure_boot_mode
	NOT_SUPPORTED_VALUE,//cci_secure_boot_mode
//power on reason
	{0},//power_on_reason_recent[POWER_REASON_RECENT_LENGTH]
	{0},//power_on_reason_counterer[POWER_REASON_COUNTER_LENGTH]
//power off reason
	{0},//power_off_reason_recent[POWER_REASON_RECENT_LENGTH]
	{0},//power_off_reason_counterer[POWER_REASON_COUNTER_LENGTH]
//crash
	{0},//crash_record(3=fatalm, 4=panica, 13=rpm, 14=fiq)
};

static struct device_information deviceinfo =
{
//hw information
	0,//hw_id
	0,//band_id
	0,//model_id
	0,//nfc_id
	"msm8930",//main_chip[MAIN_CHIP_INFO_LENGTH]
	NOT_SUPPORTED_STRING,//radio_chip[RADIO_CHIP_INFO_LENGTH]
	0,//cpuinfo_max_freq;
	0,//scaling_max_freq;
	1024,//ram;
	NOT_SUPPORTED_VALUE,//flash
	0,//emmc
	0,//hw_reset
//sw information
	{0},//android_version[KLOG_ANDROID_VERSION_LENGTH]
	{0},//modem_version[KLOG_MODEM_VERSION_LENGTH]
	{0},//flex_version[KLOG_FLEX_VERSION_LENGTH]
	{0},//rpm_version[KLOG_RPM_VERSION_LENGTH]
	"0.5",//bootloader_version[KLOG_BOOTLOADER_VERSION_LENGTH]
	{0},//linux_version[KLOG_LINUX_VERSION_LENGTH]
	{0},//version_id[KLOG_VERSION_ID_LENGTH]
	{0},//build_version[KLOG_BUILD_VERSION_LENGTH]
	{0},//build_date[KLOG_BUILD_DATE_LENGTH]
	{0},//build_type[KLOG_BUILD_TYPE_LENGTH]
	{0},//build_user[KLOG_BUILD_USER_LENGTH]
	{0},//build_host[KLOG_BUILD_HOST_LENGTH]
	{0},//build_key[KLOG_BUILD_KEY_LENGTH]
	0,//secure_mode
};

static struct debug_information debuginfo =
{
//debug mode
	1,//debug_mode
//debug interface
	1,//usb_sbl_diag
	1,//usb_diag
	1,//usb_adb
	1,//uart
	1,//jtag
//ram dump trigger method
	NOT_SUPPORTED_VALUE,//hot_key_ram_dump
	NOT_SUPPORTED_VALUE,//command_ram_dump
	1,//crash_ram_dump
//logger
	1,//bootloader_log
	1,//kernel_log
	1,//logcat_log
	1,//klog_log
	1,//rpm_log
};

static struct performance_information performanceinfo =
{
//bootup/shutdown/suspend/resume
	0,//bootup_time
	0,//shutdown_time
	0,//suspend_time
	0,//resume_time
	0,//up_time
	0,//idle_time
	0,//sleep_time
};

static struct warranty_information warrantyinfo =
{
//warranty record
	{0},//total_deviceon_time[TOTAL_DEVICE_ON_TIME]
	{0},//first_voice_call_timestamp[FIRST_VOICE_CALL_TIMESTAMP]
	{0},//first_data_call_timestamp[FIRST_DATA_CALL_TIMESTAMP]
	NOT_SUPPORTED_VALUE,//maintenance_record
};

static struct factory_information factoryinfo =
{
//FTM
	1,//backup_command
	1,//restore_command
	0,//backup_record
};
#endif // #ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE

static char *klog_magic = (void *)MSM_KLOG_MAGIC;	//magic number
static int mem_ready = 0;				//0: memory is not ready, 1: memory is ready
static int mem_have_clean = 0;				//0: memory is not clean, 1: memory is clean
static int magic_priority = KLOG_PRIORITY_INVALID;
static int crash_state = 0;				//0: not crash, 0x01: crashing, 0x02: previous crash
static int rtc_synced = 0;
#ifdef DVT1_BOARD_HW_ID
static int device_info_update = 0;
#endif // #ifdef DVT1_BOARD_HW_ID
#ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
static char rpm_version[KLOG_RPM_VERSION_LENGTH] = {0};
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
static char bootloader_version[KLOG_BOOTLOADER_VERSION_LENGTH] = {0};
//for fault/exception record
#if CCI_KLOG_CRASH_SIZE
static int fault_log_level = 3;
static int fault_level = 0;
static int fault_type = -1;
static char fault_msg[256] = {0};
static int kernel_log_level = -1;
#endif // #if CCI_KLOG_CRASH_SIZE
#ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
static char *modem_log = (void *)MSM_SHARED_RAM_BASE;
static unsigned long modem_log_addr = 0;
#endif // #ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
static char previous_reboot_magic[KLOG_MAGIC_LENGTH + 1] = {0};
static int previous_normal_boot = -1;
static unsigned long warmboot = 0;
static unsigned long startup = 0;
static int crashflag_inited = 0;
static int unknownrebootflag_inited = 0;
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
static int force_panic_when_suspend = 0;
static int force_panic_when_power_off = 0;
#endif // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC

/*******************************************************************************
* Local Function Declaration
*******************************************************************************/

/*******************************************************************************
* External Variable/Structure Declaration
*******************************************************************************/

/*******************************************************************************
* External Function Declaration
*******************************************************************************/

/*** Functions ***/

#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
int get_force_panic_when_suspend(void)
{
	return force_panic_when_suspend;
}

int get_force_panic_when_power_off(void)
{
	return force_panic_when_power_off;
}
#endif // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC

#if CCI_KLOG_CRASH_SIZE
//level: 0x10:enable/disable, 0x01: panic, 0x02: die, 0x03: data abort, 0x04: prefetch abort, 0x05: subsystem fatal error
//type: depend on level, data abort: refer to fsr_info[], prefetch abort: ifsr_info[], subsystem: restart level
//msg: message from panic or die, subsystem name
int get_fault_state(void)
{
	return fault_level;
}

void set_fault_state(int level, int type, const char* msg)
{
	if(level == 0)//reset fault
	{
		fault_level = 0;
		fault_type = -1;
		fault_msg[0] = 0;
		pklog_category[KLOG_CRASH]->index = 0;
		pklog_category[KLOG_CRASH]->overload = 0;
		memset(&pklog_category[KLOG_CRASH]->buffer[0], 0, pklog_category[KLOG_CRASH]->size - KLOG_CATEGORY_HEADER_SIZE);
	}
	else if(level > 0)//fault to crash start
	{
		if(fault_level <= 0)
		{
			fault_level = level;
			fault_type = type;
			strncpy(fault_msg, msg, strlen(msg));
			kprintk("fault_level=0x%X, fault_type=%d, fault_msg=%s\n", fault_level, fault_type, fault_msg);
#ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
			if(fault_level == 0x5 && strcmp(fault_msg, "modem") == 0)//modem subsystem
			{
				if(modem_log_addr >= MSM_SHARED_RAM_PHYS && modem_log_addr < (MSM_SHARED_RAM_PHYS + MSM_SHARED_RAM_SIZE))//dynamic allocated address
				{
					modem_log += (modem_log_addr - MSM_SHARED_RAM_PHYS);
				}
				else//invalid address, use last 2KB address of SMEM by default
				{
					kprintk("invalid modem crash log address:0x%lX, use default address\n", modem_log_addr);
					modem_log += (MSM_SHARED_RAM_SIZE - 1024 * 2);
				}
				if(strlen(modem_log) > 0)
				{
					kprintk("modem crash log:%s\n", modem_log);
				}
				else
				{
					kprintk("modem crash log is empty\n");
				}
			}
#endif // #define CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
		}
		else
		{
			kprintk("fault already exist:%d, ignore:%d\n", fault_level, level);
		}
	}
	else if(level < 0)//fault to crash end
	{
		if(fault_level > 0)
		{
			fault_level |= 0x10;//end flag
			kprintk("fault_level=0x%X, fault_type=%d, fault_msg=%s\n", fault_level, fault_type, fault_msg);
		}
		else
		{
			kprintk("fault not exist, ignore:%d\n", level);
		}
	}

	return;
}
#endif // #if CCI_KLOG_CRASH_SIZE

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
struct timespec klog_get_kernel_rtc_timestamp(const struct timespec *log_time)
{
	struct timespec current_time;

	if(log_time)
	{
		current_time.tv_sec = log_time->tv_sec;
		current_time.tv_nsec = log_time->tv_nsec;
	}
	else
	{
		current_time = current_kernel_time();
	}

	return current_time;
}

struct timespec klog_get_kernel_clock_timestamp(void)
{
	struct timespec current_time;
	unsigned long long now_clock;
	unsigned long now_clock_ns;

	now_clock = cpu_clock(UINT_MAX);
	now_clock_ns = do_div(now_clock, 1000000000);
	current_time.tv_sec = (time_t)now_clock;
	current_time.tv_nsec = (long)now_clock_ns;

	return current_time;
}

struct klog_time klog_record_kernel_timestamp(const struct timespec *log_time)
{
	struct klog_time current_time;

	current_time.clock = klog_get_kernel_clock_timestamp();
	current_time.rtc = klog_get_kernel_rtc_timestamp(log_time);

	if(mem_ready == 1)
	{
//update crash_state first
		update_priority();

		if((crash_state & 0x02) == 0)//only allow to record if not previous crash
		{
//record kernel clock time(uptime)
			snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH, KLOG_KERNEL_TIME_LENGTH, "[%08lX.%08lX]", (unsigned long) current_time.clock.tv_sec, current_time.clock.tv_nsec);

//record kernel first RTC sync time
			if(rtc_synced == 0 && current_time.clock.tv_sec > 30)
			{
				rtc_synced = 1;
				snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH, KLOG_FIRST_RTC_TIMESTAMP_LENGTH, "[%08lX.%08lX]", current_time.rtc.tv_sec, current_time.rtc.tv_nsec);
			}

//record kernel RTC time
			snprintf(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH, KLOG_LAST_RTC_TIMESTAMP_LENGTH, "[%08lX.%08lX]", current_time.rtc.tv_sec, current_time.rtc.tv_nsec);
		}
	}

	return current_time;
}
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP

#ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
void klog_record_rpm_version(const char *str)
{
	strncpy(rpm_version, str, KLOG_RPM_VERSION_LENGTH - 1);

	return;
}
EXPORT_SYMBOL(klog_record_rpm_version);
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION

static __inline__ void __cklc_append_char(unsigned int category, unsigned char c)
{
	if(mem_ready == 1)
	{
//update crash_state first
		update_priority();
	}

	if(mem_ready == 0 || (crash_state & 0x02) > 0)//not allow to record if previous crash
	{
		return;
	}

	if(!mem_have_clean)
	{
#if CCI_KLOG_CRASH_SIZE
		memset(&pklog_category[KLOG_CRASH]->name[0], 0, CCI_KLOG_CRASH_SIZE);
		snprintf(pklog_category[KLOG_CRASH]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_CRASH);
		pklog_category[KLOG_CRASH]->size = CCI_KLOG_CRASH_SIZE;
#endif // #if CCI_KLOG_CRASH_SIZE
#if CCI_KLOG_APPSBL_SIZE
		if(strncmp(pklog_category[KLOG_APPSBL]->name, KLOG_CATEGORY_NAME_APPSBL, strlen(KLOG_CATEGORY_NAME_APPSBL)) != 0
		|| (int)pklog_category[KLOG_APPSBL]->index >= (int)(pklog_category[KLOG_APPSBL]->size - KLOG_CATEGORY_HEADER_SIZE))//category name not match or index over than valid size, that means garbage, so we clean it
		{
			memset(&pklog_category[KLOG_APPSBL]->name[0], 0, CCI_KLOG_APPSBL_SIZE);
			snprintf(pklog_category[KLOG_APPSBL]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_APPSBL);
			pklog_category[KLOG_APPSBL]->size = CCI_KLOG_APPSBL_SIZE;
		}
#endif // #if CCI_KLOG_APPSBL_SIZE
		memset(&pklog_category[KLOG_KERNEL]->name[0], 0, KLOG_CATEGORY_TOTAL_SIZE - CCI_KLOG_CRASH_SIZE - CCI_KLOG_APPSBL_SIZE);
#if CCI_KLOG_KERNEL_SIZE
		snprintf(pklog_category[KLOG_KERNEL]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_KERNEL);
		pklog_category[KLOG_KERNEL]->size = CCI_KLOG_KERNEL_SIZE;
#endif // #if CCI_KLOG_KERNEL_SIZE
#if CCI_KLOG_ANDROID_MAIN_SIZE
		snprintf(pklog_category[KLOG_ANDROID_MAIN]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_ANDROID_MAIN);
		pklog_category[KLOG_ANDROID_MAIN]->size = CCI_KLOG_ANDROID_MAIN_SIZE;
#endif // #if CCI_KLOG_ANDROID_MAIN_SIZE
#if CCI_KLOG_ANDROID_SYSTEM_SIZE
		snprintf(pklog_category[KLOG_ANDROID_SYSTEM]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_ANDROID_SYSTEM);
		pklog_category[KLOG_ANDROID_SYSTEM]->size = CCI_KLOG_ANDROID_SYSTEM_SIZE;
#endif // #if CCI_KLOG_ANDROID_SYSTEM_SIZE
#if CCI_KLOG_ANDROID_RADIO_SIZE
		snprintf(pklog_category[KLOG_ANDROID_RADIO]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_ANDROID_RADIO);
		pklog_category[KLOG_ANDROID_RADIO]->size = CCI_KLOG_ANDROID_RADIO_SIZE;
#endif // #if CCI_KLOG_ANDROID_RADIO_SIZE
#if CCI_KLOG_ANDROID_EVENTS_SIZE
		snprintf(pklog_category[KLOG_ANDROID_EVENTS]->name, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_ANDROID_EVENTS);
		pklog_category[KLOG_ANDROID_EVENTS]->size = CCI_KLOG_ANDROID_EVENTS_SIZE;
#endif // #if CCI_KLOG_ANDROID_EVENTS_SIZE
#if (CCI_KLOG_HEADER_SIZE + KLOG_CATEGORY_TOTAL_SIZE) + KLOG_CATEGORY_NAME_LENGTH < CCI_KLOG_SIZE
		snprintf(MSM_KLOG_FOOTER, KLOG_CATEGORY_NAME_LENGTH, KLOG_CATEGORY_NAME_RESERVE);
#endif // #if (CCI_KLOG_HEADER_SIZE + KLOG_CATEGORY_TOTAL_SIZE) + KLOG_CATEGORY_NAME_LENGTH < CCI_KLOG_SIZE

		mem_have_clean = 1;
	}
	pklog_category[category]->buffer[pklog_category[category]->index++] = c;
	if((int)pklog_category[category]->index >= (int)(pklog_category[category]->size - KLOG_CATEGORY_HEADER_SIZE))
	{
		if(pklog_category[category]->overload < 0xFF)
		{
			pklog_category[category]->overload++;
		}
		pklog_category[category]->index = 0;
	}

	return;
}

#if CCI_KLOG_CRASH_SIZE
//This function should only be called in printk.c
void set_kernel_log_level(int level)
{
	kernel_log_level = level;

	return;
}
#endif // #if CCI_KLOG_CRASH_SIZE

//This function should only be called in printk.c
void cklc_append_kernel_raw_char(unsigned char c)
{
	__cklc_append_char(KLOG_KERNEL, c);
#if CCI_KLOG_CRASH_SIZE
	if(fault_level > 0 && fault_level < 0x10 && kernel_log_level >= 0 && kernel_log_level <= fault_log_level)
	{
		__cklc_append_char(KLOG_CRASH, c);
	}
#endif // #if CCI_KLOG_CRASH_SIZE

	return;
}

static __inline__ void __cklc_append_str(unsigned int category, unsigned char *str, size_t len)
{
	int i = 0;

	for(i = 0; i < len; i++)
	{
		if(isprint(*(str + i)))
		{
			__cklc_append_char(category, *(str + i));
		}
	}

	return;
}

//Append string into KLog[category]
void cklc_append_str(unsigned int category, unsigned char *str, size_t len)
{
	CHK_CATEGORY(category);

	__cklc_append_str(category, str, len);

	return;
}
EXPORT_SYMBOL(cklc_append_str);

static __inline__ void __cklc_append_newline(unsigned int category)
{
	__cklc_append_char(category, '\n');

	return;
}

//Append a new line character('\n') into KLog[category]
void cklc_append_newline(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_newline(category);

	return;
}
EXPORT_SYMBOL(cklc_append_newline);

static __inline__ void __cklc_append_separator(unsigned int category)
{
	unsigned char *sp = "| ";

	__cklc_append_str(category, sp, strlen(sp));

	return;
}

//Append a separator symbol '| ' into KLog[category]
void cklc_append_separator(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_separator(category);

	return;
}
EXPORT_SYMBOL(cklc_append_separator);

static __inline__ void __cklc_append_time_header(unsigned int category, const struct timespec *log_time)
{
	unsigned char tbuf[32] = {0};
	unsigned tlen = 0;
	struct klog_time now;

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	now = klog_record_kernel_timestamp(log_time);
#else // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	if(log_time)
	{
		now.rtc.tv_sec = log_time->tv_sec;
		now.rtc.tv_nsec = log_time->tv_nsec;
	}
	else
	{
		now.rtc = current_kernel_time();
	}
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP

	tlen = snprintf(tbuf, sizeof(tbuf), "[%8lx.%08lx] ", now.rtc.tv_sec, now.rtc.tv_nsec);

	__cklc_append_str(category, tbuf, tlen);

	return;
}

//Append a Unix Epoch timestamp as the line header
void cklc_append_time_header(unsigned int category)
{
	CHK_CATEGORY(category);

	__cklc_append_time_header(category, NULL);

	return;
}
EXPORT_SYMBOL(cklc_append_time_header);

//For Android Logger
void cklc_append_android_log(unsigned int category,
					const struct logger_entry *header,
					const unsigned char *log_priority,
					const char * const log_tag,
					const int log_tag_bytes,
					const char * const log_msg,
					const int log_msg_bytes)
{
	int prilen = 0;
	unsigned char pribuf[17] = {0};
	struct timespec log_time;

	log_time.tv_sec = header->sec;
	log_time.tv_nsec = header->nsec;
	__cklc_append_time_header(category, &log_time);

	prilen = snprintf(pribuf, sizeof(pribuf), "{%X,%X} <%u> ", (unsigned int)header->pid, (unsigned int)header->tid, (unsigned int)*log_priority);
	__cklc_append_str(category, pribuf, prilen);

	__cklc_append_str(category, (unsigned char *)log_tag, (unsigned int)log_tag_bytes);
	__cklc_append_separator(category);

	__cklc_append_str(category, (unsigned char *)log_msg, (unsigned int)log_msg_bytes);
	__cklc_append_newline(category);

	return;
}
EXPORT_SYMBOL(cklc_append_android_log);

static void console_output(unsigned char *buf, unsigned int len)
{
	struct console *con;

	for(con = console_drivers; con; con = con->next)
	{
		if((con->flags & CON_ENABLED) && con->write &&
				(cpu_online(smp_processor_id()) ||
				(con->flags & CON_ANYTIME)))
		{
			con->write(con, buf, len);
		}
	}

	return;
}

static void __show_android_log_to_console(unsigned int category)
{
	unsigned int len = 0;
	unsigned char strbuf[80] = {0};

	len = snprintf(strbuf, sizeof(strbuf), "\n\n============================= KLog Start =============================\n");
	console_output(strbuf, len);
	len = snprintf(strbuf, sizeof(strbuf), "KLog category #%u:\nname:%s\nsize:%u\nindex:%u\noverload:%u\n", category, pklog_category[category]->name, pklog_category[category]->size, pklog_category[category]->index, pklog_category[category]->overload);
	console_output(strbuf, len);

	if(pklog_category[category]->overload == 0 && pklog_category[category]->index == 0)
	{
		len = snprintf(strbuf, sizeof(strbuf), "<Empty>");
		console_output(strbuf, len);
	}
	else
	{
		if(pklog_category[category]->overload == 0)
		{
			console_output(&pklog_category[category]->buffer[0], pklog_category[category]->index);
		}
		else
		{
			console_output(&pklog_category[category]->buffer[pklog_category[category]->index], (pklog_category[category]->size - KLOG_CATEGORY_HEADER_SIZE) - pklog_category[category]->index);
			console_output(&pklog_category[category]->buffer[0], pklog_category[category]->index - 1);
		}
	}

	len = snprintf(strbuf, sizeof(strbuf), "\n============================== KLog End ==============================\n");
	console_output(strbuf, len);

	return;
}

//Show all logs to all the console drivers. Ex. UART
void show_android_log_to_console(void)
{
	int i = 0;

	for(i = 0; i < KLOG_IGNORE; i++)
	{
		__show_android_log_to_console(i);
	}

	return;
}
EXPORT_SYMBOL(show_android_log_to_console);

int get_magic_index(char *magic)
{
	char buf[KLOG_MAGIC_LENGTH + 1] = {0};

	int i = 0;

	if(magic && strlen(magic) > 0)
	{
		strncpy(buf, magic, KLOG_MAGIC_LENGTH);

		for(i = 0; i < sizeof(kml) / sizeof(struct klog_magic_list); i++)
		{
			if(!strncmp(magic, kml[i].name, strlen(kml[i].name)))
			{
				return kml[i].index;
			}
		}
	}

	return KLOG_INDEX_INVALID;
}

int get_magic_priority(char *magic)
{
	char buf[KLOG_MAGIC_LENGTH + 1] = {0};

	int i = 0;

	if(magic && strlen(magic) > 0)
	{
		strncpy(buf, magic, KLOG_MAGIC_LENGTH);

		for(i = 0; i < sizeof(kml) / sizeof(struct klog_magic_list); i++)
		{
			if(!strncmp(magic, kml[i].name, strlen(kml[i].name)))
			{
				return kml[i].priority;
			}
		}
	}

	return KLOG_PRIORITY_INVALID;
}

int match_crash_priority(int priority)
{
	if(priority == KLOG_PRIORITY_MARM_FATAL || priority == KLOG_PRIORITY_AARM_PANIC || priority == KLOG_PRIORITY_RPM_CRASH || priority == KLOG_PRIORITY_FIQ_HANG)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void update_priority(void)
{
	if(crashflag_inited == 0)
	{
		crashflag_inited = 1;
#ifdef STUFF_CRASH_DEFAULT
		crashflag = *(unsigned int *)(klog_magic + CCI_KLOG_SIZE - sizeof(int) * 2);
		*(unsigned int *)(klog_magic + CCI_KLOG_SIZE - sizeof(int) * 2) = STUFF_CRASH_DEFAULT;
#endif // #ifdef STUFF_CRASH_DEFAULT
	}
	if(unknownrebootflag_inited == 0)
	{
		unknownrebootflag_inited = 1;
#ifdef CONFIG_WARMBOOT_CRASH
		unknownrebootflag = *(unsigned int *)(klog_magic + CCI_KLOG_SIZE - sizeof(int) * 4);
		*(unsigned int *)(klog_magic + CCI_KLOG_SIZE - sizeof(int) * 4) = CONFIG_WARMBOOT_CRASH;
#endif // #ifdef CONFIG_WARMBOOT_CRASH
	}

	if(crashflag_inited == 1 || unknownrebootflag_inited == 1)
	{
#ifdef CONFIG_WARMBOOT_CRASH
		if((warmboot == CONFIG_WARMBOOT_CRASH || crashflag == CONFIG_WARMBOOT_CRASH || unknownrebootflag == CONFIG_WARMBOOT_CRASH)
#else // #ifdef CONFIG_WARMBOOT_CRASH
		if(warmboot == 0xC0DEDEAD
#endif // #ifdef CONFIG_WARMBOOT_CRASH
			&& match_crash_priority(get_magic_priority(klog_magic)) == 0)//crash happened before, but klog magic was not recorded as crashed, so overwrite the klog magic to FIQ hang
		{
			strncpy(klog_magic, KLOG_MAGIC_FIQ_HANG, KLOG_MAGIC_LENGTH);
		}
	}

	if(magic_priority == KLOG_PRIORITY_INVALID)
	{
		if(klog_magic && strlen(klog_magic) > 0)
		{
			magic_priority = get_magic_priority(klog_magic);
//check if previous crash
			if(match_crash_priority(magic_priority) > 0)
			{
				crash_state = crash_state | 0x02;
			}
		}
	}
}

void cklc_save_magic(char *magic, int state)
{
	char buf[KLOG_MAGIC_TOTAL_LENGTH] = {0};
	int priority = KLOG_PRIORITY_INVALID;
	int magic_update = 0;

	if(klog_magic && strlen(klog_magic) > 0)
	{
		kprintk("klog_magic=%s\n", klog_magic);
	}

	if(magic_priority == KLOG_PRIORITY_INVALID)
	{
		update_priority();
	}

	if(magic && strlen(magic) > 0)
	{
		strncpy(buf, magic, KLOG_MAGIC_LENGTH);
		strncpy(buf + KLOG_MAGIC_LENGTH, klog_magic + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
		kprintk("preset magic(prepare):magic=%s, state=%d, buf=%s\n", magic, state, buf);
#endif // #ifdef CCI_KLOG_DETAIL_LOG

		priority = get_magic_priority(buf);

		if(match_crash_priority(priority) > 0)//mARM fatal or aARM panic is happening, i.e., crashing
		{
			crash_state = crash_state | 0x01;
		}
	}
	else
	{
		priority = KLOG_PRIORITY_INVALID;
	}

#ifdef CCI_KLOG_DETAIL_LOG
	kprintk("magic_priority=%d, priority=%d, state=%d\n", magic_priority, priority, state);
#endif // #ifdef CCI_KLOG_DETAIL_LOG

//magic
	if(magic_priority == KLOG_PRIORITY_INVALID || !strncmp(klog_magic, KLOG_MAGIC_POWER_OFF, KLOG_MAGIC_LENGTH))//cold-boot
	{
		state = 0;
		if(priority == KLOG_PRIORITY_INVALID)//invalid magic, init klog with default magic
		{
			magic_update = 1;
			magic_priority = KLOG_PRIORITY_KLOG_INIT;
			snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
			kprintk("preset magic(cold-boot invalid):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
		}
		else//valid magic, init klog with specified magic
		{
			magic_update = 1;
			magic_priority = priority;
			strncpy(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_INIT_CODE, KLOG_STATE_LENGTH);
			kprintk("preset magic(cold-boot valid):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
		}
	}
	else//warn-boot
	{
		if(priority == KLOG_PRIORITY_INVALID)//invalid magic, do not update magic
		{
			strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
			kprintk("preset magic(warn-boot invalid):state=%d, buf=%s\n", state, buf);
		}
		else//valid magic
		{
			if(!strncmp(buf, KLOG_MAGIC_FORCE_CLEAR, KLOG_MAGIC_LENGTH))//force clear
			{
				state = 0;
				if((crash_state & 0x01) > 0)//not allow force clear magic if crashing
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					kprintk("preset magic(!force):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
				else
				{
					magic_update = 1;
					crash_state = 0;
					magic_priority = KLOG_PRIORITY_KLOG_INIT;
					snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
					kprintk("preset magic(force):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
				}
			}
			else if(!strncmp(buf, KLOG_MAGIC_INIT, KLOG_MAGIC_LENGTH))//klog init
			{
				state = 0;
				if(crash_state == 0)//only allow to clear if not any crash
				{
					magic_update = 1;
					magic_priority = KLOG_PRIORITY_KLOG_INIT;
					snprintf(buf, KLOG_MAGIC_TOTAL_LENGTH, "%s%s", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE);
					kprintk("preset magic(init):magic=%s, state=%s, buf=%s\n", KLOG_MAGIC_INIT, KLOG_STATE_INIT_CODE, buf);
				}
				else
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					kprintk("preset magic(!init):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
			}
			else
			{
				if(priority < magic_priority)//higher priority magic, update magic
				{
					magic_update = 1;
					magic_priority = priority;
					kprintk("preset magic(warn-boot higher):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
				else//lower or same priority magic, do not update magic
				{
					strncpy(buf, klog_magic, KLOG_MAGIC_LENGTH + KLOG_STATE_LENGTH);
					kprintk("preset magic(warn-boot lower):magic=%s, state=%d, buf=%s\n", magic, state, buf);
				}
			}
		}
	}

//state
	if(state != 0)
	{
		if(state == KLOG_STATE_INIT)
		{
			if(crash_state == 0)//only allow to clear if not any crash
			{
				strncpy(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_INIT_CODE, KLOG_STATE_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("preset state(init):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else//panic, do not update state
			{
				strncpy(buf + KLOG_MAGIC_LENGTH, klog_magic + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("preset state(panic):magic=%s, state=%s, buf=%s\n", magic, KLOG_STATE_INIT_CODE, buf);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
		}
		else
		{
			if(magic_priority != KLOG_PRIORITY_INVALID)//klog inited, the state should be a valid value
			{
				state = state | simple_strtoul(buf + KLOG_MAGIC_LENGTH, NULL, 10);
			}

			state = state & 0xFFF;
			snprintf(buf + KLOG_MAGIC_LENGTH, KLOG_STATE_LENGTH + 1, "%03X", state);
#ifdef CCI_KLOG_DETAIL_LOG
			kprintk("preset state(update):magic=%s, state=%d, buf=%s\n", magic, state, buf);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
		}
	}

//update
	snprintf(klog_magic, KLOG_MAGIC_TOTAL_LENGTH, "%s", buf);
	kprintk("new klog_magic:klog_magic=%s, buf=%s\n", klog_magic, buf);

//hw_id
#ifdef DVT1_BOARD_HW_ID
	if(magic_update == 1 && device_info_update == 0)
	{
		device_info_update = 1;
		snprintf(klog_magic + KLOG_INFO_LENGTH + KLOG_IMAGE_INFO_LENGTH, KLOG_HW_ID_LENGTH, "%01X%01X%01X%01X", board_type_with_hw_id(), band_type_with_hw_id(), model_name_with_hw_id(), nfc_with_hw_id());
		kprintk("hw_info:board_type=%01X, band_type=%01X, model_name=%01X, nfc=%01X\n", board_type_with_hw_id(), band_type_with_hw_id(), model_name_with_hw_id(), nfc_with_hw_id());
	}
#endif // #ifdef DVT1_BOARD_HW_ID

#ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
	klog_record_kernel_timestamp(NULL);
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_KERNEL_TIMESTAMP
}
EXPORT_SYMBOL(cklc_save_magic);

void cklc_set_memory_ready(void)
{
	mem_ready = 1;
}
EXPORT_SYMBOL(cklc_set_memory_ready);

//Reinitialize categories for collecting logs again
void clear_klog(void)
{
//Backup KLOG status
	int org_mem_ready = mem_ready;
	int org_mem_have_clean = 1;

	int i = 0;

	mem_ready = 0;//temporary change to uninitialized

	cklc_save_magic(KLOG_MAGIC_FORCE_CLEAR, KLOG_STATE_INIT);

//Clear Each KLOG Buffer
	for(i = 0; i < KLOG_IGNORE; i++)
	{
#if CCI_KLOG_APPSBL_SIZE
		if(i == KLOG_APPSBL)//skip APPSBL
		{
			continue;
		}
#endif // #if CCI_KLOG_APPSBL_SIZE
		pklog_category[i]->index = 0;
		pklog_category[i]->overload = 0;
		memset(&pklog_category[i]->buffer[0], 0, pklog_category[i]->size - KLOG_CATEGORY_HEADER_SIZE);
	}

//Restore to KLOG status
	mem_have_clean = org_mem_have_clean;
	mem_ready = org_mem_ready;

	kprintk("clear all categories done\n");

	return;
}

void record_suspend_resume_time(int state, unsigned int time)
{
	if(state == 1)//suspending
	{
		sysinfo.suspend_time = suspend_time;
	}
	else if(state == 3)//resuming
	{
		sysinfo.resume_time = resume_time;
	}
	else//other
	{
		kprintk("invalid state %d for record suspend/resume time\n", state);
		return;
	}

//update suspend/resume time to klog header area
	memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_NORMAL_BOOT_LENGTH + KLOG_SYSTEM_ON_OFF_STATE_LENGTH + KLOG_SBL_BOOTUP_TIME_LENGTH + KLOG_ABOOT_BOOTUP_TIME_LENGTH + KLOG_ANDROID_BOOTUP_TIME_LENGTH + KLOG_ANDROID_SHUTDOWN_TIME_LENGTH + KLOG_SYSFS_SYNC_TIME_LENGTH + KLOG_KERNEL_POWER_OFF_TIME_LENGTH, &sysinfo.suspend_time, KLOG_SUSPEND_TIME_LENGTH + KLOG_RESUME_TIME_LENGTH);

	return;
}

void record_shutdown_time(int state)
{
	struct klog_time current_time;

	switch(state % 0x10)
	{
		case 1://user confirmed to power-off
			if(sysinfo.system_on_off_state / 0x10 > 0)
			{
				sysinfo.system_on_off_state = state;
				system_shutdown_time = klog_record_kernel_timestamp(NULL);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u\n", sysinfo.system_on_off_state, (unsigned int)system_shutdown_time.clock.tv_sec, (unsigned int)system_shutdown_time.clock.tv_nsec);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		case 2://perform low-level power-off
			if(sysinfo.system_on_off_state == 0x01)
			{
				sysinfo.system_on_off_state = state;
				current_time = klog_record_kernel_timestamp(NULL);
//compute Android shutdown time
				sysinfo.android_shutdown_time = (unsigned int)((current_time.clock.tv_sec - system_shutdown_time.clock.tv_sec) * 1000 + (current_time.clock.tv_nsec - system_shutdown_time.clock.tv_nsec) / 1000000);//ms
//update Android shutdown time to klog header area
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_NORMAL_BOOT_LENGTH, &sysinfo.system_on_off_state, KLOG_SYSTEM_ON_OFF_STATE_LENGTH + KLOG_SBL_BOOTUP_TIME_LENGTH + KLOG_ABOOT_BOOTUP_TIME_LENGTH + KLOG_ANDROID_BOOTUP_TIME_LENGTH + KLOG_ANDROID_SHUTDOWN_TIME_LENGTH + KLOG_SYSFS_SYNC_TIME_LENGTH + KLOG_KERNEL_POWER_OFF_TIME_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u, android_shutdown_time=%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec, sysinfo.android_shutdown_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		case 3://start sync, emergency remount
			if(sysinfo.system_on_off_state == 0x01)//in case that property_set is not fast enough
			{
				sysinfo.system_on_off_state = 0x02;
				current_time = klog_record_kernel_timestamp(NULL);
//compute Android shutdown time
				sysinfo.android_shutdown_time = (unsigned int)((current_time.clock.tv_sec - system_shutdown_time.clock.tv_sec) * 1000 + (current_time.clock.tv_nsec - system_shutdown_time.clock.tv_nsec) / 1000000);//ms
//update Android shutdown time to klog header area
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_NORMAL_BOOT_LENGTH, &sysinfo.system_on_off_state, KLOG_SYSTEM_ON_OFF_STATE_LENGTH + KLOG_SBL_BOOTUP_TIME_LENGTH + KLOG_ABOOT_BOOTUP_TIME_LENGTH + KLOG_ANDROID_BOOTUP_TIME_LENGTH + KLOG_ANDROID_SHUTDOWN_TIME_LENGTH + KLOG_SYSFS_SYNC_TIME_LENGTH + KLOG_KERNEL_POWER_OFF_TIME_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u, android_shutdown_time=%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec, sysinfo.android_shutdown_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			if(sysinfo.system_on_off_state / 0x10 > 0 || sysinfo.system_on_off_state == 0x02)
			{
				sysinfo.system_on_off_state = state;
				current_time = klog_record_kernel_timestamp(NULL);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		case 4://sync, emergency remount done
			if(sysinfo.system_on_off_state == 0x03)
			{
				sysinfo.system_on_off_state = state;
				current_time = klog_record_kernel_timestamp(NULL);
//compute fs sync time(sync = system - android)
				sysinfo.sysfs_sync_time = (unsigned int)((current_time.clock.tv_sec - system_shutdown_time.clock.tv_sec) * 1000 + (current_time.clock.tv_nsec - system_shutdown_time.clock.tv_nsec) / 1000000) - sysinfo.android_shutdown_time;//ms
//update fs sync time to klog header area
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_NORMAL_BOOT_LENGTH, &sysinfo.system_on_off_state, KLOG_SYSTEM_ON_OFF_STATE_LENGTH + KLOG_SBL_BOOTUP_TIME_LENGTH + KLOG_ABOOT_BOOTUP_TIME_LENGTH + KLOG_ANDROID_BOOTUP_TIME_LENGTH + KLOG_ANDROID_SHUTDOWN_TIME_LENGTH + KLOG_SYSFS_SYNC_TIME_LENGTH + KLOG_KERNEL_POWER_OFF_TIME_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u, sysfs_sync_time=%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec, sysinfo.sysfs_sync_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		case 5://kernel reboot
			if(sysinfo.system_on_off_state == 0x04)
			{
				sysinfo.system_on_off_state = state;
				current_time = klog_record_kernel_timestamp(NULL);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		case 6://soc power-off
			if(sysinfo.system_on_off_state == 0x05)
			{
				sysinfo.system_on_off_state = state;
				current_time = klog_record_kernel_timestamp(NULL);
//compute kernel power-off time(kernel = system - android - sync)
				system_shutdown_time.clock.tv_sec = current_time.clock.tv_sec - system_shutdown_time.clock.tv_sec;
				system_shutdown_time.clock.tv_nsec = current_time.clock.tv_nsec - system_shutdown_time.clock.tv_nsec;
				sysinfo.kernel_power_off_time = (unsigned int)(system_shutdown_time.clock.tv_sec * 1000 + system_shutdown_time.clock.tv_nsec / 1000000) - sysinfo.android_shutdown_time - sysinfo.sysfs_sync_time;//ms
//update kernel power-off time to klog header area
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH + KLOG_NORMAL_BOOT_LENGTH, &sysinfo.system_on_off_state, KLOG_SYSTEM_ON_OFF_STATE_LENGTH + KLOG_SBL_BOOTUP_TIME_LENGTH + KLOG_ABOOT_BOOTUP_TIME_LENGTH + KLOG_ANDROID_BOOTUP_TIME_LENGTH + KLOG_ANDROID_SHUTDOWN_TIME_LENGTH + KLOG_SYSFS_SYNC_TIME_LENGTH + KLOG_KERNEL_POWER_OFF_TIME_LENGTH);
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo(shutdown:0x%X):timestamp=%u.%u, kernel_power_off_time=%u, system_shutdown_time=%u\n", sysinfo.system_on_off_state, (unsigned int)current_time.clock.tv_sec, (unsigned int)current_time.clock.tv_nsec, sysinfo.kernel_power_off_time, (unsigned int)(system_shutdown_time.clock.tv_sec * 1000 + system_shutdown_time.clock.tv_nsec / 1000000));
#endif // #ifdef CCI_KLOG_DETAIL_LOG
			}
			else
			{
				kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			}
			break;

		default:
			kprintk("sysinfo(shutdown:0x%X):invalid state 0x%X\n", sysinfo.system_on_off_state, state);
			break;
	}
}

static long klog_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int flag = 0;
	struct system_information si;
#ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE
#ifndef CCI_ENGMODE_WORKAROUND
	int index = 0;
	int recent = 0;
	int counter = 0;
#endif // #ifndef CCI_ENGMODE_WORKAROUND
	int i = 0;
#endif // #ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE

	switch(cmd)
	{
		case KLOG_IOCTL_CHECK_OLD_LOG:
			flag = get_magic_index(klog_magic);
			if(copy_to_user((void *)arg, &flag ,sizeof(int)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_CLEAR_LOG:
			clear_klog();
			break;

		case KLOG_IOCTL_RECORD_SYSINFO:
			if(copy_from_user(&si, (void __user *)arg, sizeof(struct system_information)))
			{
				return -EFAULT;
			}

//check system is on or off
			if(si.system_on_off_state / 0x10 > 0)//power-on
			{
				switch(si.system_on_off_state % 0x10)
				{
					case 0://klogcat init
//set sysinfo
						memcpy(&sysinfo, &si, sizeof(struct system_information));

//recover rpm_version
#ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
						strncpy(sysinfo.rpm_version, rpm_version, KLOG_RPM_VERSION_LENGTH - 1);
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION

//recover bootloader_version
						strncpy(sysinfo.bootloader_version, bootloader_version, KLOG_BOOTLOADER_VERSION_LENGTH - 1);

//recover hw_id
#ifdef DVT1_BOARD_HW_ID
						snprintf(sysinfo.hw_id, KLOG_HW_ID_LENGTH, "%01X%01X%01X%01X", board_type_with_hw_id(), band_type_with_hw_id(), model_name_with_hw_id(), nfc_with_hw_id());
#endif // #ifdef DVT1_BOARD_HW_ID

//recover bootup and previous shutdown info
						memcpy(&si, klog_magic, sizeof(struct system_information));
#ifdef CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
#ifdef IMEM_CERT_RECORD
//check it is valid or not
						if(*cci_imem_magic == IMEM_CERT_RECORD)
						{
							sysinfo.sbl_bootup_time = *sbl_bootup_time / 1000;//ms
						}
						else
						{
							sysinfo.sbl_bootup_time = 0;
						}
#else // #ifdef IMEM_CERT_RECORD
						sysinfo.sbl_bootup_time = *sbl_bootup_time / 1000;//ms
#endif // #ifdef IMEM_CERT_RECORD
#else // #ifdef CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
						sysinfo.sbl_bootup_time = si.sbl_bootup_time / 1000;//ms
#endif // #ifdef CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
						sysinfo.aboot_bootup_time = si.aboot_bootup_time;//ms
						if(si.system_on_off_state == 0x06)//completed power-off
						{
#ifdef CCI_KLOG_DETAIL_LOG
							kprintk("sysinfo:system_on_off_state=0x%X, android_shutdown_time=%u, sysfs_sync_time=%u, kernel_power_off_time=%u\n", si.system_on_off_state, si.android_shutdown_time, si.sysfs_sync_time, si.kernel_power_off_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
							sysinfo.android_shutdown_time = si.android_shutdown_time;
							sysinfo.sysfs_sync_time = si.sysfs_sync_time;
							sysinfo.kernel_power_off_time = si.kernel_power_off_time;
						}
						break;

					case 1://klogcat boot
						sysinfo.system_on_off_state = si.system_on_off_state;
//boot_time
						system_bootup_time = klog_record_kernel_timestamp(NULL);
//save bootup info
						sysinfo.android_bootup_time = (unsigned int)(system_bootup_time.clock.tv_sec * 1000 + system_bootup_time.clock.tv_nsec / 1000000);//ms
						break;

					case 2://klogcat sysinfo
//update sysinfo
						strncpy(sysinfo.modem_version, si.modem_version, KLOG_MODEM_VERSION_LENGTH - 1);
						strncpy(sysinfo.scaling_max_freq, si.scaling_max_freq, KLOG_SCALING_MAX_FREQ_LENGTH - 1);
						strncpy(sysinfo.sim_state, si.sim_state, KLOG_SIM_STATE_LENGTH - 1);
						break;
				}
			}
			else//power-off
			{
				record_shutdown_time(si.system_on_off_state);
			}

//record sysinfo to header
			if((crash_state & 0x02) > 0)//not allow to overwrite if previous crash
			{
				kprintk("sysinfo:not allow to record sysinfo\n");
			}
			else
			{
#ifdef CCI_KLOG_DETAIL_LOG
				kprintk("sysinfo:klog_userdata_count=%s, klog_internal_count=%s, klog_external_count=%s, normal_boot=%s, android_version=%s, modem_version=%s, flex_version=%s, rpm_version=%s, bootloader_version=%s, linux_version=%s, version_id=%s, build_version=%s, build_date=%s, build_type=%s, build_user=%s, build_host=%s, build_key=%s, secure_mode=%s, debug_mode=%s, cpuinfo_max_freq=%s, scaling_max_freq=%s, system_on_off_state=0x%X, android_shutdown_time=%u, kernel_power_off_time=%u\n", sysinfo.klog_userdata_count, sysinfo.klog_internal_count, sysinfo.klog_external_count, sysinfo.normal_boot, sysinfo.android_version, sysinfo.modem_version, sysinfo.flex_version, sysinfo.rpm_version, sysinfo.bootloader_version, sysinfo.linux_version, sysinfo.version_id, sysinfo.build_version, sysinfo.build_date, sysinfo.build_type, sysinfo.build_user, sysinfo.build_host, sysinfo.build_key, sysinfo.secure_mode, sysinfo.debug_mode, sysinfo.cpuinfo_max_freq, sysinfo.scaling_max_freq, sysinfo.system_on_off_state, sysinfo.android_shutdown_time, sysinfo.kernel_power_off_time);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
				memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH, &sysinfo.normal_boot, sizeof(struct system_information) - (KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH));
			}
			break;

#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
		case KLOG_IOCTL_FORCE_PANIC:
			panic("klog_panic");
			break;

		case KLOG_IOCTL_FORCE_CRASH:
			*((int*)0) = 0;
			break;

		case KLOG_IOCTL_SET_PANIC_WHEN_SUSPEND:
			if(copy_from_user(&flag, (void __user *)arg, sizeof(int)))
			{
				return -EFAULT;
			}
			force_panic_when_suspend = flag;
			break;

		case KLOG_IOCTL_SET_PANIC_WHEN_POWER_OFF:
			if(copy_from_user(&flag, (void __user *)arg, sizeof(int)))
			{
				return -EFAULT;
			}
			force_panic_when_power_off = flag;
			break;
#endif // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC

		case KLOG_IOCTL_GET_HEADER:
			memcpy(&sysinfo, klog_magic, sizeof(struct system_information));
			if(copy_to_user((void *)arg, &sysinfo, sizeof(struct system_information)))
			{
				return -EFAULT;
			}
			break;

#if CCI_KLOG_CRASH_SIZE
		case KLOG_IOCTL_GET_CRASH:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_CRASH]->name[0], pklog_category[KLOG_CRASH]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_CRASH_SIZE

#if CCI_KLOG_APPSBL_SIZE
		case KLOG_IOCTL_GET_APPSBL:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_APPSBL]->name[0], pklog_category[KLOG_APPSBL]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_APPSBL_SIZE

#if CCI_KLOG_KERNEL_SIZE
		case KLOG_IOCTL_GET_KERNEL:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_KERNEL]->name[0], pklog_category[KLOG_KERNEL]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_KERNEL_SIZE

#if CCI_KLOG_ANDROID_MAIN_SIZE
		case KLOG_IOCTL_GET_ANDROID_MAIN:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_ANDROID_MAIN]->name[0], pklog_category[KLOG_ANDROID_MAIN]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_ANDROID_MAIN_SIZE

#if CCI_KLOG_ANDROID_SYSTEM_SIZE
		case KLOG_IOCTL_GET_ANDROID_SYSTEM:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_ANDROID_SYSTEM]->name[0], pklog_category[KLOG_ANDROID_SYSTEM]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_ANDROID_SYSTEM_SIZE

#if CCI_KLOG_ANDROID_RADIO_SIZE
		case KLOG_IOCTL_GET_ANDROID_RADIO:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_ANDROID_RADIO]->name[0], pklog_category[KLOG_ANDROID_RADIO]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_ANDROID_RADIO_SIZE

#if CCI_KLOG_ANDROID_EVENTS_SIZE
		case KLOG_IOCTL_GET_ANDROID_EVENTS:
			if(copy_to_user((void *)arg, &pklog_category[KLOG_ANDROID_EVENTS]->name[0], pklog_category[KLOG_ANDROID_EVENTS]->size))
			{
				return -EFAULT;
			}
			break;
#endif // #if CCI_KLOG_ANDROID_EVENTS_SIZE

		case KLOG_IOCTL_GET_RESERVE:
/*
			if(copy_to_user((void *)arg, &pklog_category[KLOG_RESERVE]->name[0], pklog_category[KLOG_RESERVE]->size))
			{
				return -EFAULT;
			}
*/
			break;

#ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE
		case KLOG_IOCTL_GET_BOOTINFO:
#ifdef CCI_ENGMODE_WORKAROUND
			{
				struct boot_information bi;

				if(bootinfo_inited == 0)
				{
					if(copy_from_user(&bi, (void __user *)arg, sizeof(struct boot_information)))
					{
						return -EFAULT;
					}

					bootinfo_inited = 1;

					for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
					{
#ifdef CCI_ENGMODE_LOG
						kprintk("bi.power_on_reason_recent[%d]=0x%X\n", i, bi.power_on_reason_recent[i]);
#endif // #ifdef CCI_ENGMODE_LOG
						bootinfo.power_on_reason_recent[i] = bi.power_on_reason_recent[i];
					}
					for(i = 0; i < POWER_REASON_COUNTER_LENGTH; i++)
					{
#ifdef CCI_ENGMODE_LOG
						kprintk("bi.power_on_reason_counter[%d]=%u\n", i, bi.power_on_reason_counter[i]);
#endif // #ifdef CCI_ENGMODE_LOG
						bootinfo.power_on_reason_counter[i] = bi.power_on_reason_counter[i];
					}
					for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
					{
#ifdef CCI_ENGMODE_LOG
						kprintk("bi.power_off_reason_recent[%d]=0x%X\n", i, bi.power_off_reason_recent[i]);
#endif // #ifdef CCI_ENGMODE_LOG
						bootinfo.power_off_reason_recent[i] = bi.power_off_reason_recent[i];
					}
					for(i = 0; i < POWER_REASON_COUNTER_LENGTH; i++)
					{
#ifdef CCI_ENGMODE_LOG
						kprintk("bi.power_off_reason_counter[%d]=%u\n", i, bi.power_off_reason_counter[i]);
#endif // #ifdef CCI_ENGMODE_LOG
						bootinfo.power_off_reason_counter[i] = bi.power_off_reason_counter[i];
					}
				}
			}
#else // #ifdef CCI_ENGMODE_WORKAROUND
			if(bootinfo_inited == 0)
			{
				bootinfo_inited = 1;
//power-on reason history
				recent = *power_on_off_history;//latest index of recent power-on reason
				for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
				{
					index = (recent - i) < 0 ? (recent - i + POWER_REASON_RECENT_LENGTH) : (recent - i);//ring buffer
#ifdef CCI_ENGMODE_LOG
					kprintk("recent %d power-on reason:[%d]=0x%X\n", i, index, *(power_on_off_history + 1 + index));
#endif // #ifdef CCI_ENGMODE_LOG
					bootinfo.power_on_reason_recent[i] = *(power_on_off_history + 1 + index);
				}

//power-on reason counter
				counter = *(power_on_off_history + 11);//numbers of valid power-on counter
				memcpy(&bootinfo.power_on_reason_counter, power_on_off_history + 12, sizeof(unsigned int) * POWER_REASON_COUNTER_LENGTH);
				for(i = 0; i < counter; i++)
				{
#ifdef CCI_ENGMODE_LOG
					kprintk("power-on counter[%d]=%u\n", i, bootinfo.power_on_reason_counter[i]);
#endif // #ifdef CCI_ENGMODE_LOG
				}

//power-off reason history
				recent = *(power_on_off_history + 42);//latest index of recent power-off reason
				for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
				{
					index = (recent - i) < 0 ? (recent - i + POWER_REASON_RECENT_LENGTH) : (recent - i);//ring buffer
#ifdef CCI_ENGMODE_LOG
					kprintk("recent %d power-off reason:[%d]=0x%X\n", i, index, *(power_on_off_history + 43 + index));
#endif // #ifdef CCI_ENGMODE_LOG
					bootinfo.power_off_reason_recent[i] = *(power_on_off_history + 43 + index);
				}

//power-off reason counter
				counter = *(power_on_off_history + 53);//numbers of valid power-off counter
				memcpy(&bootinfo.power_off_reason_counter, power_on_off_history + 54, sizeof(unsigned int) * POWER_REASON_COUNTER_LENGTH);
				for(i = 0; i < counter; i++)
				{
#ifdef CCI_ENGMODE_LOG
					kprintk("power-off counter[%d]=%u\n", i, bootinfo.power_off_reason_counter[i]);
#endif // #ifdef CCI_ENGMODE_LOG
				}
			}
#endif // #ifdef CCI_ENGMODE_WORKAROUND

			bootinfo.sd_ram_dump_mode = sysinfo.debug_mode[0] - 0x30;
//crash_record(3=fatalm, 4=panica, 13=rpm, 14=fiq)
			bootinfo.crash_record[0] = bootinfo.power_off_reason_counter[2];
			bootinfo.crash_record[1] = bootinfo.power_off_reason_counter[3];
			bootinfo.crash_record[2] = bootinfo.power_off_reason_counter[12];
			bootinfo.crash_record[3] = bootinfo.power_off_reason_counter[13];
#ifdef CCI_ENGMODE_LOG
			kprintk("emergency_download_mode=%d\n", bootinfo.emergency_download_mode);
			kprintk("qct_download_mode=%d\n", bootinfo.qct_download_mode);
			kprintk("cci_download_mode=%d\n", bootinfo.cci_download_mode);
			kprintk("mass_storage_download_mode=%d\n", bootinfo.mass_storage_download_mode);
			kprintk("sd_download_mode=%d\n", bootinfo.sd_download_mode);
			kprintk("usb_ram_dump_mode=%d\n", bootinfo.usb_ram_dump_mode);
			kprintk("sd_ram_dump_mode=%d\n", bootinfo.sd_ram_dump_mode);
			kprintk("normal_boot_mode=%d\n", bootinfo.normal_boot_mode);
			kprintk("sd_boot_mode=%d\n", bootinfo.sd_boot_mode);
			kprintk("fastboot_mode=%d\n", bootinfo.fastboot_mode);
			kprintk("recovery_mode=%d\n", bootinfo.recovery_mode);
			kprintk("simple_test_mode=%d\n", bootinfo.simple_test_mode);
			kprintk("charging_only_mode=%d\n", bootinfo.charging_only_mode);
			kprintk("android_safe_mode=%d\n", bootinfo.android_safe_mode);
			kprintk("qct_secure_boot_mode=%d\n", bootinfo.qct_secure_boot_mode);
			kprintk("cci_secure_boot_mode=%d\n", bootinfo.cci_secure_boot_mode);
			for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
			{
				kprintk("power_on_reason_recent[%d]=0x%X\n", i, bootinfo.power_on_reason_recent[i]);
			}
			for(i = 0; i < POWER_REASON_COUNTER_LENGTH; i++)
			{
				kprintk("power_on_reason_counter[%d]=0x%X\n", i, bootinfo.power_on_reason_counter[i]);
			}
			for(i = 0; i < POWER_REASON_RECENT_LENGTH; i++)
			{
				kprintk("power_off_reason_recent[%d]=0x%X\n", i, bootinfo.power_off_reason_recent[i]);
			}
			for(i = 0; i < POWER_REASON_COUNTER_LENGTH; i++)
			{
				kprintk("power_off_reason_counter[%d]=0x%X\n", i, bootinfo.power_off_reason_counter[i]);
			}
			for(i = 0; i < POWER_REASON_COUNTER_LENGTH; i++)
			{
				kprintk("crash_record[%d]=0x%X\n", i, bootinfo.crash_record[i]);
			}
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &bootinfo, sizeof(struct boot_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_DEVICEINFO:
			{
				struct device_information di;

				if(deviceinfo_inited == 0)
				{
					if(copy_from_user(&di, (void __user *)arg, sizeof(struct device_information)))
					{
						return -EFAULT;
					}

					deviceinfo_inited = 1;

					deviceinfo.emmc = di.emmc;
#ifdef CCI_ENGMODE_LOG
					kprintk("modem_version=%s\n", di.modem_version);
#endif // #ifdef CCI_ENGMODE_LOG
					strncpy(sysinfo.modem_version, di.modem_version, KLOG_MODEM_VERSION_LENGTH - 1);
				}
			}
#ifdef DVT1_BOARD_HW_ID
			deviceinfo.hw_id = board_type_with_hw_id();
			deviceinfo.band_id = band_type_with_hw_id();
			deviceinfo.model_id = model_name_with_hw_id();
			deviceinfo.nfc_id = nfc_with_hw_id();
#endif // #ifdef DVT1_BOARD_HW_ID
			deviceinfo.cpuinfo_max_freq = simple_strtoul(sysinfo.cpuinfo_max_freq, NULL, 10);
			deviceinfo.scaling_max_freq = simple_strtoul(sysinfo.scaling_max_freq, NULL, 10);
			strncpy(deviceinfo.android_version, sysinfo.android_version, KLOG_ANDROID_VERSION_LENGTH - 1);
			strncpy(deviceinfo.modem_version, sysinfo.modem_version, KLOG_MODEM_VERSION_LENGTH - 1);
			strncpy(deviceinfo.flex_version, sysinfo.flex_version, KLOG_FLEX_VERSION_LENGTH - 1);
			strncpy(deviceinfo.rpm_version, sysinfo.rpm_version, KLOG_RPM_VERSION_LENGTH - 1);
			strncpy(deviceinfo.linux_version, sysinfo.linux_version, KLOG_LINUX_VERSION_LENGTH - 1);
			strncpy(deviceinfo.version_id, sysinfo.version_id, KLOG_VERSION_ID_LENGTH - 1);
			strncpy(deviceinfo.build_version, sysinfo.build_version, KLOG_BUILD_VERSION_LENGTH - 1);
			strncpy(deviceinfo.build_date, sysinfo.build_date, KLOG_BUILD_DATE_LENGTH - 1);
			strncpy(deviceinfo.build_type, sysinfo.build_type, KLOG_BUILD_TYPE_LENGTH - 1);
			strncpy(deviceinfo.build_user, sysinfo.build_user, KLOG_BUILD_USER_LENGTH - 1);
			strncpy(deviceinfo.build_host, sysinfo.build_host, KLOG_BUILD_HOST_LENGTH - 1);
			strncpy(deviceinfo.build_key, sysinfo.build_key, KLOG_BUILD_KEY_LENGTH - 1);
			deviceinfo.secure_mode = sysinfo.secure_mode[0] - 0x30;
#ifdef CCI_ENGMODE_LOG
			kprintk("hw_id=%d\n", deviceinfo.hw_id);
			kprintk("band_id=%d\n", deviceinfo.band_id);
			kprintk("main_chip=%s\n", deviceinfo.main_chip);
			kprintk("radio_chip=%s\n", deviceinfo.radio_chip);
			kprintk("cpuinfo_max_freq=%u\n", deviceinfo.cpuinfo_max_freq);
			kprintk("scaling_max_freq=%u\n", deviceinfo.scaling_max_freq);
			kprintk("ram=%u\n", deviceinfo.ram);
			kprintk("flash=%u\n", deviceinfo.flash);
			kprintk("emmc=%u\n", deviceinfo.emmc);
			kprintk("hw_reset=%d\n", deviceinfo.hw_reset);
			kprintk("android_version=%s\n", deviceinfo.android_version);
			kprintk("modem_version=%s\n", deviceinfo.modem_version);
			kprintk("flex_version=%s\n", deviceinfo.flex_version);
			kprintk("rpm_version=%s\n", deviceinfo.rpm_version);
			kprintk("bootloader_version=%s\n", deviceinfo.bootloader_version);
			kprintk("linux_version=%s\n", deviceinfo.linux_version);
			kprintk("version_id=%s\n", deviceinfo.version_id);
			kprintk("build_version=%s\n", deviceinfo.build_version);
			kprintk("build_date=%s\n", deviceinfo.build_date);
			kprintk("build_type=%s\n", deviceinfo.build_type);
			kprintk("build_user=%s\n", deviceinfo.build_user);
			kprintk("build_host=%s\n", deviceinfo.build_host);
			kprintk("build_key=%s\n", deviceinfo.build_key);
			kprintk("secure_mode=%d\n", deviceinfo.secure_mode);
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &deviceinfo, sizeof(struct device_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_DEBUGINFO:
			{
				struct debug_information di;

				if(debuginfo_inited == 0)
				{
					if(copy_from_user(&di, (void __user *)arg, sizeof(struct debug_information)))
					{
						return -EFAULT;
					}

					debuginfo_inited = 1;
				}
			}
			debuginfo.debug_mode = sysinfo.debug_mode[0] - 0x30;
			debuginfo.usb_diag = sysinfo.debug_mode[0] - 0x30;
			debuginfo.crash_ram_dump = sysinfo.debug_mode[0] - 0x30;
#ifdef CCI_ENGMODE_LOG
			kprintk("debug_mode=%d\n", debuginfo.debug_mode);
			kprintk("usb_sbl_diag=%d\n", debuginfo.usb_sbl_diag);
			kprintk("usb_diag=%d\n", debuginfo.usb_diag);
			kprintk("usb_adb=%d\n", debuginfo.usb_adb);
			kprintk("uart=%d\n", debuginfo.uart);
			kprintk("jtag=%d\n", debuginfo.jtag);
			kprintk("hot_key_ram_dump=%d\n", debuginfo.hot_key_ram_dump);
			kprintk("command_ram_dump=%d\n", debuginfo.command_ram_dump);
			kprintk("crash_ram_dump=%d\n", debuginfo.crash_ram_dump);
			kprintk("bootloader_log=%d\n", debuginfo.bootloader_log);
			kprintk("kernel_log=%d\n", debuginfo.kernel_log);
			kprintk("logcat_log=%d\n", debuginfo.logcat_log);
			kprintk("klog_log=%d\n", debuginfo.klog_log);
			kprintk("rpm_log=%d\n", debuginfo.rpm_log);
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &debuginfo, sizeof(struct debug_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_PERFORMANCEINFO:
			{
				struct performance_information pi;

				if(copy_from_user(&pi, (void __user *)arg, sizeof(struct performance_information)))
				{
					return -EFAULT;
				}

				if(pi.up_time > 0)
				{
					performanceinfo.up_time = pi.up_time;
					performanceinfo.idle_time = pi.idle_time;
					performanceinfo.sleep_time = pi.sleep_time;
				}
			}
			performanceinfo.bootup_time = sysinfo.sbl_bootup_time + sysinfo.aboot_bootup_time + sysinfo.android_bootup_time;
			performanceinfo.shutdown_time = sysinfo.android_shutdown_time + sysinfo.kernel_power_off_time;
			performanceinfo.suspend_time = sysinfo.suspend_time;
			performanceinfo.resume_time = sysinfo.resume_time;
#ifdef CCI_ENGMODE_LOG
			kprintk("bootup_time(sbl:%u, aboot:%u, kernel:%u)=%u\n", sysinfo.sbl_bootup_time, sysinfo.aboot_bootup_time, sysinfo.android_bootup_time, performanceinfo.bootup_time);
			kprintk("shutdown_time(android:%u, kernel:%u)=%u\n", sysinfo.android_shutdown_time, sysinfo.kernel_power_off_time, performanceinfo.shutdown_time);
			kprintk("suspend_time=%u\n", performanceinfo.suspend_time);
			kprintk("resume_time=%u\n", performanceinfo.resume_time);
			kprintk("up_time=%u\n", performanceinfo.up_time);
			kprintk("idle_time=%u\n", performanceinfo.idle_time);
			kprintk("sleep_time=%u\n", performanceinfo.sleep_time);
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &performanceinfo, sizeof(struct performance_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_WARRANTYINFO:
#ifdef CCI_ENGMODE_WORKAROUND
			{
				struct warranty_information wi;

				if(warrantyinfo_inited == 0)
				{
					if(copy_from_user(&wi, (void __user *)arg, sizeof(struct warranty_information)))
					{
						return -EFAULT;
					}

					warrantyinfo_inited = 1;

#ifdef CCI_ENGMODE_LOG
					kprintk("wi.total_deviceon_time=%s\n", wi.total_deviceon_time);
#endif // #ifdef CCI_ENGMODE_LOG
					strncpy(warrantyinfo.total_deviceon_time, wi.total_deviceon_time, 31);
#ifdef CCI_ENGMODE_LOG
					kprintk("wi.first_voice_call_timestamp=%s\n", wi.first_voice_call_timestamp);
#endif // #ifdef CCI_ENGMODE_LOG
					strncpy(warrantyinfo.first_voice_call_timestamp, wi.first_voice_call_timestamp, 31);
#ifdef CCI_ENGMODE_LOG
					kprintk("wi.first_data_call_timestamp=%s\n", wi.first_data_call_timestamp);
#endif // #ifdef CCI_ENGMODE_LOG
					strncpy(warrantyinfo.first_data_call_timestamp, wi.first_data_call_timestamp, 31);
				}
			}
#else // #ifdef CCI_ENGMODE_WORKAROUND
			if(warrantyinfo_inited == 0)
			{
				warrantyinfo_inited = 1;

//total_deviceon_time
				memcpy(warrantyinfo.total_deviceon_time, warranty_data, TOTAL_DEVICE_ON_TIME);
#ifdef CCI_ENGMODE_LOG
				kprintk("warrantyinfo.total_deviceon_time=%s\n", warrantyinfo.total_deviceon_time);
#endif // #ifdef CCI_ENGMODE_LOG
//first_voice_call_timestamp
				memcpy(warrantyinfo.first_voice_call_timestamp, warranty_data + TOTAL_DEVICE_ON_TIME, FIRST_VOICE_CALL_TIMESTAMP);
#ifdef CCI_ENGMODE_LOG
				kprintk("warrantyinfo.first_voice_call_timestamp=%s\n", warrantyinfo.first_voice_call_timestamp);
#endif // #ifdef CCI_ENGMODE_LOG
//first_data_call_timestamp
				memcpy(warrantyinfo.first_data_call_timestamp, warranty_data + TOTAL_DEVICE_ON_TIME + FIRST_VOICE_CALL_TIMESTAMP, FIRST_DATA_CALL_TIMESTAMP);
#ifdef CCI_ENGMODE_LOG
				kprintk("warrantyinfo.first_data_call_timestamp=%s\n", warrantyinfo.first_data_call_timestamp);
#endif // #ifdef CCI_ENGMODE_LOG
			}
#endif // #ifdef CCI_ENGMODE_WORKAROUND
#ifdef CCI_ENGMODE_LOG
			kprintk("total_deviceon_time=%s\n", warrantyinfo.total_deviceon_time);
			kprintk("first_voice_call_timestamp=%s\n", warrantyinfo.first_voice_call_timestamp);
			kprintk("first_data_call_timestamp=%s\n", warrantyinfo.first_data_call_timestamp);
			kprintk("maintenance_record=%d\n", warrantyinfo.maintenance_record);
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &warrantyinfo, sizeof(struct warranty_information)))
			{
				return -EFAULT;
			}
			break;

		case KLOG_IOCTL_GET_FACTORYINFO:
#ifdef CCI_ENGMODE_WORKAROUND
			{
				struct factory_information fi;

				if(factoryinfo_inited == 0)
				{
					if(copy_from_user(&fi, (void __user *)arg, sizeof(struct factory_information)))
					{
						return -EFAULT;
					}

					factoryinfo_inited = 1;

#ifdef CCI_ENGMODE_LOG
					kprintk("fi.backup_record=%d\n", fi.backup_record);
#endif // #ifdef CCI_ENGMODE_LOG
					factoryinfo.backup_record = fi.backup_record;
				}
			}
#else // #ifdef CCI_ENGMODE_WORKAROUND
			if(factoryinfo_inited == 0)
			{
				factoryinfo_inited = 1;

				factoryinfo.backup_record = *factory_record;
#ifdef CCI_ENGMODE_LOG
				kprintk("factoryinfo.backup_record=%d\n", factoryinfo.backup_record);
#endif // #ifdef CCI_ENGMODE_LOG
			}
#endif // #ifdef CCI_ENGMODE_WORKAROUND
#ifdef CCI_ENGMODE_LOG
			kprintk("backup_command=%d\n", factoryinfo.backup_command);
			kprintk("restore_command=%d\n", factoryinfo.restore_command);
			kprintk("backup_record=%d\n", factoryinfo.backup_record);
#endif // #ifdef CCI_ENGMODE_LOG
			if(copy_to_user((void *)arg, &factoryinfo, sizeof(struct factory_information)))
			{
				return -EFAULT;
			}
			break;
#endif // #ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE

		default:
			return -1;
	}

	return 0;
}

#ifdef CCI_KLOG_SUPPORT_ATTRIBUTE
#ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
static ssize_t klog_show_modem_log_addr(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 12, "0x%lX\n", modem_log_addr);//12 = "0x" + 8-digit hexdecimal number + '\n' + '\0'
}

static ssize_t klog_store_modem_log_addr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = simple_strtoul(buf, NULL, 10);

#ifdef CCI_KLOG_DETAIL_LOG
	kprintk("%s():val=0x%lX, modem_log_addr=0x%lX\n", __func__, val, modem_log_addr);
#endif // #ifdef CCI_KLOG_DETAIL_LOG
	modem_log_addr = val;

	return count;
}

KLOG_RW_DEV_ATTR(modem_log_addr);
#endif // #ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM

static ssize_t klog_show_prev_reboot_reason(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 8, "%s\n", previous_reboot_magic);//8 = 6-character magic + '\n' + '\0'
}

KLOG_RO_DEV_ATTR(prev_reboot_reason);

static ssize_t klog_show_prev_normal_boot(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", previous_normal_boot);//4 = 2-digit decimal number (-1 ~ 3) + '\n' + '\0'
}

KLOG_RO_DEV_ATTR(prev_normal_boot);

static struct attribute *klog_attrs[] =
{
#ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
	&dev_attr_modem_log_addr.attr,
#endif // #ifdef CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
	&dev_attr_prev_reboot_reason.attr,
	&dev_attr_prev_normal_boot.attr,
	NULL
};

static const struct attribute_group klog_attr_group =
{
//	.name	= "klog",
	.attrs	= klog_attrs,
};
#endif // #ifdef CCI_KLOG_SUPPORT_ATTRIBUTE

static const struct file_operations klog_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= klog_ioctl,
};

static struct miscdevice klog_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= KLOG_DEV_ID,
	.fops	= &klog_fops,
};

int klogmisc_init(void)
{
	int retval = 0;

//init miscdev
	retval = misc_register(&klog_miscdev);
	if(retval)
	{
		kprintk("cannot register as miscdev, err=0x%X\n", retval);
		goto error_init_miscdev;
	}

#ifdef CCI_KLOG_SUPPORT_ATTRIBUTE
//init sysfs
	retval = sysfs_create_group(&klog_miscdev.this_device->kobj, &klog_attr_group);
	if(retval < 0)
	{
		kprintk("init sysfs failed, err=0x%X\n", retval);
		goto error_init_sysfs;
	}
#endif // #ifdef CCI_KLOG_SUPPORT_ATTRIBUTE

	return 0;

#ifdef CCI_KLOG_SUPPORT_ATTRIBUTE
error_init_sysfs:
	sysfs_remove_group(&klog_miscdev.this_device->kobj, &klog_attr_group);
#endif // #ifdef CCI_KLOG_SUPPORT_ATTRIBUTE

error_init_miscdev:
	misc_deregister(&klog_miscdev);

	return retval;
}

void klogmisc_exit(void)
{
#ifdef CCI_KLOG_SUPPORT_ATTRIBUTE
	sysfs_remove_group(&klog_miscdev.this_device->kobj, &klog_attr_group);
#endif // #ifdef CCI_KLOG_SUPPORT_ATTRIBUTE

	misc_deregister(&klog_miscdev);
}

static int __init get_warmboot(char* cmdline)
{
	if(cmdline == NULL || strlen(cmdline) == 0)
	{
		kprintk("warmboot is empty\n");
	}
	else
	{
		warmboot = simple_strtoul(cmdline, NULL, 16);
		kprintk("warmboot:0x%lX\n", warmboot);
	}

	return 0;
}
__setup("warmboot=", get_warmboot);

static int __init get_startup(char* cmdline)
{
	if(cmdline == NULL || strlen(cmdline) == 0)
	{
		kprintk("startup is empty\n");
	}
	else
	{
		startup = simple_strtoul(cmdline, NULL, 16);
		kprintk("startup:0x%lX\n", startup);
	}

	return 0;
}
__setup("startup=", get_startup);

static int __init get_bootloader_version(char* cmdline)
{
	if(cmdline == NULL || strlen(cmdline) == 0)
	{
		kprintk("oemandroidboot.s1boot is empty\n");
	}
	else
	{
		strncpy(bootloader_version, cmdline, KLOG_BOOTLOADER_VERSION_LENGTH - 1);
		kprintk("oemandroidboot.s1boot:%s\n", bootloader_version);
	}

	return 0;
}
__setup("oemandroidboot.s1boot=", get_bootloader_version);

static int __init cklc_init(void)
{
	int retval = 0;
	int magic_index = 0;

//record previous normal boot
	previous_normal_boot = (klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH)[0] - 0x30;
	if(previous_normal_boot < 0 || previous_normal_boot > 3)
	{
		previous_normal_boot = -1;
	}

//record previous magic
	strncpy(previous_reboot_magic, klog_magic, KLOG_MAGIC_LENGTH);

//detect bootloader
	if(strlen(bootloader_version) == 0)
	{
		strncpy(bootloader_version, "CCI_bootloader", KLOG_BOOTLOADER_VERSION_LENGTH - 1);
	}

//detect charge-only mode
	magic_index = get_magic_index(klog_magic);
	if(startup == 0x4 && warmboot == 0)//charge-only mode
	{
		sysinfo.normal_boot[0] = '3';
		memcpy(klog_magic + KLOG_MAGIC_TOTAL_LENGTH + KLOG_KERNEL_TIME_LENGTH + KLOG_FIRST_RTC_TIMESTAMP_LENGTH + KLOG_LAST_RTC_TIMESTAMP_LENGTH, &sysinfo.normal_boot, sizeof(sysinfo.normal_boot));
	}

#ifdef STUFF_CRASH_DEFAULT
	kprintk("CCI KLog Init: prev_normal_boot=%d, crashflag=0x%X\n", previous_normal_boot, crashflag);
#else // #ifdef STUFF_CRASH_DEFAULT
	kprintk("CCI KLog Init: prev_normal_boot=%d\n", previous_normal_boot);
#endif // #ifdef STUFF_CRASH_DEFAULT

//init klog magic
	cklc_save_magic(KLOG_MAGIC_INIT, KLOG_STATE_INIT);

	retval = klogmisc_init();

	return retval;
}

static void __exit cklc_exit(void)
{
	kprintk("CCI KLog Exit\n");
	klogmisc_exit();

	return;
}

module_init(cklc_init);
module_exit(cklc_exit);

MODULE_AUTHOR("Kevin Chiang <Kevin_Chiang@Compalcomm.com>");
MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("CCI kernel and logcat log collector");
MODULE_VERSION(KLOG_VERSION);

/*
 * ------------------------------------------------------------------------------------
 * KLOG memory map
 *
 * address              : size      : field                     : note
 * ------------------------------------------------------------------------------------
 * 0x00000000-0x00000009:(0x00000A) : MSM_KLOG_MAGIC            : MAGIC:"CKLOGC###\0", please reference klog_magic_list
 * 0x0000000A-0x0000001D:(0x000014) : kernel time               : [%08lX.%08lX]\0
 * 0x0000001E-0x00000031:(0x000014) : first RTC timestamp       : [%08lX.%08lX]\0
 * 0x00000032-0x00000045:(0x000014) : last RTC timestamp        : [%08lX.%08lX]\0
 * 0x00000046-0x000003FF:(0x0003BA) : reserved                  :
 * ------------------------------------------------------------------------------------
 * 0x00000400-0x000023FF:(0x002000) : KLOG_CRASH                :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x002000-25
 * ------------------------------------------------------------------------------------
 * 0x00002400-0x00003BFF:(0x001800) : KLOG_APPSBL               :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x001800-25
 * ------------------------------------------------------------------------------------
 * 0x00003C00-0x00102BFF:(0x0FF000) : KLOG_KERNEL               :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x0FF000-25
 * ------------------------------------------------------------------------------------
 * 0x00102C00-0x00201BFF:(0x0FF000) : KLOG_ANDROID_MAIN         :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x0FF000-25
 * ------------------------------------------------------------------------------------
 * 0x00201C00-0x00300BFF:(0x0FF000) : KLOG_ANDROID_SYSTEM       :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x0FF000-25
 * ------------------------------------------------------------------------------------
 * 0x00300C00-0x003FFBFF:(0x0FF000) : KLOG_ANDROID_RADIO        :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x0FF000-25
 * ------------------------------------------------------------------------------------
 * 0x---------0x--------:(0x000000) : KLOG_ANDROID_EVENTS       :
 *                                  :                           : name             16
 *                                  :                           : size              4
 *                                  :                           : index             4
 *                                  :                           : overload          1
 *                                  :                           : buffer  0x000000-25
 * ------------------------------------------------------------------------------------
 * 0x003FFC00-0x003FFFFF:(0x000400) : RESERVE                   :
 * ------------------------------------------------------------------------------------

Ver		1.0.0		KB62 left latest version
Ver		1.1.0		Jimmy porting to DA80 Gingerbread
Ver		1.2.0		Jimmy implement KLog magic for power on/off reason
Ver		1.3.0		Jimmy implement KLog header for system information
Ver		1.4.0		Jimmy porting to DA80 IceCreamSandwich
Ver		1.5.0		Jimmy implement KLog category size customization for all categories and support APPSBL category
Ver		1.6.0		Jimmy implement KLog support CCI engmode tool
Ver		1.7.0		Jimmy implement KLog crash category
Ver		1.8.0		Jimmy porting to SA77 IceCreamSandwich
Ver		1.9.0		Jimmy porting to SA77 JellyBean
Ver		1.10.0		Jimmy implement KLog allocate memory in kernel and keep previous reboot reason
Ver		1.11.0		Jimmy implement KLog support S1 boot cmdline and crashflag
*/

