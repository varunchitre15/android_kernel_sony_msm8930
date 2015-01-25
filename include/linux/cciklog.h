#ifndef __KLOG_DRIVER_H__
#define __KLOG_DRIVER_H__

#ifndef __KLOG_COMMON_H__

#define KLOG_IOCTL_ID						0xD1
//command
#define KLOG_IOCTL_CHECK_OLD_LOG				_IO(KLOG_IOCTL_ID, 0x01)
#define KLOG_IOCTL_CLEAR_LOG					_IO(KLOG_IOCTL_ID, 0x02)
#define KLOG_IOCTL_RECORD_SYSINFO				_IO(KLOG_IOCTL_ID, 0x03)
#define KLOG_IOCTL_FORCE_PANIC					_IO(KLOG_IOCTL_ID, 0x04)
#define KLOG_IOCTL_FORCE_CRASH					_IO(KLOG_IOCTL_ID, 0x05)
#define KLOG_IOCTL_SET_PANIC_WHEN_SUSPEND			_IO(KLOG_IOCTL_ID, 0x06)
#define KLOG_IOCTL_SET_PANIC_WHEN_POWER_OFF			_IO(KLOG_IOCTL_ID, 0x07)
//get category
#define KLOG_IOCTL_GET_HEADER					_IO(KLOG_IOCTL_ID, 0x11)
#define KLOG_IOCTL_GET_CRASH					_IO(KLOG_IOCTL_ID, 0x12)
#define KLOG_IOCTL_GET_APPSBL					_IO(KLOG_IOCTL_ID, 0x13)
#define KLOG_IOCTL_GET_KERNEL					_IO(KLOG_IOCTL_ID, 0x14)
#define KLOG_IOCTL_GET_ANDROID_MAIN				_IO(KLOG_IOCTL_ID, 0x15)
#define KLOG_IOCTL_GET_ANDROID_SYSTEM				_IO(KLOG_IOCTL_ID, 0x16)
#define KLOG_IOCTL_GET_ANDROID_RADIO				_IO(KLOG_IOCTL_ID, 0x17)
#define KLOG_IOCTL_GET_ANDROID_EVENTS				_IO(KLOG_IOCTL_ID, 0x18)
#define KLOG_IOCTL_GET_RESERVE					_IO(KLOG_IOCTL_ID, 0x19)
//CCI engmode
#define KLOG_IOCTL_GET_BOOTINFO					_IO(KLOG_IOCTL_ID, 0x31)
#define KLOG_IOCTL_GET_DEVICEINFO				_IO(KLOG_IOCTL_ID, 0x32)
#define KLOG_IOCTL_GET_DEBUGINFO				_IO(KLOG_IOCTL_ID, 0x33)
#define KLOG_IOCTL_GET_PERFORMANCEINFO				_IO(KLOG_IOCTL_ID, 0x34)
#define KLOG_IOCTL_GET_WARRANTYINFO				_IO(KLOG_IOCTL_ID, 0x35)
#define KLOG_IOCTL_GET_FACTORYINFO				_IO(KLOG_IOCTL_ID, 0x36)

#define KLOG_LOG_TAG						"[klog]"
#define KLOG_DEV_ID						"cciklog"
#define KLOG_DEV_PATH						"/dev/"KLOG_DEV_ID
#define KLOG_CATEGORY_NAME_LENGTH				16

//klog info(135)
#define KLOG_MAGIC_TOTAL_LENGTH					10		//magic*6+state*3+'\0'
#define KLOG_MAGIC_LENGTH					6
#define KLOG_STATE_LENGTH					3
#define KLOG_KERNEL_TIME_LENGTH					20
#define KLOG_FIRST_RTC_TIMESTAMP_LENGTH				20
#define KLOG_LAST_RTC_TIMESTAMP_LENGTH				20
#define KLOG_NORMAL_BOOT_LENGTH					2
#define KLOG_SYSTEM_ON_OFF_STATE_LENGTH				4
#define KLOG_SBL_BOOTUP_TIME_LENGTH				4
#define KLOG_ABOOT_BOOTUP_TIME_LENGTH				4
#define KLOG_ANDROID_BOOTUP_TIME_LENGTH				4
#define KLOG_ANDROID_SHUTDOWN_TIME_LENGTH			4
#define KLOG_SYSFS_SYNC_TIME_LENGTH				4
#define KLOG_KERNEL_POWER_OFF_TIME_LENGTH			4
#define KLOG_SUSPEND_TIME_LENGTH				4
#define KLOG_RESUME_TIME_LENGTH					4
#define KLOG_USERDATA_COUNT_LENGTH				4
#define KLOG_INTERNAL_COUNT_LENGTH				4
#define KLOG_EXTERNAL_COUNT_LENGTH				4
#define KLOG_SIM_STATE_LENGTH					15
#define KLOG_INFO_LENGTH					(\
									KLOG_MAGIC_TOTAL_LENGTH +\
									KLOG_KERNEL_TIME_LENGTH +\
									KLOG_FIRST_RTC_TIMESTAMP_LENGTH +\
									KLOG_LAST_RTC_TIMESTAMP_LENGTH +\
									KLOG_NORMAL_BOOT_LENGTH +\
									KLOG_SYSTEM_ON_OFF_STATE_LENGTH +\
									KLOG_SBL_BOOTUP_TIME_LENGTH +\
									KLOG_ABOOT_BOOTUP_TIME_LENGTH +\
									KLOG_ANDROID_BOOTUP_TIME_LENGTH +\
									KLOG_ANDROID_SHUTDOWN_TIME_LENGTH +\
									KLOG_SYSFS_SYNC_TIME_LENGTH +\
									KLOG_KERNEL_POWER_OFF_TIME_LENGTH +\
									KLOG_SUSPEND_TIME_LENGTH +\
									KLOG_RESUME_TIME_LENGTH +\
									KLOG_USERDATA_COUNT_LENGTH +\
									KLOG_INTERNAL_COUNT_LENGTH +\
									KLOG_EXTERNAL_COUNT_LENGTH +\
									KLOG_SIM_STATE_LENGTH\
								)
//image info(303)
#define KLOG_ANDROID_VERSION_LENGTH				10
#define KLOG_MODEM_VERSION_LENGTH				40
#define KLOG_FLEX_VERSION_LENGTH				25
#define KLOG_RPM_VERSION_LENGTH					15
#define KLOG_BOOTLOADER_VERSION_LENGTH				35
#define KLOG_LINUX_VERSION_LENGTH				40
#define KLOG_VERSION_ID_LENGTH					25
#define KLOG_BUILD_VERSION_LENGTH				20
#define KLOG_BUILD_DATE_LENGTH					31
#define KLOG_BUILD_TYPE_LENGTH					10
#define KLOG_BUILD_USER_LENGTH					15
#define KLOG_BUILD_HOST_LENGTH					20
#define KLOG_BUILD_KEY_LENGTH					13
#define KLOG_SECURE_MODE_LENGTH					2
#define KLOG_DEBUG_MODE_LENGTH					2
#define KLOG_IMAGE_INFO_LENGTH					(\
									KLOG_ANDROID_VERSION_LENGTH +\
									KLOG_MODEM_VERSION_LENGTH +\
									KLOG_FLEX_VERSION_LENGTH +\
									KLOG_RPM_VERSION_LENGTH +\
									KLOG_BOOTLOADER_VERSION_LENGTH +\
									KLOG_LINUX_VERSION_LENGTH +\
									KLOG_VERSION_ID_LENGTH +\
									KLOG_BUILD_VERSION_LENGTH +\
									KLOG_BUILD_DATE_LENGTH +\
									KLOG_BUILD_TYPE_LENGTH +\
									KLOG_BUILD_USER_LENGTH +\
									KLOG_BUILD_HOST_LENGTH +\
									KLOG_BUILD_KEY_LENGTH +\
									KLOG_SECURE_MODE_LENGTH +\
									KLOG_DEBUG_MODE_LENGTH\
								)
//HW info(21)
#define KLOG_HW_ID_LENGTH					5
#define KLOG_CPUINFO_MAX_FREQ_LENGTH				8
#define KLOG_SCALING_MAX_FREQ_LENGTH				8
#define KLOG_HW_INFO						(\
									KLOG_HW_ID_LENGTH +\
									KLOG_CPUINFO_MAX_FREQ_LENGTH +\
									KLOG_SCALING_MAX_FREQ_LENGTH\
								)

enum klog_magic_index
{
	KLOG_INDEX_INVALID = -1,
	KLOG_INDEX_INIT = 0,
	KLOG_INDEX_MARM_FATAL,
	KLOG_INDEX_AARM_PANIC,
	KLOG_INDEX_RPM_CRASH,
	KLOG_INDEX_FIQ_HANG,
	KLOG_INDEX_DOWNLOAD_MODE,
	KLOG_INDEX_POWER_OFF,
	KLOG_INDEX_REBOOT,
	KLOG_INDEX_BOOTLOADER,
	KLOG_INDEX_RECOVERY,
	KLOG_INDEX_OEM_COMMAND,
	KLOG_INDEX_APPSBL,
	KLOG_INDEX_FORCE_CLEAR = 0xF,
};

//do not over than 6 characters
#define KLOG_MAGIC_NONE						""		//not to update magic
#define KLOG_MAGIC_INIT						"CKLOGC"	//default magic
#define KLOG_MAGIC_FORCE_CLEAR					"FRCCLR"	//force clear
#define KLOG_MAGIC_MARM_FATAL					"FATALM"	//mARM fatal error
#define KLOG_MAGIC_AARM_PANIC					"PANICA"	//aARM kernel panic
#define KLOG_MAGIC_RPM_CRASH					"RPMCRH"	//RPM crash
#define KLOG_MAGIC_FIQ_HANG					"FIQHNG"	//FIQ hang
#define KLOG_MAGIC_DOWNLOAD_MODE				"DLMODE"	//normal download mode
#define KLOG_MAGIC_POWER_OFF					"PWROFF"	//power off
#define KLOG_MAGIC_REBOOT					"REBOOT"	//normal reboot
#define KLOG_MAGIC_BOOTLOADER					"BOTLDR"	//boot into bootloader(fastboot mode)
#define KLOG_MAGIC_RECOVERY					"RCOVRY"	//boot into recovery mode
#define KLOG_MAGIC_OEM_COMMAND					"OEM-"		//boot into oem customized mode, 2-digit hex attached to the tail
#define KLOG_MAGIC_APPSBL					"APPSBL"	//AppSBL magic to indicate cold-boot

#define KLOG_PRIORITY_INVALID					-1		//any value which not defined in klog_magic_list
#define KLOG_PRIORITY_HIGHEST					0
#define KLOG_PRIORITY_LOWEST					255

#define KLOG_PRIORITY_FORCE_CLEAR				KLOG_PRIORITY_HIGHEST
#define KLOG_PRIORITY_MARM_FATAL				1
#define KLOG_PRIORITY_AARM_PANIC				1
#define KLOG_PRIORITY_RPM_CRASH					3
#define KLOG_PRIORITY_FIQ_HANG					4
#define KLOG_PRIORITY_DOWNLOAD_MODE				5
#define KLOG_PRIORITY_POWER_OFF					6
#define KLOG_PRIORITY_NATIVE_COMMAND				7
#define KLOG_PRIORITY_OEM_COMMAND				8
#define KLOG_PRIORITY_KLOG_INIT					KLOG_PRIORITY_LOWEST

//do not over than 3 digits(hex)
#define KLOG_STATE_INIT_CODE					"###"
#define KLOG_STATE_INIT						-1
#define KLOG_STATE_NONE						0x000
#define KLOG_STATE_MARM_FATAL					0x001
#define KLOG_STATE_AARM_PANIC					0x002
#define KLOG_STATE_DOWNLOAD_MODE				0x004

enum cklc_category
{
#if CCI_KLOG_CRASH_SIZE
	KLOG_CRASH,
#endif // #if CCI_KLOG_CRASH_SIZE
#if CCI_KLOG_APPSBL_SIZE
	KLOG_APPSBL,						//not collect by kernel driver
#endif // #if CCI_KLOG_APPSBL_SIZE
#if CCI_KLOG_KERNEL_SIZE
	KLOG_KERNEL,
#endif // #if CCI_KLOG_KERNEL_SIZE
#if CCI_KLOG_ANDROID_MAIN_SIZE
	KLOG_ANDROID_MAIN,
#endif // #if CCI_KLOG_ANDROID_MAIN_SIZE
#if CCI_KLOG_ANDROID_SYSTEM_SIZE
	KLOG_ANDROID_SYSTEM,
#endif // #if CCI_KLOG_ANDROID_SYSTEM_SIZE
#if CCI_KLOG_ANDROID_RADIO_SIZE
	KLOG_ANDROID_RADIO,
#endif // #if CCI_KLOG_ANDROID_RADIO_SIZE
#if CCI_KLOG_ANDROID_EVENTS_SIZE
	KLOG_ANDROID_EVENTS,
#endif // #if CCI_KLOG_ANDROID_EVENTS_SIZE
	KLOG_IGNORE						//should be the last item in the list
};

#define KLOG_CATEGORY_NAME_CRASH				"CRASH"
#define KLOG_CATEGORY_NAME_APPSBL				"APPSBL"
#define KLOG_CATEGORY_NAME_KERNEL				"KERNEL"
#define KLOG_CATEGORY_NAME_ANDROID_MAIN				"ANDROID_MAIN"
#define KLOG_CATEGORY_NAME_ANDROID_SYSTEM			"ANDROID_SYSTEM"
#define KLOG_CATEGORY_NAME_ANDROID_RADIO			"ANDROID_RADIO"
#define KLOG_CATEGORY_NAME_ANDROID_EVENTS			"ANDROID_EVENTS"
#define KLOG_CATEGORY_NAME_RESERVE				"RESERVE"
#define KLOG_CATEGORY_NAME_PROCESS				"PROCESS"

#define KLOG_DUMMY_BUFFER_SIZE					1

struct klog_category
{
	char		name[KLOG_CATEGORY_NAME_LENGTH];	//category name
	size_t		size;					//category size
	size_t		index;					//category read/write index of the ring buffer
	unsigned char	overload;				//category overload flag, 0:indicate the message ring buffer is not overrun yet, >0:indicate the message ring buffer is overran
	char		buffer[KLOG_DUMMY_BUFFER_SIZE];		//category message ring buffer, assign a dummy size here, it's real size will be assigned/checked in the filling algorithm
};

#define KLOG_CATEGORY_HEADER_SIZE				(sizeof(struct klog_category) - sizeof(char[KLOG_DUMMY_BUFFER_SIZE]))
#define KLOG_CATEGORY_TOTAL_SIZE				(\
									CCI_KLOG_CRASH_SIZE +\
									CCI_KLOG_APPSBL_SIZE +\
									CCI_KLOG_KERNEL_SIZE +\
									CCI_KLOG_ANDROID_MAIN_SIZE +\
									CCI_KLOG_ANDROID_SYSTEM_SIZE +\
									CCI_KLOG_ANDROID_RADIO_SIZE +\
									CCI_KLOG_ANDROID_EVENTS_SIZE\
								)
#if (CCI_KLOG_HEADER_SIZE + KLOG_CATEGORY_TOTAL_SIZE) > CCI_KLOG_SIZE
#error Invalid klog size definition
#endif // #if (CCI_KLOG_HEADER_SIZE + KLOG_CATEGORY_TOTAL_SIZE) > CCI_KLOG_SIZE

struct klog_magic_list
{
	int		index;
	unsigned char	name[KLOG_MAGIC_LENGTH + 1];
	int		priority;
};

struct system_information
{
	char		magic[KLOG_MAGIC_TOTAL_LENGTH];
	char		kernel_time[KLOG_KERNEL_TIME_LENGTH];
	char		first_rtc[KLOG_FIRST_RTC_TIMESTAMP_LENGTH];
	char		last_rtc[KLOG_LAST_RTC_TIMESTAMP_LENGTH];
	char		normal_boot[KLOG_NORMAL_BOOT_LENGTH];
	unsigned int	system_on_off_state;
	unsigned int	sbl_bootup_time;
	unsigned int	aboot_bootup_time;
	unsigned int	android_bootup_time;
	unsigned int	android_shutdown_time;
	unsigned int	sysfs_sync_time;
	unsigned int	kernel_power_off_time;
	unsigned int	suspend_time;
	unsigned int	resume_time;
	char		klog_userdata_count[KLOG_USERDATA_COUNT_LENGTH];
	char		klog_internal_count[KLOG_INTERNAL_COUNT_LENGTH];
	char		klog_external_count[KLOG_EXTERNAL_COUNT_LENGTH];
	char		sim_state[KLOG_SIM_STATE_LENGTH];
	char		android_version[KLOG_ANDROID_VERSION_LENGTH];
	char		modem_version[KLOG_MODEM_VERSION_LENGTH];
	char		flex_version[KLOG_FLEX_VERSION_LENGTH];
	char		rpm_version[KLOG_RPM_VERSION_LENGTH];
	char		bootloader_version[KLOG_BOOTLOADER_VERSION_LENGTH];
	char		linux_version[KLOG_LINUX_VERSION_LENGTH];
	char		version_id[KLOG_VERSION_ID_LENGTH];
	char		build_version[KLOG_BUILD_VERSION_LENGTH];
	char		build_date[KLOG_BUILD_DATE_LENGTH];
	char		build_type[KLOG_BUILD_TYPE_LENGTH];
	char		build_user[KLOG_BUILD_USER_LENGTH];
	char		build_host[KLOG_BUILD_HOST_LENGTH];
	char		build_key[KLOG_BUILD_KEY_LENGTH];
	char		secure_mode[KLOG_SECURE_MODE_LENGTH];
	char		debug_mode[KLOG_DEBUG_MODE_LENGTH];
	char		hw_id[KLOG_HW_ID_LENGTH];
	char		cpuinfo_max_freq[KLOG_CPUINFO_MAX_FREQ_LENGTH];
	char		scaling_max_freq[KLOG_SCALING_MAX_FREQ_LENGTH];
};

#ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE
//#define CCI_ENGMODE_LOG
//#define CCI_ENGMODE_WORKAROUND					//only for engmode temporary solution before iRAM is ready

#define NOT_SUPPORTED_VALUE					-1
#define NOT_SUPPORTED_STRING					"-"

#define POWER_REASON_RECENT_LENGTH				10
#define POWER_REASON_COUNTER_LENGTH				30

#define MAIN_CHIP_INFO_LENGTH					10
#define RADIO_CHIP_INFO_LENGTH					10

#define TOTAL_DEVICE_ON_TIME					32
#define FIRST_VOICE_CALL_TIMESTAMP				32
#define FIRST_DATA_CALL_TIMESTAMP				32

struct boot_information
{
//boot mode
	char		emergency_download_mode;
	char		qct_download_mode;
	char		cci_download_mode;
	char		mass_storage_download_mode;
	char		sd_download_mode;
	char		usb_ram_dump_mode;
	char		sd_ram_dump_mode;
	char		normal_boot_mode;
	char		sd_boot_mode;
	char		fastboot_mode;
	char		recovery_mode;
	char		simple_test_mode;
	char		charging_only_mode;
	char		android_safe_mode;
//secure boot mode
	char		qct_secure_boot_mode;
	char		cci_secure_boot_mode;
//power on reason
	unsigned int	power_on_reason_recent[POWER_REASON_RECENT_LENGTH];
	unsigned int	power_on_reason_counter[POWER_REASON_COUNTER_LENGTH];
//power off reason
	unsigned int	power_off_reason_recent[POWER_REASON_RECENT_LENGTH];
	unsigned int	power_off_reason_counter[POWER_REASON_COUNTER_LENGTH];
//crash
	unsigned int	crash_record[POWER_REASON_COUNTER_LENGTH];
};

struct device_information
{
//hw information
	char		hw_id;
	char		band_id;
	char		model_id;
	char		nfc_id;
	char		main_chip[MAIN_CHIP_INFO_LENGTH];
	char		radio_chip[RADIO_CHIP_INFO_LENGTH];
	unsigned int	cpuinfo_max_freq;
	unsigned int	scaling_max_freq;
	unsigned int	ram;
	unsigned int	flash;
	unsigned int	emmc;
	char		hw_reset;
//sw information
	char		android_version[KLOG_ANDROID_VERSION_LENGTH];
	char		modem_version[KLOG_MODEM_VERSION_LENGTH];
	char		flex_version[KLOG_FLEX_VERSION_LENGTH];
	char		rpm_version[KLOG_RPM_VERSION_LENGTH];
	char		bootloader_version[KLOG_BOOTLOADER_VERSION_LENGTH];
	char		linux_version[KLOG_LINUX_VERSION_LENGTH];
	char		version_id[KLOG_VERSION_ID_LENGTH];
	char		build_version[KLOG_BUILD_VERSION_LENGTH];
	char		build_date[KLOG_BUILD_DATE_LENGTH];
	char		build_type[KLOG_BUILD_TYPE_LENGTH];
	char		build_user[KLOG_BUILD_USER_LENGTH];
	char		build_host[KLOG_BUILD_HOST_LENGTH];
	char		build_key[KLOG_BUILD_KEY_LENGTH];
	char		secure_mode;
};

struct debug_information
{
//debug mode
	char		debug_mode;
//debug interface
	char		usb_sbl_diag;
	char		usb_diag;
	char		usb_adb;
	char		uart;
	char		jtag;
//ram dump trigger method
	char		hot_key_ram_dump;
	char		command_ram_dump;
	char		crash_ram_dump;
//logger
	char		bootloader_log;
	char		kernel_log;
	char		logcat_log;
	char		klog_log;
	char		rpm_log;
};

struct performance_information
{
//bootup/shutdown/suspend/resume
	unsigned int	bootup_time;
	unsigned int	shutdown_time;
	unsigned int	suspend_time;
	unsigned int	resume_time;
	unsigned int	up_time;
	unsigned int	idle_time;
	unsigned int	sleep_time;
};

struct warranty_information
{
//warranty record
	char		total_deviceon_time[TOTAL_DEVICE_ON_TIME];
	char		first_voice_call_timestamp[FIRST_VOICE_CALL_TIMESTAMP];
	char		first_data_call_timestamp[FIRST_DATA_CALL_TIMESTAMP];
	char		maintenance_record;
};

struct factory_information
{
//FTM
	char		backup_command;
	char		restore_command;
	char		backup_record;
};
#endif // #ifdef CCI_KLOG_SUPPORT_CCI_ENGMODE

#endif // #ifndef __KLOG_COMMON_H__

#ifdef CONFIG_CCI_KLOG
//#define CCI_KLOG_DETAIL_LOG
#define CCI_HW_ID
#ifdef IMEM_CERT_RECORD
#define CCI_KLOG_SBL_BOOT_TIME_USE_IMEM
#endif // #ifdef IMEM_CERT_RECORD
#define CCI_KLOG_MODEM_CRASH_LOG_USE_SMEM
#define CCI_KLOG_SUPPORT_ATTRIBUTE
#include "../../drivers/staging/android/logger.h"

#define kprintk(fmt, args...)					printk(KERN_CRIT KLOG_LOG_TAG fmt, ##args)

struct klog_time
{
	struct timespec	clock;
	struct timespec	rtc;
};

void cklc_append_kernel_raw_char(unsigned char c);
void cklc_append_str(unsigned int category, unsigned char *str, size_t len);
void cklc_append_newline(unsigned int category);
void cklc_append_separator(unsigned int category);
void cklc_append_time_header(unsigned int category);
void show_android_log_to_console(void);
void cklc_append_android_log(unsigned int category, const struct logger_entry *header, const unsigned char *log_priority, const char * const log_tag, const int log_tag_bytes, const char * const log_msg, const int log_msg_bytes);
void cklc_save_magic(char *magic, int state);
void cklc_set_memory_ready(void);
int match_crash_priority(int priority);
void update_priority(void);
#ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
void klog_record_rpm_version(const char *str);
#endif // #ifdef CONFIG_CCI_KLOG_RECORD_RPM_VERSION
int get_fault_state(void);
void set_fault_state(int level, int type, const char* msg);
void set_kernel_log_level(int level);
void record_shutdown_time(int state);
struct timespec klog_get_kernel_clock_timestamp(void);
#ifdef CCI_KLOG_ALLOW_FORCE_PANIC
int get_force_panic_when_suspend(void);
int get_force_panic_when_power_off(void);
#endif // #ifdef CCI_KLOG_ALLOW_FORCE_PANIC

#else // #ifdef CONFIG_CCI_KLOG

#define cklc_append_kernel_raw_char(c)				do {} while (0)
#define cklc_append_str(category, str, len)			do {} while (0)
#define cklc_append_newline(category)				do {} while (0)
#define cklc_append_separator(category)				do {} while (0)
#define cklc_append_time_header(category)			do {} while (0)
#define cklc_save_magic						do {} while (0)
#define cklc_set_memory_ready					do {} while (0)
#define klog_record_rpm_version					do {} while (0)

#endif // #ifdef CONFIG_CCI_KLOG

#endif // #ifndef __KLOG_DRIVER_H__

