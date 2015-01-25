/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
///#include <linux/smp_lock.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <linux/notifier.h>
#include <linux/wait.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/err.h>

#include <linux/cdev.h>
//#include "cci_kernel_info.h"
#include <linux/uaccess.h>
#include <linux/msm_adc.h>

//for RSV RW
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/msm-charger.h>
#include <linux/rw_info.h>

//S:LE
#include <mach/msm_iomap.h>
#include <linux/io.h>
#define IMEM_BASE 0x2A03F000
#define TZ_CONTROL 0x940
#define UNUSED	0xAAAAAAAA
#define TZ_ENABLE 0xF55FF55F
//E:LE

#include <linux/gpio.h> //for key detection

enum fs_cookie_status
{
  FS_COOKIE_STATUS_UNSET,
  FS_COOKIE_STATUS_SET
};

enum fs_cookie_id
{
  FS_COOKIE_ID_RECOVERY,
  FS_COOKIE_ID_BACKUP,
  FS_COOKIE_ID_MAX
};

#define FIND_RSV_SECTOR(rsv_sector)	(0x7fff - rsv_sector)

#define RSV_COOKIE_START    0x53565352   /* RSVS */
#define RSV_COOKIE_END        0x45565352   /* RSVE */

#define FS_COOKIE_ID_FORCE_BACKUP_FLAG 0x46 /* 'F' */
#define MAX_RSV_READ_SECTOR 0x20
#define UUID_FOR_ENABLE_DL_MODE		"39313336373631612D643837372D343061622D626338652D323464306163333166376362"
#define ENABLE_DLOAD_MODE_FLAG		0x55555555
enum fs_back_reco_status
{
  BACKUP_RECOVER_SUCCESS,
  BACKUP_RECOVER_FAIL1,
  BACKUP_RECOVER_FAIL2,
  BACKUP_RECOVER_FAIL3,
  BACKUP_RECOVER_FAIL4,
};

enum fs_process
{
  FS_PROCESS_START,
  FS_PROCESS_END,
};
enum fs_action
{
  FS_READ_FLAG,
  FS_WRITE_FLAG
};

struct rsv_flag
{
  unsigned int start_magic;               /* STCK */
  unsigned int index;
  unsigned int cookie_id;                 /* unique ID for the cookie */
  unsigned int process;
  unsigned int status;
  unsigned int end_magic;              /* in bytes */
};

struct rsv_read_permission
{
  unsigned char allow[MAX_RSV_READ_SECTOR]; // allow[0] represent rsv sector 0's permission, 'y' represents allow to be read
                                                                            // allow[0x1f] represent rsv sector 0x1f's permission, 'y' represents allow to be read
};

struct cci_dload_flag_type
{
  unsigned int magic_num;
  unsigned int data;
  unsigned int cci_magic_1;
  unsigned int cci_magic_2;
  char buf[128];
};

/*---------------------  Static Definitions -------------------------*/
#define CCI_KERNEL_INFO_DEBUG 0   //0:disable, 1:enable
#if (CCI_KERNEL_INFO_DEBUG)
    #define Printker(args...)    printk(args)
#else
    #define Printker(args...)
#endif

#define GPIO_FTM_PIN_NUM 19
#define CMD_ALLOW 0

/*---------------------  Static Classes  ----------------------------*/

#define CCI_KERNEL_INFO_SIZE 4096
#define CCI_KERNEL_INFO_MAJOR 88;
#define CCI_KERNEL_INFO_DRIVER_NAME "cci_kernel_info"

static int cci_kernel_info_major = CCI_KERNEL_INFO_MAJOR;
static struct class *cci_kernel_info_class;

//--------global variables--------//
//int key_volup_pressed = 0;
//int key_voldown_pressed = 0;
int charger_is_detected = 0;
int cciaboot_flag = 0;
int if_board_evt = 0;
unsigned long startup_magic_flag = 0;
unsigned long warmboot_magic_flag = 0;
EXPORT_SYMBOL(charger_is_detected);
EXPORT_SYMBOL(cciaboot_flag);
EXPORT_SYMBOL(if_board_evt);
EXPORT_SYMBOL(startup_magic_flag);
EXPORT_SYMBOL(warmboot_magic_flag);


extern int key_capture_pressed;
extern int ftm_pin_is_low;

//--------global variables--------//

#if 0 //LE
extern int rw_info_start_charging(int chg_current);
extern int rw_info_stop_charging(void);
extern bool msm_charger_timeout_disable(bool disable);
extern int system_loading_update(enum module_loading_op action, int id, int loading);

EXPORT_SYMBOL(system_loading_update);
#endif

#if CMD_ALLOW  
static int kernel_read_adc(int channel, int *mv_reading)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	Printker("%s: called for %d\n", __func__, channel);
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	wait_for_completion(&conv_complete_evt);
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret) {
		pr_err("%s: couldnt close channel %d ret=%d\n",
						__func__, channel, ret);
	}
	if (mv_reading)
		*mv_reading = adc_chan_result.measurement;

	Printker("%s: done for %d\n", __func__, channel);
	return 0;// adc_chan_result.physical;
out:
	Printker("%s: done for %d\n", __func__, channel);
	return -EINVAL;

}

int init_permission_table(struct rsv_read_permission *per_ptr)
{
  int i, j;
  unsigned int not_allow_table[] = { 3, 4, 5, 6, 7, 8, 9, 0x0a, 0xb, 0xc, 0xd};
  int size_of_table = sizeof(not_allow_table) / sizeof(unsigned int);
  
  for(i = 0; i < MAX_RSV_READ_SECTOR; i++)
  {
    for(j = 0; j < size_of_table; j++)
    {
      if(not_allow_table[j] == i)
      {
        per_ptr->allow[i] = 'n';
        break;
      }
    }
    if(j == size_of_table)
      per_ptr->allow[i] = 'y';
  }

  return 1;
}

int cci_rw_info_read_sector(unsigned char* buf_sector, unsigned char* block_string, int sector_no)
{
      //----------Usage example---------------
      //unsigned char buf[0x200];
      //cci_rw_info_read_sector(buf, "/dev/block/mmcblk0p5", 0);  <--read 0th sector of mmcblk0p5 partition
      //-------------------------------------

      struct file* filep;
      mm_segment_t old_fs;     
      unsigned int sector_size = 0x200;
      int result = 0;

      filep=filp_open(block_string, O_RDONLY, 0644);
      if(IS_ERR(filep))
      {
        printk(KERN_ERR "Kernel: open partition fail\n");
	return -1;
      }
      	
      old_fs = get_fs();
      set_fs(KERNEL_DS);
      filep->f_pos = sector_size * sector_no;
  
      memset((void*) buf_sector, 0x0, 0x200);

      result = filep->f_op->read(filep, (void*) buf_sector, sector_size,  &filep->f_pos);

      set_fs(old_fs);
      filp_close(filep,0);		
	  
      if(result < 0)
      //if(1)
        return -1;
      else
	return 1;
}

static int cci_rw_info_write_file(unsigned char* buf_sector, unsigned char* block_string)
{
      //----------Usage example---------------
      //unsigned char buf[0x200];
      //cci_rw_info_read_sector(buf, "/dev/block/mmcblk0p5", 0);  <--read 0th sector of mmcblk0p5 partition
      //-------------------------------------

      struct file* filep;
      mm_segment_t old_fs;     
      unsigned int sector_size = 0x200;
      int result = 0;

      filep=filp_open(block_string, O_RDWR | O_CREAT, 0644);
      if(IS_ERR(filep))
      {
        printk(KERN_ERR "Kernel: open partition fail\n");
	return -1;
      }
      	
      old_fs = get_fs();
      set_fs(KERNEL_DS);
	  
      result = filep->f_op->write(filep, (void*) buf_sector, sector_size,  &filep->f_pos);

      set_fs(old_fs);
      filp_close(filep,0);		
	  
      if(result < 0)
        return -1;
      else
	return 1;
}

static int cci_rw_info_write_binary(unsigned char* buf_sector, unsigned char* block_string, int sector_no)
{
      //----------Usage example---------------
      //unsigned char buf[0x200];
      //cci_rw_info_read_sector(buf, "/dev/block/mmcblk0p5", 0);  <--read 0th sector of mmcblk0p5 partition
      //-------------------------------------

      struct file* filep;
      mm_segment_t old_fs;     
      unsigned int sector_size = 0x200;
      int result = 0;

      if( (strncmp((void*) block_string, "/dev/block/mmcblk0", sizeof("/dev/block/mmcblk0") - 1) != 0) &&
	  (sector_no >= 0x7fff) && (sector_no <= (0x7fff - 0x1800)) )
        return -1;

      filep=filp_open(block_string, O_RDWR, 0644);
      if(IS_ERR(filep))
      {
        printk(KERN_ERR "Kernel: open partition fail\n");
	return -2;
      }
      	
      old_fs = get_fs();
      set_fs(KERNEL_DS);
      filep->f_pos = sector_size * sector_no;

      result = filep->f_op->write(filep, (void*) buf_sector, sector_size,  &filep->f_pos);

      set_fs(old_fs);
      filp_close(filep,0);		
	  
      if(result < 0)
        return result;
      else
	return 1;
}
#endif

struct cci_kernel_info_dev
{
  struct cdev cdev;
  unsigned char vaule[CCI_KERNEL_INFO_SIZE];
};

struct cci_kernel_info_dev dev;

int cci_kernel_info_open(struct inode *inode, struct file *filp)
{
  Printker("cci_kernel_info: calling KERNEL_INFO_OPEN !!!!\n");
  
  return 0;
}

#define CMD_GRP_SHT	4
#define CMD_GRP_MASK	0xF0
#define SUB_CMD_MASK	0x0F

#define DBG_INF_GRP 0x00
#define ADC_CMD_GRP 0x10
#define CHG_CMD_GRP 0x20
#define AUD_CMD_GRP 0x30
#define DBG_CMD_GRP 0x40
#define FAC_CMD_GRP 0x50
#define TST_CMD_GRP 0x60


unsigned long cci_debug_list(unsigned long row, char *debug_str, unsigned long str_len);
unsigned long cci_general_debug_info(unsigned long row, char *debug_str, unsigned long str_len);
unsigned long cci_chgarging_debug_info(unsigned long row, char *debug_str, unsigned long str_len);
unsigned long cci_force_crash_hidden_cmd(unsigned long row, char *debug_str, unsigned long str_len);


typedef enum
{
	CCI_DEBUG_LIST = 0,
	CCI_RESERVE = 1,
	CCI_GENERAL_DEBUG = 2,
	CCI_CHARGING_DEBUG = 3,
	CCI_DEBUG_MAX,
	CCI_DEBUG_HIDDEN = 2000,
	CCI_FORCE_CRASH_HIDDEN = 2001,
	CCI_DEBUG_HIDDEN_MAX,	
}cci_debug_idx_e_type;

typedef unsigned long (*cci_debug_f_type) (unsigned long row, char *debug_str, unsigned long str_len);

struct cci_debug_table_s_type
{
	cci_debug_idx_e_type debug_idx;
	cci_debug_f_type cci_debug_fp;
	char debug_info_title[MAX_DEBUG_INFO_TITLE_LENGTH];
};

const struct cci_debug_table_s_type cci_debug_info_table[] =
{
	{CCI_DEBUG_LIST, (cci_debug_f_type) cci_debug_list, "CCI_DEBUG_LIST\t"},
	{CCI_RESERVE, (cci_debug_f_type) NULL, "Index\tDebug Info Type\t"},
	{CCI_GENERAL_DEBUG, (cci_debug_f_type) cci_general_debug_info, "2\tGeneral Debug Info\t"},	
	{CCI_CHARGING_DEBUG, (cci_debug_f_type) cci_chgarging_debug_info, "3\tCharging Debug Info\t"},
};
static int cci_debug_info_table_len = (sizeof(cci_debug_info_table) / sizeof(struct cci_debug_table_s_type));

const struct cci_debug_table_s_type cci_debug_hidden_table[] =
{
	{CCI_DEBUG_HIDDEN, (cci_debug_f_type) NULL, "CCI_DEBUG_HIDDEN\t"},
	{CCI_FORCE_CRASH_HIDDEN, (cci_debug_f_type) cci_force_crash_hidden_cmd, "2\tForce Crash Hidden\t"},	
};
static int cci_debug_hidden_table_len = (sizeof(cci_debug_hidden_table) / sizeof(struct cci_debug_table_s_type));

//---------------Debug related function declared here: START  --------------//

unsigned long cci_debug_list(unsigned long row, char *debug_str, unsigned long str_len)
{
	memset(debug_str, 0x0, str_len);

	if((str_len < 300) || (row > 102))
		return -1;

	if(row < (sizeof(cci_debug_info_table) / sizeof(struct cci_debug_table_s_type)))
	{
		if(cci_debug_info_table[row].debug_info_title)
		{
			sprintf(debug_str, "%s", cci_debug_info_table[row].debug_info_title);
			debug_str[MAX_DEBUG_INFO_TITLE_LENGTH - 1] = '\0';
		}
	}

	return 0;
}

unsigned long cci_general_debug_info(unsigned long row, char *debug_str, unsigned long str_len)
{
	memset(debug_str, 0x0, str_len);

	if((str_len < 300) || (row > 102))
		return -1;

	switch(row)
	{
		case 0:
			sprintf(debug_str, "General Debug Info\t");
			break;

		case 1:
			//--- add your debug info here ---//
			sprintf(debug_str, "Test1\tTest2\tTest3");
			break;

		default:
			break;
	}

	return 0;
}

unsigned long cci_chgarging_debug_info(unsigned long row, char *debug_str, unsigned long str_len)
{
	memset(debug_str, 0x0, str_len);

	if((str_len < 300) || (row > 102))
		return -1;

	switch(row)
	{
		case 0:
			sprintf(debug_str, "Charging debug info\t");
			break;

		case 1:
			//--- add your debug info here ---//
			break;

		default:
			break;
	}

	return 0;
}

unsigned long cci_force_crash_hidden_cmd(unsigned long row, char *debug_str, unsigned long str_len)
{
	volatile unsigned long *panic_ptr = 0;

	memset(debug_str, 0x0, str_len);

	if((str_len < 300) || (row > 102))
		return -1;

	switch(row)
	{
		case 0:
			printk("%s, force crash!!\n", __func__);
			*panic_ptr = 0;
			while(1);

			break;

		default:
			break;
	}

	return 0;
}


//---------------Debug related function declared here: END  --------------//

static long cci_kernel_info_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  char buf[50];
#if CMD_ALLOW    
  int iRet;
  int iResult;
#endif
    int i;
  int ioctl_result = 0;
  struct cci_debug_struct *cci_debug_info_req = NULL;
  
  //int count; for debug purpose only
  
//S:LE
  void *imem = ioremap_nocache(IMEM_BASE, SZ_4K);
//E:LE
  Printker("cci_kernel_info: control case...%d \n",cmd);

  memset(buf, 0, sizeof(buf));
	  
  switch(DEBUG_INFO_IOCTL_ORIG(cmd & CMD_GRP_MASK))
  {
    case (DEBUG_INFO_IOCTL_NORMAL):
	cci_debug_info_req = kmalloc(sizeof(struct cci_debug_struct), GFP_KERNEL);

	if(cci_debug_info_req == NULL)
	{
		ioctl_result = -ENOMEM;
		break;
	}
	
    	if(copy_from_user(cci_debug_info_req, (void __user *) arg, sizeof(struct cci_debug_struct)))
    	{
    		ioctl_result = -EFAULT;
    		break;
    	}	
    
    	if(cci_debug_info_req->debug_idx < CCI_DEBUG_MAX)
    	{
    		for(i = 0; i < cci_debug_info_table_len; i++)
    		{
    			if(cci_debug_info_table[i].debug_idx == cci_debug_info_req->debug_idx)
    			{
    				if(cci_debug_info_table[i].cci_debug_fp != NULL)
    				{
    					ioctl_result = cci_debug_info_table[i].cci_debug_fp(cci_debug_info_req->row, cci_debug_info_req->debug_str, cci_debug_info_req->str_len);
    				}
    				else
    					ioctl_result = 0;
    
    				break;
    			}
    		}	
    		if(i == cci_debug_info_table_len)
    		{
    			ioctl_result = DBGINFO_NO_SUCH_CMD;
    			break;
    		}
    	}
    	else if((cci_debug_info_req->debug_idx >= CCI_DEBUG_HIDDEN) && (cci_debug_info_req->debug_idx < CCI_DEBUG_HIDDEN_MAX))
  {
    		for(i = 0; i < cci_debug_hidden_table_len; i++)
    		{
    			if(cci_debug_hidden_table[i].debug_idx == cci_debug_info_req->debug_idx)
    			{
    				if(cci_debug_hidden_table[i].cci_debug_fp != NULL)
    				{
    					ioctl_result = cci_debug_hidden_table[i].cci_debug_fp(cci_debug_info_req->row, cci_debug_info_req->debug_str, cci_debug_info_req->str_len);
    				}
    				else
    					ioctl_result = 0;
    
    				break;
    			}
    		}
    
    		if(i == cci_debug_info_table_len)
    		{
    			ioctl_result = DBGINFO_NO_SUCH_CMD;
    			break;
    		}
    	}
    	else
    	{
    		ioctl_result = DBGINFO_CMD_OUTOFRANGE;
    	}
    
    	cci_debug_info_req->debug_str[MAX_DEBUG_STR_LENGTH - 1] = '\0';
    
    	if (copy_to_user((void*)arg, cci_debug_info_req, sizeof(struct cci_debug_struct)))
    		ioctl_result = -EFAULT;
    	
    	break;	
  
    case (DEBUG_INFO_IOCTL_ORIG(ADC_CMD_GRP )): //vtherm = 0x11, vbat = 0x13
    {
#if CMD_ALLOW    		
      int iChannel =  cmd & SUB_CMD_MASK;
      
      iResult=kernel_read_adc(iChannel,&iRet);
      if (iResult)
        sprintf(buf, "%d <= ADC channel %d ERR %d\n", 0, iChannel, iResult);
      else
        sprintf(buf, "%d <= ADC channel %d\n", iRet, iChannel);

      #if (CCI_KERNEL_INFO_DEBUG)
      printk(buf);
      #endif
      
      if (copy_to_user((void*)arg, buf, sizeof(buf)))
        return -EFAULT;
      if (iResult)
        return -EIO;   
#endif	  
    }
    break;
		
    case (DEBUG_INFO_IOCTL_ORIG(CHG_CMD_GRP)): //0x2x
    {
#if 0 //M:LE
      bool chg_timeout_result;
      int chg_current, cRet = 0;
#else
      int cRet = 0;
#endif
	  
      int cCmd= cmd & SUB_CMD_MASK;

      switch(cCmd)
      {
#if 0 //M:LE
        case 0x0:  //disable charging, cci_rw_info c 0x20
          cRet = rw_info_stop_charging();
	  if(cRet != 0)
	  {
	    //error occurs
	    sprintf(buf, "Disable charging ERR %d\n", cRet);
	  }
	  else
	    sprintf(buf, "Disable chargine success!!\n");

          break;  
	  
	case 0x1: //enable charging, cci_rw_info c 0x21
	
	  // for flexible, we choose 500mA charging current at present
	  // It may be modified once we can distinguish the type of charger (USB/AC).
	  chg_current = 500;

	  cRet = rw_info_start_charging(chg_current);
	  if(cRet != 0)
	  {
	    //error occurs
	    sprintf(buf, "Enable charging ERR %d with chg current %d\n", cRet, chg_current);
	  }
	  else
	    sprintf(buf, "Enable charging with current %d success!!\n", chg_current);

	  break;

	case 0x2:  //enable charging timeout, cci_rw_info c 0x22
          chg_timeout_result = msm_charger_timeout_disable(false);
	  if(chg_timeout_result != true)
	  {
	    //error occurs
	    sprintf(buf, "Enable charging timeout ERR!!\n");
	  }
	  else
	    sprintf(buf, "Enable charging timeout success!!\n");

          break;  

	case 0x3:  //disable charging timeout, cci_rw_info c 0x23
          chg_timeout_result = msm_charger_timeout_disable(true);
	  if(chg_timeout_result != true)
	  {
	    //error occurs
	    sprintf(buf, "Disable charging timeout ERR!!\n");
	  }
	  else
	    sprintf(buf, "Disable charging timeout success!!\n");

          break;  
#endif

	  default:
	    sprintf(buf, "Nothing to do ???\n");
	    break;
      }

      if (copy_to_user((void*)arg, buf, sizeof(buf)))
        return -EFAULT;
      if (cRet)
        return -EIO;   
    }
		
    break;

    case (DEBUG_INFO_IOCTL_ORIG(AUD_CMD_GRP)): //0x3x
    break;

    case (DEBUG_INFO_IOCTL_ORIG(DBG_CMD_GRP)): //0x4x
    {
#if CMD_ALLOW 		
      struct cci_dload_flag_type dload_flag;    
      int sector_no = 0xd;	  
      int sector_size = 0x200;
      unsigned char *buf_sector_rsvsec = (unsigned char *) kmalloc(sector_size, GFP_KERNEL);	  
#endif	  
		
      int dChannel = cmd & SUB_CMD_MASK;
      
      switch(dChannel)
      {
#if CMD_ALLOW      
        case 0x0:  //cci_rw_info c 0x40, 0x4'0' means disable download mode
        {
          memset((void*) &dload_flag, 0x0, sizeof(dload_flag));

          dload_flag.magic_num = 0;
          dload_flag.data = 0x0;

          memcpy((void*) buf_sector_rsvsec, (void*) &dload_flag, sizeof(dload_flag));
          if(cci_rw_info_write_binary(buf_sector_rsvsec, "/dev/block/mmcblk0", FIND_RSV_SECTOR(sector_no)) == 1)
          {
    	    sprintf(buf, "1 <= Disable DL mode done!\n");	
          }
          else
    	    sprintf(buf, "0 <= Disable DL mode failed!!!!!\n");	
		
		//S:LE
		  writel(UNUSED, imem + TZ_CONTROL);
		//E:LE
          break;          
        }
        break;

        case 0x1:  //cci_rw_info c 0x41, 0x4'1' means enable download mode 
        {
          memset((void*) &dload_flag, 0x0, sizeof(dload_flag));
          memcpy((void*)&dload_flag.buf, UUID_FOR_ENABLE_DL_MODE,sizeof(UUID_FOR_ENABLE_DL_MODE));
          dload_flag.data = ENABLE_DLOAD_MODE_FLAG;

          memcpy((void*) buf_sector_rsvsec, (void*) &dload_flag, sizeof(dload_flag));
          if(cci_rw_info_write_binary(buf_sector_rsvsec, "/dev/block/mmcblk0", FIND_RSV_SECTOR(sector_no)) == 1)
          {
    	    sprintf(buf, "1 <= Enable DL mode done!\n");	
          }
          else
    	    sprintf(buf, "0 <= Enable DL mode failed!!!!!\n");	
		
		//S:LE
		  writel(TZ_ENABLE, imem + TZ_CONTROL);
		//E:LE
          break;          
        }
        break;		
#endif
        
        case 0x2:  //for test panic/reset behavior // 0x42
        {
          unsigned long *panic_ptr = 0;
          *panic_ptr = 0;
        }
        break;
		
        case 0x3:
        {
          unsigned int tempvalue = 0;
          tempvalue = __raw_readl(imem+0xB0C);
          sprintf(buf, "%d <= RebootType!\n", tempvalue);
        }
        break;
	
        default:
	  sprintf(buf, "Invalid cmd\n");
        break;
      }
#if CMD_ALLOW 	  
      kfree(buf_sector_rsvsec);
#endif

      if (copy_to_user((void*)arg, buf, sizeof(buf)))
        return -EFAULT;
	  
    }
    break;

    case (DEBUG_INFO_IOCTL_ORIG(FAC_CMD_GRP)):
    {
#if CMD_ALLOW  		
      //struct file* filep;
      //mm_segment_t old_fs;     
      struct rsv_flag rflag, rflag_force;    
      int sector_no;	  
      int cmd_backup = -1;
      int sector_size = 0x200;
      struct rsv_read_permission rsv_permission;
      unsigned char output_file[30];
	  
      unsigned char static_super_blk_data[16] = { 0x9a, 0x47, 0xf4, 0x9f, 0x5d, 0x2d, 0x03, 0xa3, 0x58, 0x75, 0xa8, 0xe5, 0x9f, 0xe7, 0x79, 0x0a}; 	
#endif	  
    	
      int fCmd =  cmd & SUB_CMD_MASK;

#if CMD_ALLOW  	  
      unsigned char *buf_sector_rsvsec0 = (unsigned char *) kmalloc(sector_size, GFP_KERNEL);
      unsigned char *buf_sector_rsvsec2 = (unsigned char *) kmalloc(sector_size, GFP_KERNEL);
      unsigned char *buf_sector_fsg = (unsigned char *) kmalloc(sector_size, GFP_KERNEL);  
      unsigned char *buf_sector_rsv = (unsigned char *) kmalloc(sector_size, GFP_KERNEL);  	  
#endif	  

      switch(fCmd)
      {
#if CMD_ALLOW        
        case 0x0: //cci_rw_info c 0x50

          sector_no = 0; 
          if(cci_rw_info_read_sector(buf_sector_rsvsec0, "/dev/block/mmcblk0", FIND_RSV_SECTOR(sector_no)) < 0)
          {
            printk(KERN_ERR "cci_rw_info: get %08x th sector of %s fail\n", FIND_RSV_SECTOR(sector_no), "/dev/block/mmcblk0");
            cmd_backup = 3;  
            sprintf(buf, "%d <= backup result!! (Read sector fail)\n", cmd_backup);			
            break;
          }

          sector_no = 2;		  
          if(cci_rw_info_read_sector(buf_sector_rsvsec2, "/dev/block/mmcblk0", FIND_RSV_SECTOR(sector_no)) < 0)
          {
            printk(KERN_ERR "cci_rw_info: get %08x th sector of %s fail\n", FIND_RSV_SECTOR(sector_no), "/dev/block/mmcblk0");
            cmd_backup = 3;  
            sprintf(buf, "%d <= backup result!! (Read sector fail)\n", cmd_backup);		
            break;			
          }		  

          sector_no = 0;
          if(cci_rw_info_read_sector(buf_sector_fsg, "/dev/block/platform/msm_sdcc.1/by-name/fsg", sector_no) < 0)
          {
            printk(KERN_ERR "cci_rw_info: get %08x th sector of %s fail\n", sector_no, "/dev/block/mmcblk0p21");
            cmd_backup = 3;  
            sprintf(buf, "%d <= backup result!! (Read sector fail)\n", cmd_backup);		
            break;
          }

          rflag.start_magic = RSV_COOKIE_START;
          rflag.index = 0;
          rflag.cookie_id = FS_COOKIE_ID_BACKUP;
          rflag.process = FS_PROCESS_END;
          rflag.status = BACKUP_RECOVER_SUCCESS;
          rflag.end_magic = RSV_COOKIE_END;	

          rflag_force.start_magic = RSV_COOKIE_START;
          rflag_force.index = 0;
          rflag_force.cookie_id = FS_COOKIE_ID_FORCE_BACKUP_FLAG;
          rflag_force.process = FS_PROCESS_END;
          rflag_force.status = BACKUP_RECOVER_SUCCESS;
          rflag_force.end_magic = RSV_COOKIE_END;	

          if((memcmp((void*) static_super_blk_data, (void*) buf_sector_fsg, sizeof(static_super_blk_data)) == 0) &&\
              ((memcmp((void*) &rflag, (void*) buf_sector_rsvsec0, sizeof(rflag)) == 0) || (memcmp((void*) &rflag_force, (void*) buf_sector_rsvsec2, sizeof(rflag_force)) == 0)))
          {
            cmd_backup = 1;
            sprintf(buf, "%d <= backup result!! (Backup success)\n", cmd_backup);			
          }
          else if((memcmp((void*) static_super_blk_data, (void*) buf_sector_fsg, sizeof(static_super_blk_data)) != 0) &&\
              ((memcmp((void*) &rflag, (void*) buf_sector_rsvsec0, sizeof(rflag)) == 0) || (memcmp((void*) &rflag_force, (void*) buf_sector_rsvsec2, sizeof(rflag_force)) == 0)))
          {
            cmd_backup = 4;
            sprintf(buf, "%d <= backup result!! (Static sb data fail)\n", cmd_backup);			
          }
          else if((memcmp((void*) &rflag, (void*) buf_sector_rsvsec0, sizeof(rflag.start_magic)) == 0) ||\
                     (memcmp((void*) &rflag_force, (void*) buf_sector_rsvsec2, sizeof(rflag_force.start_magic)) == 0))
          {
            cmd_backup = 0;
            sprintf(buf, "%d <= backup result!! (Backup fail)\n", cmd_backup);	                
          }
          else
          {
            cmd_backup = 2;
            sprintf(buf, "%d <= backup result!! (Backup isn't called yet)\n", cmd_backup);				              
          }
     	  
  	  break;

        case 0xA:  //cci_rw_info c 0x5A
            init_permission_table(&rsv_permission);

            for(sector_no = 0; sector_no < MAX_RSV_READ_SECTOR; sector_no++)
            {
              Printker("cci_rw_info: rsv_permission.allow[%d] = %c\n", sector_no, rsv_permission.allow[sector_no]);
              if(rsv_permission.allow[sector_no] == 'y')
              {
                cci_rw_info_read_sector(buf_sector_rsv, "/dev/block/mmcblk0", FIND_RSV_SECTOR(sector_no));
	        sprintf(output_file, "/mnt/sdcard/rsv_%02x.txt", sector_no);
                cci_rw_info_write_file(buf_sector_rsv, output_file);				
              }
            }

	    sprintf(buf, "Dumped done!\n");		
            break;
#endif

	case 0xF:
	  {
  	    pr_debug("cci_rw_info c 0x5F!!!\n");
            pr_debug("cci_rw_info:if_board_evt=%d, cciaboot_flag=%d, startup_magic=0x%lx, warmboot_magic=0x%lx\n", if_board_evt, cciaboot_flag, startup_magic_flag, warmboot_magic_flag);

	    if( if_board_evt != 1)
	    {
	        if(cciaboot_flag == 1) //M:LE
	        {
	            if( (startup_magic_flag & 0xF) == 0x4)
	            {
		        charger_is_detected = 1;
	            }
	        }
	        else // M:LE
	        {
	            if( ( (startup_magic_flag & 0xF) == 0x4) && ( (warmboot_magic_flag & 0xFFFFFFFF) == 0) )
	            {
		        charger_is_detected = 1;	        
	            }
	        }
	    }
	    else
	    {
	    	charger_is_detected = 0; //Prevent evt board to enter chgonly becuase it usually use charger to normal power on
	    }


	    sprintf(buf, "%1d%1d%1d [cap][ftm][chg]\n", key_capture_pressed, ftm_pin_is_low, charger_is_detected);
		
	    pr_debug("buf string = %s", buf); 	
	  }
	  break;

	default:
	  sprintf(buf, "Invalid check cmd\n");
	  break;

      }

#if CMD_ALLOW  
      kfree(buf_sector_rsvsec0);
      kfree(buf_sector_rsvsec2);	  
      kfree(buf_sector_fsg);
      kfree(buf_sector_rsv);
#endif

      if (copy_to_user((void*)arg, buf, sizeof(buf)))
        return -EFAULT;

    }
    break;

    case (DEBUG_INFO_IOCTL_ORIG(TST_CMD_GRP)):
    {
      int l_action, l_id, l_loading;
      int iTest =  cmd & SUB_CMD_MASK;
      int iRet = -1;;

      switch(iTest)
      {
        case 0x0:
#if 0 //M:LE
	  if(copy_from_user(buf, (void*) arg, sizeof(buf)))
	  {
	    return -EFAULT;
	  }	

	  l_action = simple_strtol((char*) buf, NULL, 10);
	  l_id = simple_strtol((char*) &buf[10], NULL, 10);
	  l_loading = simple_strtol((char*) &buf[20], NULL, 10);

  
	  iRet = system_loading_update((enum module_loading_op) l_action, l_id, l_loading);

#else
	  iRet = -1;
#endif

	  memset(buf, 0x0, sizeof(buf));
          if(iRet != -1)
          {
	    sprintf(buf, "%d, <=Ret, input=%d, %d, %d\n", (int)iRet, (int)l_action, (int)l_id, (int)l_loading);
          }
	  else
	  {
	    sprintf(buf, "system_loading_update error, iRet = %d\n", iRet);
	  }
      }

      #if (CCI_KERNEL_INFO_DEBUG)
      printk(buf);
      #endif
      
      if (copy_to_user((void*)arg, buf, sizeof(buf)))
        return -EFAULT;
      if (iRet == -1)
        return -EIO;   
    }
    break;
	
    default:
      break;
  }
  
 if(cci_debug_info_req != NULL)
 	kfree(cci_debug_info_req);
  
 return ioctl_result;
}

static const struct file_operations cci_kernel_info_fops=
{
  .owner = THIS_MODULE,
  .open = cci_kernel_info_open,
  .unlocked_ioctl = cci_kernel_info_ioctl, 
  //.read =
  //.write =  
};


static int cci_kernel_info_setup_cdev(void)
{
  struct device *ccidev;

  int rc, err, devno = MKDEV(cci_kernel_info_major, 0);
  rc = 0;

  cdev_init(&dev.cdev, &cci_kernel_info_fops);
  dev.cdev.owner = THIS_MODULE;
  dev.cdev.ops = &cci_kernel_info_fops;
  err = cdev_add(&dev.cdev, devno, 1);
  if(err)
    Printker("cci_kernel_info: Error %d adding cci_kernel_info\n", err);

  cci_kernel_info_class = class_create(THIS_MODULE,
  				CCI_KERNEL_INFO_DRIVER_NAME);
  
  if (IS_ERR(cci_kernel_info_class)) {
    Printker("cci_kernel_info: failed to create cci kernel info device class\n");
    rc = -ENOMEM;
    goto init_unreg_bail;
  }

  ccidev = device_create(cci_kernel_info_class, NULL, MKDEV(cci_kernel_info_major, 0), NULL, CCI_KERNEL_INFO_DRIVER_NAME);
  
  if (!ccidev) {
  	Printker("cci_kernel_info: failed to create device \n");
  	rc = -ENOMEM;
  	goto init_remove_bail;	
  }

init_unreg_bail:
	  unregister_chrdev_region(MKDEV(cci_kernel_info_major, 0), 1);

init_remove_bail:	
	
  return rc;
}

int cci_kernel_info_init(void)
{
  int result;
  dev_t devno = MKDEV(cci_kernel_info_major, 0);

  Printker("cci_kernel_info: cci_kernel_info_init is in progress!!!!!");

  if (cci_kernel_info_major)
    result = register_chrdev_region(devno, 1, "cci_kernel_info");
  else
  {
    result = alloc_chrdev_region(&devno, 0, 1, "cci_kernel_info");

    cci_kernel_info_major = MAJOR(devno);
  }

  Printker("cci_kernel_info: result = %d!!!!!", result);

  if (result < 0)
    return result;

  cci_kernel_info_setup_cdev();
  
  return 0;
  }



static void cci_kernel_info_exit(void) {
  cdev_del(&dev.cdev);
  unregister_chrdev_region(MKDEV(cci_kernel_info_major, 0), 1);
}
module_init(cci_kernel_info_init);
module_exit(cci_kernel_info_exit);
