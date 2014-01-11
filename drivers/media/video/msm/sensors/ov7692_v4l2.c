/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <mach/msm_iomap.h>//2012/11/15 
#include "msm_sensor.h"
#include "msm_camera_i2c_mux.h"//2012/08/27
#include "msm.h"//2013/01/28
#define SENSOR_NAME "ov7692"

#define CAMERA_EFFECT_OFF		0
#define CAMERA_EFFECT_MONO		1
#define CAMERA_EFFECT_NEGATIVE		2
#define CAMERA_EFFECT_SOLARIZE		3
#define CAMERA_EFFECT_SEPIA		4
#define CAMERA_EFFECT_POSTERIZE         5
#define CAMERA_EFFECT_WHITEBOARD	6
#define CAMERA_EFFECT_BLACKBOARD	7
#define CAMERA_EFFECT_AQUA		8
#define CAMERA_EFFECT_EMBOSS		9
#define CAMERA_EFFECT_SKETCH		10
#define CAMERA_EFFECT_NEON		11
#define CAMERA_EFFECT_USER_DEFINED1     12
#define CAMERA_EFFECT_USER_DEFINED2     13
#define CAMERA_EFFECT_USER_DEFINED3     14
#define CAMERA_EFFECT_USER_DEFINED4     15
#define CAMERA_EFFECT_USER_DEFINED5     16
#define CAMERA_EFFECT_USER_DEFINED6     17
#define CAMERA_EFFECT_MAX               18

DEFINE_MUTEX(ov7692_mut);
//B 2012/11/15 
#define GPIO_CONFIG(gpio)         (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))
#define GPIO_IN_OUT(gpio)         (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))
#define MSM_OV_CAM_CLK 4
//E 2012/11/15 
extern int req_fps;//2013/01/28
static struct msm_sensor_ctrl_t ov7692_s_ctrl;

static int effect_value = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80; /* DEFAULT SATURATION VALUES*/
static unsigned int SAT_V = 0x80; /* DEFAULT SATURATION VALUES*/
static bool power = false;//2012/08/27
static struct msm_camera_i2c_reg_conf ov7692_start_settings[] = {
    //{0x0e, 0x00},//2012/11/15 
    //B 2012/09/21
    {0xff, 0x01},
    {0xb5, 0x40},
    {0xff, 0x00},
    //E 2012/09/21
    {0x0e, 0x00},//2012/11/15 
};

static struct msm_camera_i2c_reg_conf ov7692_stop_settings[] = {
	{0x0e, 0x08},
    //B 2012/09/21
    {0xff, 0x01},
    {0xb5, 0x70},
    {0xff, 0x00},
    //E 2012/09/21
};
//B 2012/10/29
static struct msm_camera_i2c_reg_conf ov7692_reset_settings[] = {
    {0x12, 0x80},
};
//E 2012/10/29
static struct msm_camera_i2c_reg_conf ov7692_recommend_settings[] = {
	//{0x12, 0x80},//2012/10/29
	{0x0e, 0x08},
	{0x69, 0x52},
	{0x1e, 0xb3},
	{0x48, 0x42},
	{0xff, 0x01},
	{0xae, 0xa0},
	{0xa8, 0x26},
	{0xb4, 0xc0},
	{0xb5, 0x40},
	{0x86, 0x48},
	{0xff, 0x00},
	{0x0c, 0x00},
	{0x62, 0x10},
	{0x12, 0x00},
	{0x17, 0x65},
	{0x18, 0xa4},
	{0x19, 0x0c},
	{0x1a, 0xf6},
	{0x3e, 0x30},
	{0x64, 0x0a},
	{0xff, 0x01},
	{0xb4, 0xc0},
	{0xff, 0x00},
	{0x67, 0x20},
	{0x81, 0x3f},
	{0xcc, 0x02},
	{0xcd, 0x80},
	{0xce, 0x01},
	{0xcf, 0xe0},
	{0xc8, 0x02},
	{0xc9, 0x80},
	{0xca, 0x01},
	{0xcb, 0xe0},
	{0xd0, 0x48},
	{0x82, 0x03},
	{0x0e, 0x00},
	{0x70, 0x00},
	{0x71, 0x34},
	{0x74, 0x28},
	{0x75, 0x98},
	{0x76, 0x00},
	{0x77, 0x64},
	{0x78, 0x01},
	{0x79, 0xc2},
	{0x7a, 0x4e},
	{0x7b, 0x1f},
	{0x7c, 0x00},
	{0x11, 0x00},
	{0x20, 0x00},
	{0x21, 0x23},
	{0x50, 0x9a},
	{0x51, 0x80},
	{0x4c, 0x7d},
	{0x0e, 0x00},
	{0x80, 0x7f},
	{0x85, 0x00},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
    //B 2013/04/15
	{0x89, 0x24},
	{0x8a, 0x22},
	{0x8b, 0x1d},
	{0xbb, 0x84},
	{0xbc, 0x57},
    //E 2013/04/15
	{0xbd, 0x27},
	{0xbe, 0x0e},
	{0xbf, 0xb8},
	{0xc0, 0xc5},
	{0xc1, 0x1e},
	{0xb7, 0x05},
	{0xb8, 0x09},
	{0xb9, 0x00},
	{0xba, 0x18},
	{0x5a, 0x1f},
	{0x5b, 0x9f},
	{0x5c, 0x69},
	{0x5d, 0x42},
	{0x24, 0x78},
	{0x25, 0x68},
	{0x26, 0xb3},
	{0xa3, 0x0b},
	{0xa4, 0x15},
	{0xa5, 0x29},
	{0xa6, 0x4a},
	{0xa7, 0x58},
	{0xa8, 0x65},
	{0xa9, 0x70},
	{0xaa, 0x7b},
	{0xab, 0x85},
	{0xac, 0x8e},
	{0xad, 0xa0},
	{0xae, 0xb0},
	{0xaf, 0xcb},
	{0xb0, 0xe1},
	{0xb1, 0xf1},
	{0xb2, 0x14},
	{0x8e, 0x92},
	{0x96, 0xff},
	{0x97, 0x00},
    //B 2013/03/07
	{0x13, 0xf7},
    {0x14, 0x2f},//2013/04/15
	{0x15, 0x00},
	//{0x14, 0x3b},
    //E 2013/03/07
	{0x0e, 0x00},
	//B 2012/11/09
    {0xff, 0x00},
    {0xb4, 0x20},//2013/04/15
    {0xb7, 0x05},//2013/01/03
    //E 2012/11/09
    //B 2013/03/07
    {0xb6, 0x66},
    {0xd3, 0x80},
    //E 2013/03/07
    //B 2013/03/14
    {0x8c, 0x5e},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
    {0x92, 0x8e},
    {0x93, 0x82},
    {0x94, 0x1b},
    {0x95, 0x1c},
	{0x96, 0xff},
	{0x97, 0x00},
    {0x98, 0x30},
    {0x99, 0x10},
    {0x9a, 0x48},
    {0x9b, 0x3e},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
    {0xa0, 0x65},
    {0xa1, 0x5c},
    {0xa2, 0x1c},
    //E 2013/03/14
    //B 2013/04/15
    {0x81, 0x2f},
    {0x5c, 0x1f},
    //E 2013/04/15
};

static struct msm_camera_i2c_reg_conf ov7692_full_settings[] = {
    //B 2013/04/02
    {0xff, 0x00},
    {0x31, 0x83},
    {0x20, 0x00},
    {0x21, 0x23},
    {0x50, 0x9a},
    {0x51, 0x80},
    //E 2013/04/02
    {0x0c, 0x00},//2012/09/19
	{0xcc, 0x02},
	{0xcd, 0x80},
	{0xce, 0x01},
	{0xcf, 0xe0},
	{0xc8, 0x02},
	{0xc9, 0x80},
	{0xca, 0x01},
	{0xcb, 0xe0},
};
//B 2013/01/28
static struct msm_camera_i2c_reg_conf ov7692_half_settings[] = {
    //B 2013/04/02
    {0xff, 0x00},
    {0x31, 0x87},
    {0x20, 0x00},
    {0x21, 0x57},
    {0x50, 0x4d},
    {0x51, 0x40},
    //E 2013/04/02
    {0x0c, 0x00},
	{0xcc, 0x02},
	{0xcd, 0x80},
	{0xce, 0x01},
	{0xcf, 0xe0},
	{0xc8, 0x02},
	{0xc9, 0x80},
	{0xca, 0x01},
	{0xcb, 0xe0},
};
//E 2013/01/28
static struct v4l2_subdev_info ov7692_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};


static struct msm_camera_i2c_conf_array ov7692_init_conf[] = {
        //B 2012/10/29
	{&ov7692_reset_settings[0],
	ARRAY_SIZE(ov7692_reset_settings), 10, MSM_CAMERA_I2C_BYTE_DATA},
        //E 2012/10/29
	{&ov7692_recommend_settings[0],
	ARRAY_SIZE(ov7692_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov7692_confs[] = {
	{&ov7692_full_settings[0],
	ARRAY_SIZE(ov7692_full_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};
//B 2013/01/28
static struct msm_camera_i2c_conf_array ov7692_half_fps_confs[] = {
	{&ov7692_half_settings[0],
	ARRAY_SIZE(ov7692_half_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},	
};
//E 2013/01/28

static struct msm_camera_i2c_reg_conf ov7692_saturation[][4] = {
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x00, 0x00, 0x00, 0x00},
		{0xd9, 0x00, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},/* SATURATION LEVEL0*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x10, 0x00, 0x00, 0x00},
		{0xd9, 0x10, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL1*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x20, 0x00, 0x00, 0x00},
		{0xd9, 0x20, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL2*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x30, 0x00, 0x00, 0x00},
		{0xd9, 0x30, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL3*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x40, 0x00, 0x00, 0x00},
		{0xd9, 0x40, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL4*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x50, 0x00, 0x00, 0x00},
		{0xd9, 0x50, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL5*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x60, 0x00, 0x00, 0x00},
		{0xd9, 0x60, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL6*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x70, 0x00, 0x00, 0x00},
		{0xd9, 0x70, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL7*/
	{{0x81, 0x33, 0x00, 0x00, 0xCC}, {0xd8, 0x80, 0x00, 0x00, 0x00},
		{0xd9, 0x80, 0x00, 0x00, 0x00},
		{0xd2, 0x02, 0x00, 0x00, 0x00},},	/* SATURATION LEVEL8*/
};
static struct msm_camera_i2c_conf_array ov7692_saturation_confs[][1] = {
	{{ov7692_saturation[0], ARRAY_SIZE(ov7692_saturation[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[1], ARRAY_SIZE(ov7692_saturation[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[2], ARRAY_SIZE(ov7692_saturation[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[3], ARRAY_SIZE(ov7692_saturation[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[4], ARRAY_SIZE(ov7692_saturation[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[5], ARRAY_SIZE(ov7692_saturation[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[6], ARRAY_SIZE(ov7692_saturation[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[7], ARRAY_SIZE(ov7692_saturation[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_saturation[8], ARRAY_SIZE(ov7692_saturation[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_saturation_enum_map[] = {
	MSM_V4L2_SATURATION_L0,
	MSM_V4L2_SATURATION_L1,
	MSM_V4L2_SATURATION_L2,
	MSM_V4L2_SATURATION_L3,
	MSM_V4L2_SATURATION_L4,
	MSM_V4L2_SATURATION_L5,
	MSM_V4L2_SATURATION_L6,
	MSM_V4L2_SATURATION_L7,
	MSM_V4L2_SATURATION_L8,
};
static struct msm_sensor_output_info_t ov7692_dimensions[] = {
	{
		.x_output = 0x280,
		.y_output = 0x1E0,
		.line_length_pclk = 0x280,
		.frame_length_lines = 0x1E0,
		/*B:20120821*/
		.vt_pixel_clk = 24000000,
		.op_pixel_clk = 24000000,
		/*E:20120821*/
		.binning_factor = 1,
	},
};

static struct msm_camera_i2c_enum_conf_array ov7692_saturation_enum_confs = {
	.conf = &ov7692_saturation_confs[0][0],
	.conf_enum = ov7692_saturation_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_saturation_enum_map),
	.num_index = ARRAY_SIZE(ov7692_saturation_confs),
	.num_conf = ARRAY_SIZE(ov7692_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_contrast[][16] = {
	{{0xb2, 0x29}, {0xa3, 0x55}, {0xa4, 0x5b}, {0xa5, 0x67}, {0xa6, 0x7e},
		{0xa7, 0x89}, {0xa8, 0x93}, {0xa9, 0x9c}, {0xaa, 0xa4},
		{0xab, 0xac}, {0xac, 0xb3}, {0xad, 0xbe}, {0xae, 0xc7},
		{0xaf, 0xd5}, {0xb0, 0xdd}, {0xb1, 0xe1},},	/* CONTRAST L0*/
	{{0xb2, 0x20}, {0xa3, 0x43}, {0xa4, 0x4a}, {0xa5, 0x58}, {0xa6, 0x73},
		{0xa7, 0x80}, {0xa8, 0x8b}, {0xa9, 0x96}, {0xaa, 0x9f},
		{0xab, 0xa8}, {0xac, 0xb1}, {0xad, 0xbe}, {0xae, 0xc9},
		{0xaf, 0xd8}, {0xb0, 0xe2}, {0xb1, 0xe8},},	/* CONTRAST L1*/
	{{0xb2, 0x18}, {0xa3, 0x31}, {0xa4, 0x39}, {0xa5, 0x4a}, {0xa6, 0x68},
		{0xa7, 0x77}, {0xa8, 0x84}, {0xa9, 0x90}, {0xaa, 0x9b},
		{0xab, 0xa5}, {0xac, 0xaf}, {0xad, 0xbe}, {0xae, 0xca},
		{0xaf, 0xdc}, {0xb0, 0xe7}, {0xb1, 0xee},},	/* CONTRAST L2*/
	{{0xb2, 0x10}, {0xa3, 0x1f}, {0xa4, 0x28}, {0xa5, 0x3b}, {0xa6, 0x5d},
		{0xa7, 0x6e}, {0xa8, 0x7d}, {0xa9, 0x8a}, {0xaa, 0x96},
		{0xab, 0xa2}, {0xac, 0xad}, {0xad, 0xbe}, {0xae, 0xcc},
		{0xaf, 0xe0}, {0xb0, 0xed}, {0xb1, 0xf4},},	/* CONTRAST L3*/
	 {{0xb2, 0x6}, {0xa3, 0xb}, {0xa4, 0x15}, {0xa5, 0x2a}, {0xa6, 0x51},
		{0xa7, 0x63}, {0xa8, 0x74}, {0xa9, 0x83}, {0xaa, 0x91},
		{0xab, 0x9e}, {0xac, 0xaa}, {0xad, 0xbe}, {0xae, 0xce},
		{0xaf, 0xe5}, {0xb0, 0xf3}, {0xb1, 0xfb},},	/* CONTRAST L4*/
	{{0xb2, 0xc}, {0xa3, 0x4}, {0xa4, 0xc}, {0xa5, 0x1f}, {0xa6, 0x45},
		{0xa7, 0x58}, {0xa8, 0x6b}, {0xa9, 0x7c}, {0xaa, 0x8d},
		{0xab, 0x9d}, {0xac, 0xac}, {0xad, 0xc3}, {0xae, 0xd2},
		{0xaf, 0xe8}, {0xb0, 0xf2}, {0xb1, 0xf7},},	/* CONTRAST L5*/
	{{0xb2, 0x1}, {0xa3, 0x2}, {0xa4, 0x9}, {0xa5, 0x1a}, {0xa6, 0x3e},
		{0xa7, 0x4a}, {0xa8, 0x59}, {0xa9, 0x6a}, {0xaa, 0x79},
		{0xab, 0x8e}, {0xac, 0xa4}, {0xad, 0xc1}, {0xae, 0xdb},
		{0xaf, 0xf4}, {0xb0, 0xff}, {0xb1, 0xff},},	/* CONTRAST L6*/
	{{0xb2, 0xc}, {0xa3, 0x4}, {0xa4, 0x8}, {0xa5, 0x17}, {0xa6, 0x27},
		{0xa7, 0x3d}, {0xa8, 0x54}, {0xa9, 0x60}, {0xaa, 0x77},
		{0xab, 0x85}, {0xac, 0xa4}, {0xad, 0xc6}, {0xae, 0xd2},
		{0xaf, 0xe9}, {0xb0, 0xf0}, {0xb1, 0xf7},},	/* CONTRAST L7*/
	{{0xb2, 0x1}, {0xa3, 0x4}, {0xa4, 0x4}, {0xa5, 0x7}, {0xa6, 0xb},
		{0xa7, 0x17}, {0xa8, 0x2a}, {0xa9, 0x41}, {0xaa, 0x59},
		{0xab, 0x6b}, {0xac, 0x8b}, {0xad, 0xb1}, {0xae, 0xd2},
		{0xaf, 0xea}, {0xb0, 0xf4}, {0xb1, 0xff},},	/* CONTRAST L8*/
};

static struct msm_camera_i2c_conf_array ov7692_contrast_confs[][1] = {
	{{ov7692_contrast[0], ARRAY_SIZE(ov7692_contrast[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[1], ARRAY_SIZE(ov7692_contrast[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[2], ARRAY_SIZE(ov7692_contrast[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[3], ARRAY_SIZE(ov7692_contrast[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[4], ARRAY_SIZE(ov7692_contrast[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[5], ARRAY_SIZE(ov7692_contrast[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[6], ARRAY_SIZE(ov7692_contrast[6]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[7], ARRAY_SIZE(ov7692_contrast[7]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_contrast[8], ARRAY_SIZE(ov7692_contrast[8]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};


static int ov7692_contrast_enum_map[] = {
	MSM_V4L2_CONTRAST_L0,
	MSM_V4L2_CONTRAST_L1,
	MSM_V4L2_CONTRAST_L2,
	MSM_V4L2_CONTRAST_L3,
	MSM_V4L2_CONTRAST_L4,
	MSM_V4L2_CONTRAST_L5,
	MSM_V4L2_CONTRAST_L6,
	MSM_V4L2_CONTRAST_L7,
	MSM_V4L2_CONTRAST_L8,
};

static struct msm_camera_i2c_enum_conf_array ov7692_contrast_enum_confs = {
	.conf = &ov7692_contrast_confs[0][0],
	.conf_enum = ov7692_contrast_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_contrast_enum_map),
	.num_index = ARRAY_SIZE(ov7692_contrast_confs),
	.num_conf = ARRAY_SIZE(ov7692_contrast_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};
static struct msm_camera_i2c_reg_conf ov7692_sharpness[][2] = {
	{{0xb4, 0x20, 0x00, 0x00, 0xDF},
		{0xb6, 0x00, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 0*/
	{{0xb4, 0x20, 0x00, 0x00, 0xDF},
		{0xb6, 0x01, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 1*/
	{{0xb4, 0x00, 0x00, 0x00, 0xDF},
		{0xb6, 0x00, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 2*/
	{{0xb4, 0x20, 0x00, 0x00, 0xDF},
		{0xb6, 0x66, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 3*/
	{{0xb4, 0x20, 0x00, 0x00, 0xDF},
		{0xb6, 0x99, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 4*/
	{{0xb4, 0x20, 0x00, 0x00, 0xDF},
		{0xb6, 0xcc, 0x00, 0x00, 0xE0},},    /* SHARPNESS LEVEL 5*/
};

static struct msm_camera_i2c_conf_array ov7692_sharpness_confs[][1] = {
	{{ov7692_sharpness[0], ARRAY_SIZE(ov7692_sharpness[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_sharpness[1], ARRAY_SIZE(ov7692_sharpness[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_sharpness[2], ARRAY_SIZE(ov7692_sharpness[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_sharpness[3], ARRAY_SIZE(ov7692_sharpness[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_sharpness[4], ARRAY_SIZE(ov7692_sharpness[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_sharpness[5], ARRAY_SIZE(ov7692_sharpness[5]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_sharpness_enum_map[] = {
	MSM_V4L2_SHARPNESS_L0,
	MSM_V4L2_SHARPNESS_L1,
	MSM_V4L2_SHARPNESS_L2,
	MSM_V4L2_SHARPNESS_L3,
	MSM_V4L2_SHARPNESS_L4,
	MSM_V4L2_SHARPNESS_L5,
};

static struct msm_camera_i2c_enum_conf_array ov7692_sharpness_enum_confs = {
	.conf = &ov7692_sharpness_confs[0][0],
	.conf_enum = ov7692_sharpness_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_sharpness_enum_map),
	.num_index = ARRAY_SIZE(ov7692_sharpness_confs),
	.num_conf = ARRAY_SIZE(ov7692_sharpness_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_exposure[][3] = {
	{{0x24, 0x50}, {0x25, 0x40}, {0x26, 0xa2},}, /*EXPOSURECOMPENSATIONN2*/
	{{0x24, 0x70}, {0x25, 0x60}, {0x26, 0xa2},}, /*EXPOSURECOMPENSATIONN1*/
	{{0x24, 0x86}, {0x25, 0x76}, {0x26, 0xb3},}, /*EXPOSURECOMPENSATIOND*/
	{{0x24, 0xa8}, {0x25, 0xa0}, {0x26, 0xc4},}, /*EXPOSURECOMPENSATIONp1*/
	{{0x24, 0xc0}, {0x25, 0xb8}, {0x26, 0xe6},}, /*EXPOSURECOMPENSATIONP2*/
};

static struct msm_camera_i2c_conf_array ov7692_exposure_confs[][1] = {
	{{ov7692_exposure[0], ARRAY_SIZE(ov7692_exposure[0]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_exposure[1], ARRAY_SIZE(ov7692_exposure[1]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_exposure[2], ARRAY_SIZE(ov7692_exposure[2]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_exposure[3], ARRAY_SIZE(ov7692_exposure[3]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_exposure[4], ARRAY_SIZE(ov7692_exposure[4]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_exposure_enum_map[] = {
	MSM_V4L2_EXPOSURE_N2,
	MSM_V4L2_EXPOSURE_N1,
	MSM_V4L2_EXPOSURE_D,
	MSM_V4L2_EXPOSURE_P1,
	MSM_V4L2_EXPOSURE_P2,
};

static struct msm_camera_i2c_enum_conf_array ov7692_exposure_enum_confs = {
	.conf = &ov7692_exposure_confs[0][0],
	.conf_enum = ov7692_exposure_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_exposure_enum_map),
	.num_index = ARRAY_SIZE(ov7692_exposure_confs),
	.num_conf = ARRAY_SIZE(ov7692_exposure_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_iso[][1] = {
	{{0x14, 0x20, 0x00, 0x00, 0x8F},},   /*ISO_AUTO*/
	{{0x14, 0x20, 0x00, 0x00, 0x8F},},   /*ISO_DEBLUR*/
	{{0x14, 0x00, 0x00, 0x00, 0x8F},},   /*ISO_100*/
	{{0x14, 0x10, 0x00, 0x00, 0x8F},},   /*ISO_200*/
	{{0x14, 0x20, 0x00, 0x00, 0x8F},},   /*ISO_400*/
	{{0x14, 0x30, 0x00, 0x00, 0x8F},},   /*ISO_800*/
	{{0x14, 0x40, 0x00, 0x00, 0x8F},},   /*ISO_1600*/
};


static struct msm_camera_i2c_conf_array ov7692_iso_confs[][1] = {
	{{ov7692_iso[0], ARRAY_SIZE(ov7692_iso[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_iso[1], ARRAY_SIZE(ov7692_iso[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_iso[2], ARRAY_SIZE(ov7692_iso[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_iso[3], ARRAY_SIZE(ov7692_iso[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_iso[4], ARRAY_SIZE(ov7692_iso[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_iso[5], ARRAY_SIZE(ov7692_iso[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_iso_enum_map[] = {
	MSM_V4L2_ISO_AUTO ,
	MSM_V4L2_ISO_DEBLUR,
	MSM_V4L2_ISO_100,
	MSM_V4L2_ISO_200,
	MSM_V4L2_ISO_400,
	MSM_V4L2_ISO_800,
	MSM_V4L2_ISO_1600,
};


static struct msm_camera_i2c_enum_conf_array ov7692_iso_enum_confs = {
	.conf = &ov7692_iso_confs[0][0],
	.conf_enum = ov7692_iso_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_iso_enum_map),
	.num_index = ARRAY_SIZE(ov7692_iso_confs),
	.num_conf = ARRAY_SIZE(ov7692_iso_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_no_effect[] = {
	{0x81, 0x00, 0x00, 0x00, 0xDF},
	{0x28, 0x00,},
	{0xd2, 0x00,},
	{0xda, 0x80,},
	{0xdb, 0x80,},
};

static struct msm_camera_i2c_conf_array ov7692_no_effect_confs[] = {
	{&ov7692_no_effect[0],
	ARRAY_SIZE(ov7692_no_effect), 0,
	MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},
};

static struct msm_camera_i2c_reg_conf ov7692_special_effect[][5] = {
	{{0x81, 0x20, 0x00, 0x00, 0xDF}, {0x28, 0x00,}, {0xd2, 0x18,},
		{0xda, 0x80,}, {0xdb, 0x80,},},	/*for special effect OFF*/
	{{0x81, 0x20, 0x00, 0x00, 0xDF}, {0x28, 0x00,}, {0xd2, 0x18,},
		{0xda, 0x80,}, {0xdb, 0x80,},},	/*for special effect MONO*/
	{{0x81, 0x20, 0x00, 0x00, 0xDF}, {0x28, 0x80,}, {0xd2, 0x40,},
		{0xda, 0x80,}, {0xdb, 0x80,},},	/*for special efefct Negative*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},/*Solarize is not supported by sensor*/
	{{0x81, 0x20, 0x00, 0x00, 0xDF}, {0x28, 0x00,}, {0xd2, 0x18,},
		{0xda, 0x40,}, {0xdb, 0xa0,},},	/*for sepia*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/* Posteraize not supported */
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/* White board not supported*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*Blackboard not supported*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*Aqua not supported*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*Emboss not supported */
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*sketch not supported*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*Neon not supported*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},		/*MAX value*/
};

static struct msm_camera_i2c_conf_array ov7692_special_effect_confs[][1] = {
	{{ov7692_special_effect[0],  ARRAY_SIZE(ov7692_special_effect[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[1],  ARRAY_SIZE(ov7692_special_effect[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[2],  ARRAY_SIZE(ov7692_special_effect[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[3],  ARRAY_SIZE(ov7692_special_effect[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[4],  ARRAY_SIZE(ov7692_special_effect[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[5],  ARRAY_SIZE(ov7692_special_effect[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[6],  ARRAY_SIZE(ov7692_special_effect[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[7],  ARRAY_SIZE(ov7692_special_effect[7]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[8],  ARRAY_SIZE(ov7692_special_effect[8]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[9],  ARRAY_SIZE(ov7692_special_effect[9]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[10], ARRAY_SIZE(ov7692_special_effect[10]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[11], ARRAY_SIZE(ov7692_special_effect[11]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_special_effect[12], ARRAY_SIZE(ov7692_special_effect[12]), 0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_special_effect_enum_map[] = {
	MSM_V4L2_EFFECT_OFF,
	MSM_V4L2_EFFECT_MONO,
	MSM_V4L2_EFFECT_NEGATIVE,
	MSM_V4L2_EFFECT_SOLARIZE,
	MSM_V4L2_EFFECT_SEPIA,
	MSM_V4L2_EFFECT_POSTERAIZE,
	MSM_V4L2_EFFECT_WHITEBOARD,
	MSM_V4L2_EFFECT_BLACKBOARD,
	MSM_V4L2_EFFECT_AQUA,
	MSM_V4L2_EFFECT_EMBOSS,
	MSM_V4L2_EFFECT_SKETCH,
	MSM_V4L2_EFFECT_NEON,
	MSM_V4L2_EFFECT_MAX,
};

static struct msm_camera_i2c_enum_conf_array
		 ov7692_special_effect_enum_confs = {
	.conf = &ov7692_special_effect_confs[0][0],
	.conf_enum = ov7692_special_effect_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_special_effect_enum_map),
	.num_index = ARRAY_SIZE(ov7692_special_effect_confs),
	.num_conf = ARRAY_SIZE(ov7692_special_effect_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_antibanding[][2] = {
	{{0x13, 0x20, 0x00, 0x00, 0xDF},
		{0x14, 0x16, 0x00, 0x00, 0xE8},},   /*ANTIBANDING 60HZ*/
	{{0x13, 0x20, 0x00, 0x00, 0xDF},
		{0x14, 0x17, 0x00, 0x00, 0xE8},},   /*ANTIBANDING 50HZ*/
	{{0x13, 0x20, 0x00, 0x00, 0xDF},
		{0x14, 0x14, 0x00, 0x00, 0xE8},},   /* ANTIBANDING AUTO*/
};


static struct msm_camera_i2c_conf_array ov7692_antibanding_confs[][1] = {
	{{ov7692_antibanding[0], ARRAY_SIZE(ov7692_antibanding[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_antibanding[1], ARRAY_SIZE(ov7692_antibanding[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_antibanding[2], ARRAY_SIZE(ov7692_antibanding[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_antibanding_enum_map[] = {
	MSM_V4L2_POWER_LINE_60HZ,
	MSM_V4L2_POWER_LINE_50HZ,
	MSM_V4L2_POWER_LINE_AUTO,
};


static struct msm_camera_i2c_enum_conf_array ov7692_antibanding_enum_confs = {
	.conf = &ov7692_antibanding_confs[0][0],
	.conf_enum = ov7692_antibanding_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_antibanding_enum_map),
	.num_index = ARRAY_SIZE(ov7692_antibanding_confs),
	.num_conf = ARRAY_SIZE(ov7692_antibanding_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};

static struct msm_camera_i2c_reg_conf ov7692_wb_oem[][4] = {
	{{-1, -1, -1, -1 , -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},/*WHITEBALNACE OFF*/
	{{0x13, 0xf7}, {0x15, 0x00}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},}, /*WHITEBALNACE AUTO*/
	{{0x13, 0xf5}, {0x01, 0x56}, {0x02, 0x50},
		{0x15, 0x00},},	/*WHITEBALNACE CUSTOM*/
	{{0x13, 0xf5}, {0x01, 0x66}, {0x02, 0x40},
		{0x15, 0x00},},	/*INCANDISCENT*/
	{{-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1},},	/*FLOURESECT NOT SUPPORTED */
	{{0x13, 0xf5}, {0x01, 0x43}, {0x02, 0x5d},
		{0x15, 0x00},},	/*DAYLIGHT*/
	{{0x13, 0xf5}, {0x01, 0x48}, {0x02, 0x63},
		{0x15, 0x00},},	/*CLOUDY*/
};

static struct msm_camera_i2c_conf_array ov7692_wb_oem_confs[][1] = {
	{{ov7692_wb_oem[0], ARRAY_SIZE(ov7692_wb_oem[0]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[1], ARRAY_SIZE(ov7692_wb_oem[1]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[2], ARRAY_SIZE(ov7692_wb_oem[2]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[3], ARRAY_SIZE(ov7692_wb_oem[3]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[4], ARRAY_SIZE(ov7692_wb_oem[4]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[5], ARRAY_SIZE(ov7692_wb_oem[5]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
	{{ov7692_wb_oem[6], ARRAY_SIZE(ov7692_wb_oem[6]),  0,
		MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA},},
};

static int ov7692_wb_oem_enum_map[] = {
	MSM_V4L2_WB_OFF,
	MSM_V4L2_WB_AUTO ,
	MSM_V4L2_WB_CUSTOM,
	MSM_V4L2_WB_INCANDESCENT,
	MSM_V4L2_WB_FLUORESCENT,
	MSM_V4L2_WB_DAYLIGHT,
	MSM_V4L2_WB_CLOUDY_DAYLIGHT,
};

static struct msm_camera_i2c_enum_conf_array ov7692_wb_oem_enum_confs = {
	.conf = &ov7692_wb_oem_confs[0][0],
	.conf_enum = ov7692_wb_oem_enum_map,
	.num_enum = ARRAY_SIZE(ov7692_wb_oem_enum_map),
	.num_index = ARRAY_SIZE(ov7692_wb_oem_confs),
	.num_conf = ARRAY_SIZE(ov7692_wb_oem_confs[0]),
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
};


int ov7692_saturation_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	if (value <= MSM_V4L2_SATURATION_L8)
		SAT_U = SAT_V = value * 0x10;
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}


int ov7692_contrast_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int ov7692_sharpness_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int ov7692_effect_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	effect_value = value;
	if (effect_value == CAMERA_EFFECT_OFF) {
		rc = msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->no_effect_settings, 0);
		if (rc < 0) {
			CDBG("write faield\n");
			return rc;
		}
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xda, SAT_U,
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xdb, SAT_V,
			MSM_CAMERA_I2C_BYTE_DATA);
	} else {
		rc = msm_sensor_write_enum_conf_array(
			s_ctrl->sensor_i2c_client,
			ctrl_info->enum_cfg_settings, value);
	}
	return rc;
}

int ov7692_antibanding_msm_sensor_s_ctrl_by_enum(
		struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
		return rc;
}

int ov7692_msm_sensor_s_ctrl_by_enum(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	rc = msm_sensor_write_enum_conf_array(
		s_ctrl->sensor_i2c_client,
		ctrl_info->enum_cfg_settings, value);
	if (rc < 0) {
		CDBG("write faield\n");
		return rc;
	}
	return rc;
}

struct msm_sensor_v4l2_ctrl_info_t ov7692_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L8,
		.step = 1,
		.enum_cfg_settings = &ov7692_saturation_enum_confs,
		.s_v4l2_ctrl = ov7692_saturation_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L8,
		.step = 1,
		.enum_cfg_settings = &ov7692_contrast_enum_confs,
		.s_v4l2_ctrl = ov7692_contrast_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L5,
		.step = 1,
		.enum_cfg_settings = &ov7692_sharpness_enum_confs,
		.s_v4l2_ctrl = ov7692_sharpness_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.enum_cfg_settings = &ov7692_exposure_enum_confs,
		.s_v4l2_ctrl = ov7692_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = MSM_V4L2_PID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_1600,
		.step = 1,
		.enum_cfg_settings = &ov7692_iso_enum_confs,
		.s_v4l2_ctrl = ov7692_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.enum_cfg_settings = &ov7692_special_effect_enum_confs,
		.s_v4l2_ctrl = ov7692_effect_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_60HZ,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.enum_cfg_settings = &ov7692_antibanding_enum_confs,
		.s_v4l2_ctrl = ov7692_antibanding_msm_sensor_s_ctrl_by_enum,
	},
	{
		.ctrl_id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.min = MSM_V4L2_WB_OFF,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.enum_cfg_settings = &ov7692_wb_oem_enum_confs,
		.s_v4l2_ctrl = ov7692_msm_sensor_s_ctrl_by_enum,
	},

};

/*
static struct msm_camera_csi_params ov7692_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x14,
};
*/

static struct msm_sensor_output_reg_addr_t ov7692_reg_addr = {
	.x_output = 0xCC,
	.y_output = 0xCE,
	.line_length_pclk = 0xC8,
	.frame_length_lines = 0xCA,
};

static struct msm_sensor_id_info_t ov7692_id_info = {
	.sensor_id_reg_addr = 0x0A,
	.sensor_id = 0x7692,
};

static const struct i2c_device_id ov7692_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov7692_s_ctrl},
	{ }
};


static struct i2c_driver ov7692_i2c_driver = {
	.id_table = ov7692_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov7692_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("OV7692\n");

	rc = i2c_add_driver(&ov7692_i2c_driver);

	return rc;
}

static struct v4l2_subdev_core_ops ov7692_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov7692_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov7692_subdev_ops = {
	.core = &ov7692_subdev_core_ops,
	.video  = &ov7692_subdev_video_ops,
};

//B 2012/08/27
static struct msm_cam_clk_info ov_cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

int32_t ov7692_v4l2_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;
    struct msm_camera_sensor_info *data = s_ctrl->sensordata;
    unsigned config = GPIO_CFG(MSM_OV_CAM_CLK, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);//2012/11/15 
	struct device *dev = NULL;

	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
    
    pr_err("%s: %s\n", __func__, data->sensor_name);
    s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
    if (!s_ctrl->reg_ptr) {
	    pr_err("%s: could not allocate mem for regulators\n", __func__);
		return -ENOMEM;
    }

    rc = msm_camera_request_gpio_table(data, 1);
    if (rc < 0) {
	    pr_err("%s: request gpio failed\n", __func__);
	    goto request_gpio_failed;
    }

    if (!power) {
        rc = msm_camera_config_vreg(dev,
	    	    s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		        s_ctrl->sensordata->sensor_platform_info->num_vreg,
        	    s_ctrl->vreg_seq,
		        s_ctrl->num_vreg_seq,
		        s_ctrl->reg_ptr, 1);
	if (rc < 0) {
	    pr_err("%s: regulator on failed\n", __func__);
	    goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(dev,
		    s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		    s_ctrl->sensordata->sensor_platform_info->num_vreg,
            s_ctrl->vreg_seq,
		    s_ctrl->num_vreg_seq,
		    s_ctrl->reg_ptr, 1);
	if (rc < 0) {
	    pr_err("%s: enable regulator failed\n", __func__);
	    goto enable_vreg_failed;
	}
        power = true;
    }
    mdelay(10);//2012/10/29

    rc = msm_camera_config_gpio_table(data, 1);
    if (rc < 0) {
	    pr_err("%s: config gpio failed\n", __func__);
	    goto config_gpio_failed;
    }

    //mdelay(25);//2012/10/29
    mdelay(35);//2012/11/15 

    if (s_ctrl->clk_rate != 0)
        ov_cam_clk_info->clk_rate = s_ctrl->clk_rate;
    gpio_tlmm_config(config, 0);//2012/11/15 
    rc = msm_cam_clk_enable(dev,
		ov_cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(ov_cam_clk_info), 1);
    if (rc < 0) {
	    pr_err("%s: clk enable failed\n", __func__);
	    goto enable_clk_failed;
    }

    //usleep_range(1000, 2000);
    //mdelay(5);//2012/10/29
    mdelay(8);//2012/11/15 
    if (data->sensor_platform_info->ext_power_ctrl != NULL)
	    data->sensor_platform_info->ext_power_ctrl(1);

    if (data->sensor_platform_info->i2c_conf && data->sensor_platform_info->i2c_conf->use_i2c_mux)
	    msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		rc = msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto cci_init_failed;
		}
	}
	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
	return rc;

cci_init_failed:
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

enable_clk_failed:
    msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
    msm_camera_enable_vreg(dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
            s_ctrl->vreg_seq,
		    s_ctrl->num_vreg_seq,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
    msm_camera_config_vreg(dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
        s_ctrl->vreg_seq,
		s_ctrl->num_vreg_seq,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
    msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
    kfree(s_ctrl->reg_ptr);
    return rc;
}

int32_t ov7692_v4l2_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_RELEASE);
	}
	CDBG("%s\n", __func__);
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);
    
	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	msm_cam_clk_enable(dev,
		ov_cam_clk_info, s_ctrl->cam_clk, ARRAY_SIZE(ov_cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
        /*
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
        */
	msm_camera_request_gpio_table(data, 0);
	kfree(s_ctrl->reg_ptr);
	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
	return 0;
}
//E 2012/08/27
//B 2013/01/28
int32_t ov7692_write_res_settings(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t res)
{
	int32_t rc;

    if (res == 1) {
        res = 0;
   	    rc = msm_sensor_write_conf_array(
		    s_ctrl->sensor_i2c_client,
		    s_ctrl->msm_sensor_reg->half_fps_settings, res);
	    if (rc < 0)
		    return rc;
    } else {
	    rc = msm_sensor_write_conf_array(
		    s_ctrl->sensor_i2c_client,
		    s_ctrl->msm_sensor_reg->mode_settings, res);
	    if (rc < 0)
		    return rc;
    }
	rc = msm_sensor_write_output_settings(s_ctrl, res);
	if (rc < 0)
		return rc;
    
	if (s_ctrl->func_tbl->sensor_adjust_frame_lines)
		s_ctrl->func_tbl->sensor_adjust_frame_lines(s_ctrl);

	return rc;
}
//B 2013/04/02
int32_t ov7692_set_fps(struct msm_sensor_ctrl_t *s_ctrl,
						struct fps_cfg *fps)
{
    int new_fps = 0;//2013/01/28
    
	//s_ctrl->fps_divider = fps->fps_div; //2013/06/28, frame_length_lines affected by this value
    s_ctrl->fps_divider = fps->fps_div;//2013/07/03
    //B 2013/01/28
    new_fps = s_ctrl->msm_sensor_reg->output_settings->vt_pixel_clk
                  / s_ctrl->msm_sensor_reg->output_settings->frame_length_lines
                  * Q10
                  / s_ctrl->msm_sensor_reg->output_settings->line_length_pclk
                  / fps->fps_div;

    if (new_fps != req_fps && (new_fps == 15 || new_fps == 30)) {
        req_fps = new_fps;
        if (new_fps == 15) {                
            ov7692_write_res_settings(s_ctrl, 1);
        } else {
            ov7692_write_res_settings(s_ctrl, 0);
        }            
    }    
    //E 2013/01/28
    
	return 0;
}
//E 2013/04/02
int32_t ov7692_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
        if (req_fps == 15)
            res = 1;
		ov7692_write_res_settings(s_ctrl, res);
        res = 0;

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
        //B 2013/03/07
		//msleep(600);
        //pr_err("%s: delay 600ms to skip frames\n", __func__);
        //E 2013/03/07
	}
	return rc;
}
//E 2013/01/28

static struct msm_sensor_fn_t ov7692_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = ov7692_set_fps,//2013/04/02
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = ov7692_setting,//2013/01/28
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
        //B 2012/08/27
	.sensor_power_up = ov7692_v4l2_power_up,
	.sensor_power_down = ov7692_v4l2_power_down,
        //E 2012/08/27
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t ov7692_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov7692_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov7692_start_settings),
	.stop_stream_conf = ov7692_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov7692_stop_settings),
	.init_settings = &ov7692_init_conf[0],
	.init_size = ARRAY_SIZE(ov7692_init_conf),
	.mode_settings = &ov7692_confs[0],
	.no_effect_settings = &ov7692_no_effect_confs[0],
	.output_settings = &ov7692_dimensions[0],
	.num_conf = ARRAY_SIZE(ov7692_confs),
	.half_fps_settings = &ov7692_half_fps_confs[0],//2013/01/28
};

static struct msm_sensor_ctrl_t ov7692_s_ctrl = {
	.msm_sensor_reg = &ov7692_regs,
	.msm_sensor_v4l2_ctrl_info = ov7692_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(ov7692_v4l2_ctrl_info),
	.sensor_i2c_client = &ov7692_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.sensor_output_reg_addr = &ov7692_reg_addr,
	.sensor_id_info = &ov7692_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &ov7692_mut,
	.sensor_i2c_driver = &ov7692_i2c_driver,
	.sensor_v4l2_subdev_info = ov7692_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov7692_subdev_info),
	.sensor_v4l2_subdev_ops = &ov7692_subdev_ops,
	.func_tbl = &ov7692_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
