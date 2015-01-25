/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
 * Copyright (C) 2012 Sony Mobile Communications AB.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>
#include <linux/delay.h> //Taylor--20121018

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)

static void memset16(void *_ptr, unsigned short val, unsigned count)
{
	//unsigned short *ptr = _ptr;
	unsigned int *ptr = _ptr; //Taylor--20121018
	unsigned int rgb888_val;

	//Taylor--20121018--B
	unsigned int r = (val & 0xF800 )>>11;
	unsigned int g = (val & 0x07E0 )>> 5;
	unsigned int b = (val    ) & 0x001F;

	rgb888_val = (r<<3) | (r>>2); 
	rgb888_val |=((g<<2) | (g>>4))<<8;
	rgb888_val |=((b<<3) | (b>>2))<<16;
	//Taylor--20121018--E

	count >>= 1;
	while (count--)
		*ptr++ = rgb888_val; //Taylor--20121018
		//*ptr++ = val;
}

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename, bool bf_supported)
{
	struct fb_info *info;
	int fd, count, err = 0;
	unsigned max;
	//unsigned short *data, *bits, *ptr;
	unsigned short *data, *ptr; //Taylor--20121018
	unsigned int *bits; //Taylor--20121018

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = sys_lseek(fd, (off_t)0, 2);
	if (count <= 0) {
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if (sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	if (bf_supported && (info->node == 1 || info->node == 2)) {
		err = -EPERM;
		pr_err("%s:%d no info->creen_base on fb%d!\n",
		       __func__, __LINE__, info->node);
		goto err_logo_free_data;
	}
	if (info->screen_base) {
		//bits = (unsigned short *)(info->screen_base);
		bits = (unsigned int *)(info->screen_base); //Taylor--20121018
		while (count > 3) {
			unsigned n = ptr[0];
			if (n > max)
				break;
			memset16(bits, ptr[1], n << 1);
			bits += n;
			max -= n;
			ptr += 2;
			count -= 4;
		}
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);
