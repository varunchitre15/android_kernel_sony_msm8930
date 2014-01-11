/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/proc_fs.h> 
#include "msm_sensor.h"
#define SENSOR_NAME "imx134"
#define PLATFORM_DRIVER_NAME "msm_camera_imx134"
#define imx134_obj imx134_##obj

//S  JackBB 2012/12/3 [Q111M]
/* Register addresses for HDR control */
#define SHORT_SHUTTER_WORD_ADDR                0x0230
#define SHORT_GAIN_BYTE_ADDR           0x0233
#define ABS_GAIN_R_WORD_ADDR           0x0714
#define ABS_GAIN_B_WORD_ADDR           0x0716
#define LSC_ENABLE_BYTE_ADDR           0x0700
#define EN_LSC_BYTE_ADDR                       0x4500
#define RAM_SEL_TOGGLE_BYTE_ADDR       0x3A63
#define LSC_TABLE_START_ADDR           0x4800
#define TC_OUT_NOISE_REGADDR        0x4452
#define TC_OUT_MID_REGADDR          0x4454
#define TC_NOISE_BRATE_REGADDR      0x4458
#define TC_MID_BRATE_REGADDR        0x4459
#define TC_SWITCH_REGADDR           0x446C

#define TC_SWITCH_BYTE_ADDR            0x446C
#define TC_OUT_NOISE_WORD_ADDR         0x4452
#define TC_OUT_MID_WORD_ADDR           0x4454
#define TC_OUT_SAT_WORD_ADDR           0x4456

#define LSC_TABLE_LEN_BYTES                    280 //504  //for IMX134 it is {7 X 5} => 280
#define IMX135_TC_SWITCH_ENABLE 0

/*Adaptive tone reproduction curve*/
#define INIT_ATR_OUT_NOISE     0x100
#define INIT_ATR_OUT_MID       0xA00
#define INIT_ATR_OUT_SAT       0xFFF
#define ATR_OFFSET             0x00    
// Luke 0701 HDR video
#define THRESHOLD_DEAD_ZONE    30 //3// 255 /*lokesh:  just a hack */
#define THRESHOLD_0            30 //3 //20 /*lokesh: th0 should be same as deadzone*/       
#define THRESHOLD_1            42//13
#define INIT_ATR_GAIN          0.5 

/*Controlling Over exposure*/
#define Y_STATS_DIFF_THRESHOLD 50
#define Y_OVER_EXP_THRESHOLD   192 //768
#define FRAME_MINIMUM_INPUT    60

#define DEBUG_AVG_BYTE_ADDR    0x4470
#define DEBUG_MIN_BYTE_ADDR    0x4472
#define DEBUG_MAX_BYTE_ADDR    0x4474
#define DEBUG_SEL_BYTE_ADDR    0x4476

//E  JackBB 2012/12/3 [Q111M]

DEFINE_MUTEX(imx134_mut);
static struct msm_sensor_ctrl_t imx134_s_ctrl;

static struct msm_camera_i2c_reg_conf imx134_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx134_stop_settings[] = {
	{0x0100, 0x00},
	{0x3A43, 0x01},
};
//B 2012/11/06
static struct msm_camera_i2c_reg_conf imx134_flash_settings[] = {
  	{0x0100, 0x01},
   	{0x0800, 0x01},
        {0x0801, 0xc8},
        {0x0802, 0x01},
        {0x0804, 0x02},
        {0x080a, 0xf0},
        {0x080b, 0x00},
        {0x3104, 0x00},
        {0x3108, 0x01},
};
//E 2012/11/06
static struct msm_camera_i2c_reg_conf imx134_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx134_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx134_prev_settings[] = {
  /* reg_C
        1/2HV
        H : 1640
        V : 1232  */
	/* PLL setting 20120824*/
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x01},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x3F},
    {0x0309, 0x05},
    {0x030B, 0x02},
    {0x030C, 0x00},
    {0x030D, 0x64},
    {0x030E, 0x01},
    {0x3A06, 0x12},
    {0x0101, 0x03},//2012/09/19
    {0x0105, 0x00},
    {0x0108, 0x03},
    {0x0109, 0x30},
    {0x010B, 0x32},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x01},
    {0x0391, 0x22},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4083, 0x01},
    {0x0340, 0x0B},
    {0x0341, 0x90},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x00},
    {0x0345, 0x00},
    {0x0346, 0x00},
    {0x0347, 0x00},
    {0x0348, 0x0C},
    {0x0349, 0xCF},
    {0x034A, 0x09},
    {0x034B, 0x9F},
    {0x034C, 0x06},
    {0x034D, 0x68},
    {0x034E, 0x04},
    {0x034F, 0xD0},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x06},
    {0x0355, 0x68},
    {0x0356, 0x04},
    {0x0357, 0xD0},
    {0x3310, 0x06},
    {0x3311, 0x68},
    {0x3312, 0x04},
    {0x3313, 0xD0},
    {0x331C, 0x00},
    {0x331D, 0x70},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    {0x0830, 0x67},
    {0x0831, 0x1F},
    {0x0832, 0x47},
    {0x0833, 0x1F},
    {0x0834, 0x1F},
    {0x0835, 0x17},
    {0x0836, 0x77},
    {0x0837, 0x27},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    {0x0202, 0x05},
    {0x0203, 0x20},
    {0x0240, 0x00},
    {0x0241, 0x00},
    {0x0254, 0x00},
    {0x0205, 0x33},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    {0x0216, 0x01},
    {0x0217, 0x00},
    {0x0218, 0x01},
    {0x0219, 0x00},
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x08},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},

};

static struct msm_camera_i2c_reg_conf imx134_snap_settings[] = {
   /*reg_A
        Full-resolution     *30fps
        H : 3280
        V : 2464
        */
	/* PLL setting 20120824*/
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x01},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x3F},
    {0x0309, 0x05},
    {0x030B, 0x01},
    {0x030C, 0x00},
    {0x030D, 0x64},
    {0x030E, 0x01},
    {0x3A06, 0x11},
    {0x0101, 0x03},//2012/09/19
    {0x0105, 0x00},
    {0x0108, 0x03},
    {0x0109, 0x30},
    {0x010B, 0x32},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x00},
    {0x0391, 0x11},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4083, 0x01},
    {0x0340, 0x0B},
    {0x0341, 0x90},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x00},
    {0x0345, 0x00},
    {0x0346, 0x00},
    {0x0347, 0x00},
    {0x0348, 0x0C},
    {0x0349, 0xCF},
    {0x034A, 0x09},
    {0x034B, 0x9F},
    {0x034C, 0x0C},
    {0x034D, 0xD0},
    {0x034E, 0x09},
    {0x034F, 0xA0},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x0C},
    {0x0355, 0xD0},
    {0x0356, 0x09},
    {0x0357, 0xA0},
    {0x3310, 0x0C},
    {0x3311, 0xD0},
    {0x3312, 0x09},
    {0x3313, 0xA0},
    {0x331C, 0x06},
    {0x331D, 0x00},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    {0x0830, 0x7F},
    {0x0831, 0x37},
    {0x0832, 0x5F},
    {0x0833, 0x37},
    {0x0834, 0x37},
    {0x0835, 0x3F},
    {0x0836, 0xC7},
    {0x0837, 0x3F},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    {0x0202, 0x0A},
    {0x0203, 0x44},
    {0x0240, 0x00},
    {0x0241, 0x00},
    {0x0254, 0x00},
    {0x0205, 0x33},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    {0x0216, 0x01},
    {0x0217, 0x00},
    {0x0218, 0x01},
    {0x0219, 0x00},
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x08},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x08},
    {0x023C, 0x00},
    {0x023E, 0x00},
    {0x023F, 0x00},
    {0x0258, 0x00},
    {0x0700, 0x00},
    {0x4004, 0x00},
    {0x4326, 0x03},

};

static struct msm_camera_i2c_reg_conf imx134_recommend_settings[] = {
	/* global setting 20120824*/
    {0x0220, 0x01},
    {0x0250, 0x0B},
    {0x3302, 0x11},
    {0x3833, 0x20},
    {0x3893, 0x01},
    {0x38C2, 0x08},
    {0x38C3, 0x08},
    {0x3906, 0x08},
    {0x3907, 0x01},
    {0x3C09, 0x01},
    {0x421C, 0x00},
    {0x421D, 0x8C},
    {0x421F, 0x02},
    {0x4500, 0x1F},
    {0x7006, 0x04},
    {0x3008, 0xB0},
    {0x320A, 0x01},
    {0x320D, 0x10},
    {0x3216, 0x2E},
    {0x3230, 0x0A},
    {0x3228, 0x05},
    {0x3229, 0x02},
    {0x322C, 0x02},
    {0x3390, 0x45},
    {0x3409, 0x0C},
    {0x340B, 0xF5},
    {0x340C, 0x2D},
    {0x3411, 0x18},
    {0x3412, 0x41},
    {0x3413, 0xAD},
    {0x3414, 0x21},
    {0x3427, 0x04},
    {0x3480, 0x21},
    {0x3484, 0x21},
    {0x3488, 0x21},
    {0x348C, 0x21},
    {0x3490, 0x21},
    {0x3494, 0x21},
    {0x349C, 0x38},
    {0x34A3, 0x38},
    {0x3511, 0x92},
    {0x3518, 0x00},
    {0x3519, 0x94},
    {0x364F, 0x00},

    /*B:20120924*/
    /* Image Quality adjustment setting */
    //Bypass Settings		
    {0x0700, 0x00},
    {0x4101, 0x00},
    {0x4203, 0x08},
    //Defect Correction Recommended Setting		
    {0x4100, 0xF8},
    {0x4102, 0x0B},
    //White Balance Gain for Color Artifact Correction		
    {0x0712, 0x01},
    {0x0713, 0x00},
    {0x0714, 0x01},
    {0x0715, 0x00},
    {0x0716, 0x01},
    {0x0717, 0x00},
    {0x0718, 0x01},
    {0x0719, 0x00},
    //Color Artifact Recommended Setting		
    {0x4240, 0x1E},
    {0x4241, 0x14},
    {0x4242, 0x0A},
    {0x4243, 0x96},
    {0x4244, 0x00},
    {0x433C, 0x01},
    {0x4345, 0x04},
    //RGB Filter Recommended Setting		
    {0x4281, 0x2A},
    {0x4282, 0x1E},
    {0x4284, 0x05},
    {0x4287, 0x1E},
    {0x4288, 0x05},
    {0x428B, 0x1E},
    {0x428C, 0x05},
    {0x428F, 0x14},
    {0x4297, 0x00},
    {0x4298, 0x1D},
    {0x4299, 0x1D},
    {0x429A, 0x13},
    {0x429C, 0x05},
    {0x429D, 0x05},
    {0x429E, 0x05},
    {0x42A0, 0x00},
    {0x42A1, 0x00},
    {0x42A2, 0x00},
    {0x42A4, 0x12},
    {0x42A5, 0x44},
    {0x42A6, 0x91},
    {0x42A7, 0x24},
    //DLC/ADP Recommended Setting		
    {0x4206, 0x66},
    {0x4207, 0x00},
    {0x4218, 0x02},
    {0x4219, 0x3F},
    {0x421B, 0x02},
    {0x4220, 0xFF},
    {0x4223, 0x44},
    {0x4224, 0x46},
    {0x4225, 0xF7},
    {0x4226, 0x14},
    {0x4227, 0xF2},
    {0x4228, 0x00},
    {0x4229, 0x64},
    {0x422A, 0xFC},
    {0x422B, 0xF9},
    {0x422C, 0xFA},
    {0x422D, 0x03},
    //HDR Setting		
    {0x4300, 0x00},
    {0x4316, 0x12},
    {0x4317, 0x22},
    {0x4318, 0x00},
    {0x4319, 0x00},
    {0x431A, 0x00},
    {0x4324, 0x03},
    {0x4325, 0x20},
    {0x4326, 0x03},
    {0x4327, 0x84},
    {0x4328, 0x03},
    {0x4329, 0x20},
    {0x432A, 0x03},
    {0x432B, 0x84},
    {0x432C, 0x01},
    {0x4401, 0x3F},
    {0x4402, 0xFF},
    {0x4412, 0x3F},
    {0x4413, 0xFF},
    {0x441D, 0x28},
    {0x4444, 0x00},
    {0x4445, 0x00},
    {0x4446, 0x3F},
    {0x4447, 0xFF},
/*S JackBB 2012/10/24 */
    {0x4452, 0x00},//Out_Noise high
    {0x4453, 0x80},//Out_Noise low
    {0x4454, 0x08},//Out_Mid high
    {0x4455, 0xFF},//Out_Mid low
    {0x4456, 0x06},//Out_Sat high
    {0x4457, 0xFF},//Out_Sat low
/*E JackBB 2012/10/24  */
    {0x4458, 0x18},
    {0x4459, 0x18},
    {0x445A, 0x3F},
    {0x445B, 0x3A},
    {0x4462, 0x00},
    {0x4463, 0x00},
    {0x4464, 0x00},
    {0x4465, 0x00},
    /*E:20120924*/
};

/*B:20120913 */
static struct msm_camera_i2c_reg_conf imx134_hdr_video_settings[] = {
  /* reg_B
        HDR  movie (4:3)
        H : 1640
        V : 1232  */
	/* PLL setting 20120824*/
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x01},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x3F},
    {0x0309, 0x05},
    {0x030B, 0x02},
    {0x030C, 0x00},
    {0x030D, 0x64},
    {0x030E, 0x01},
    {0x3A06, 0x12},
    {0x0101, 0x03},//2012/0919 Jiahan_Li
    {0x0105, 0x00},
    {0x0108, 0x03},
    {0x0109, 0x30},
    {0x010B, 0x32},
    {0x0112, 0x0E},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x00},
    {0x0391, 0x11},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4083, 0x01},
    {0x0340, 0x0B},
    {0x0341, 0x90},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x00},
    {0x0345, 0x00},
    {0x0346, 0x00},
    {0x0347, 0x00},
    {0x0348, 0x0C},
    {0x0349, 0xCF},
    {0x034A, 0x09},
    {0x034B, 0x9F},
    {0x034C, 0x06},
    {0x034D, 0x68},
    {0x034E, 0x04},
    {0x034F, 0xD0},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x06},
    {0x0355, 0x68},
    {0x0356, 0x04},
    {0x0357, 0xD0},
    {0x3310, 0x06},
    {0x3311, 0x68},
    {0x3312, 0x04},
    {0x3313, 0xD0},
    {0x331C, 0x00},
    {0x331D, 0x70},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    {0x0830, 0x67},
    {0x0831, 0x1F},
    {0x0832, 0x47},
    {0x0833, 0x1F},
    {0x0834, 0x1F},
    {0x0835, 0x17},
    {0x0836, 0x77},
    {0x0837, 0x27},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    {0x0202, 0x0A},
    {0x0203, 0x44},
    {0x0240, 0x0A},
    {0x0241, 0x44},
    {0x0254, 0x00},
    {0x0205, 0x33},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    {0x0216, 0x01},
    {0x0217, 0x00},
    {0x0218, 0x01},
    {0x0219, 0x00},
    {0x0230, 0x00},
    {0x0231, 0xA4},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
/*S:[bug856] Set AE_SAT and WB_LMT for HDR direct mode , Jim Lai 20121108 */
    {0x0238, 0x01},
    {0x0239, 0x08},
    {0x441E, 0x3B},
    {0x441F, 0xF0},
    {0x4446, 0x3B},
    {0x4447, 0xF0},
/*E:[bug856] Set AE_SAT and WB_LMT for HDR direct mode , Jim Lai 20121108 */
    {0x33B0, 0x06},
    {0x33B1, 0x68},
    {0x33B3, 0x02},
    {0x3800, 0x00}
};
/*E:20120913 */

static struct msm_camera_i2c_reg_conf imx134_60fps_settings[] = {
  /* reg_E-2
     "1/4HV
     *60fps"
     H : 820
     V : 616 */
	/* PLL setting 20120916*/
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x01},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x3F},
    {0x0309, 0x05},
    {0x030B, 0x02},
    {0x030C, 0x00},
    {0x030D, 0x32},
    {0x030E, 0x01},
    {0x3A06, 0x12},
    {0x0101, 0x03},//2012/09/19
    {0x0105, 0x00},
    {0x0108, 0x03},
    {0x0109, 0x30},
    {0x010B, 0x32},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x01},
    {0x0391, 0x22},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4083, 0x01},
    {0x0340, 0x02},
    {0x0341, 0xE4},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x03},
    {0x0345, 0x34},
    {0x0346, 0x02},
    {0x0347, 0x68},
    {0x0348, 0x09},
    {0x0349, 0x9B},
    {0x034A, 0x07},
    {0x034B, 0x37},
    {0x034C, 0x03},
    {0x034D, 0x34},
    {0x034E, 0x02},
    {0x034F, 0x68},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x03},
    {0x0355, 0x34},
    {0x0356, 0x02},
    {0x0357, 0x68},
    {0x3310, 0x03},
    {0x3311, 0x34},
    {0x3312, 0x02},
    {0x3313, 0x68},
    {0x331C, 0x00},
    {0x331D, 0x0A},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    {0x0830, 0x57},
    {0x0831, 0x0F},
    {0x0832, 0x2F},
    {0x0833, 0x17},
    {0x0834, 0x0F},
    {0x0835, 0x0F},
    {0x0836, 0x47},
    {0x0837, 0x27},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    {0x0202, 0x05},
    {0x0203, 0x20},
    {0x0240, 0x00},
    {0x0241, 0x00},
    {0x0254, 0x00},
    {0x0205, 0x33},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    {0x0216, 0x01},
    {0x0217, 0x00},
    {0x0218, 0x01},
    {0x0219, 0x00},
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x08},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},

};

static struct msm_camera_i2c_reg_conf imx134_120fps_settings[] = {
  /* reg_E
     "1/4HV
     *120fps"
     H : 820
     V : 616 */
	/* PLL setting 20120824*/
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x01},
    {0x0305, 0x03},
    {0x0306, 0x00},
    {0x0307, 0x3F},
    {0x0309, 0x05},
    {0x030B, 0x02},
    {0x030C, 0x00},
    {0x030D, 0x64},
    {0x030E, 0x01},
    {0x3A06, 0x12},
    {0x0101, 0x03},//2012/09/19
    {0x0105, 0x00},
    {0x0108, 0x03},
    {0x0109, 0x30},
    {0x010B, 0x32},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x01},
    {0x0391, 0x22},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4083, 0x01},
    {0x0340, 0x02},
    {0x0341, 0xE4},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x03},
    {0x0345, 0x34},
    {0x0346, 0x02},
    {0x0347, 0x68},
    {0x0348, 0x09},
    {0x0349, 0x9B},
    {0x034A, 0x07},
    {0x034B, 0x37},
    {0x034C, 0x03},
    {0x034D, 0x34},
    {0x034E, 0x02},
    {0x034F, 0x68},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x03},
    {0x0355, 0x34},
    {0x0356, 0x02},
    {0x0357, 0x68},
    {0x3310, 0x03},
    {0x3311, 0x34},
    {0x3312, 0x02},
    {0x3313, 0x68},
    {0x331C, 0x00},
    {0x331D, 0x0A},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    {0x0830, 0x67},
    {0x0831, 0x1F},
    {0x0832, 0x47},
    {0x0833, 0x1F},
    {0x0834, 0x1F},
    {0x0835, 0x17},
    {0x0836, 0x77},
    {0x0837, 0x27},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    {0x0202, 0x05},
    {0x0203, 0x20},
    {0x0240, 0x00},
    {0x0241, 0x00},
    {0x0254, 0x00},
    {0x0205, 0x33},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    {0x0216, 0x01},
    {0x0217, 0x00},
    {0x0218, 0x01},
    {0x0219, 0x00},
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x08},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},
};

/*B:20121103*/
static struct msm_camera_i2c_reg_conf imx134b_prev_settings[] = {
  /* 20121107
    reg_C
    1/2HV
    H : 1640
    V : 1232  */
    //Clock Setting
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0305, 0x0C},
    {0x0309, 0x05},
    {0x030B, 0x02},
    {0x030C, 0x01},
    {0x030D, 0x77},
    {0x030E, 0x01},
    {0x3A06, 0x12},
    //Mode setting
    {0x0108, 0x03},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x01},
    {0x0391, 0x22},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4082, 0x01},
    {0x4083, 0x01},
    {0x7006, 0x04},
    //OptionnalFunction setting		
    {0x0700, 0x00},
    {0x3A63, 0x00},
    {0x4100, 0xF8},
    {0x4203, 0xFF},
    {0x4344, 0x00},
    {0x441C, 0x01},
    //Size setting
    {0x0340, 0x0A},
    {0x0341, 0xD2},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x00},
    {0x0345, 0x00},
    {0x0346, 0x00},
    {0x0347, 0x00},
    {0x0348, 0x0C},
    {0x0349, 0xCF},
    {0x034A, 0x09},
    {0x034B, 0x9F},
    {0x034C, 0x06},
    {0x034D, 0x68},
    {0x034E, 0x04},
    {0x034F, 0xD0},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x06},
    {0x0355, 0x68},
    {0x0356, 0x04},
    {0x0357, 0xD0},
    {0x301D, 0x30},
    {0x3310, 0x06},
    {0x3311, 0x68},
    {0x3312, 0x04},
    {0x3313, 0xD0},
    {0x331C, 0x00},
    {0x331D, 0x10},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    //Global Timing Setting
    {0x0830, 0x5F},
    {0x0831, 0x1F},
    {0x0832, 0x3F},
    {0x0833, 0x1F},
    {0x0834, 0x1F},
    {0x0835, 0x17},
    {0x0836, 0x67},
    {0x0837, 0x27},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    //Integration Time Setting
    {0x0202, 0x0A},
    {0x0203, 0xCE},
    //Gain Setting
    {0x0205, 0x00},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    //HDR Setting
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x04},
    {0x023B, 0x00},
    {0x023C, 0x01},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},
    {0x33B4, 0x01},
    {0x3800, 0x00},
};

static struct msm_camera_i2c_reg_conf imx134b_snap_settings[] = {
  /* 20121107
   reg_A
   Full-resolution     *30fps
   H : 3280
   V : 2464 */
    //Clock Setting
    {0x011E, 0x18},
    {0x011F, 0x00},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0305, 0x0C},
    {0x0309, 0x05},
    {0x030B, 0x01},
    {0x030C, 0x01},
    {0x030D, 0x77},
    {0x030E, 0x01},
    {0x3A06, 0x11},
    //Mode setting
    {0x0108, 0x03},
    {0x0112, 0x0A},
    {0x0113, 0x0A},
    {0x0381, 0x01},
    {0x0383, 0x01},
    {0x0385, 0x01},
    {0x0387, 0x01},
    {0x0390, 0x00},
    {0x0391, 0x11},
    {0x0392, 0x00},
    {0x0401, 0x00},
    {0x0404, 0x00},
    {0x0405, 0x10},
    {0x4082, 0x01},
    {0x4083, 0x01},
    {0x7006, 0x04},
    //OptionnalFunction setting		
    {0x0700, 0x00},
    {0x3A63, 0x00},
    {0x4100, 0xF8},
    {0x4203, 0xFF},
    {0x4344, 0x00},
    {0x441C, 0x01},
    //Size setting
    {0x0340, 0x0A},
    {0x0341, 0xD2},
    {0x0342, 0x0E},
    {0x0343, 0x10},
    {0x0344, 0x00},
    {0x0345, 0x00},
    {0x0346, 0x00},
    {0x0347, 0x00},
    {0x0348, 0x0C},
    {0x0349, 0xCF},
    {0x034A, 0x09},
    {0x034B, 0x9F},
    {0x034C, 0x0C},
    {0x034D, 0xD0},
    {0x034E, 0x09},
    {0x034F, 0xA0},
    {0x0350, 0x00},
    {0x0351, 0x00},
    {0x0352, 0x00},
    {0x0353, 0x00},
    {0x0354, 0x0C},
    {0x0355, 0xD0},
    {0x0356, 0x09},
    {0x0357, 0xA0},
    {0x301D, 0x30},
    {0x3310, 0x0C},
    {0x3311, 0xD0},
    {0x3312, 0x09},
    {0x3313, 0xA0},
    {0x331C, 0x00},
    {0x331D, 0x10},
    {0x4084, 0x00},
    {0x4085, 0x00},
    {0x4086, 0x00},
    {0x4087, 0x00},
    {0x4400, 0x00},
    //Global Timing Setting
    {0x0830, 0x77},
    {0x0831, 0x2F},
    {0x0832, 0x5F},
    {0x0833, 0x37},
    {0x0834, 0x37},
    {0x0835, 0x37},
    {0x0836, 0xBF},
    {0x0837, 0x3F},
    {0x0839, 0x1F},
    {0x083A, 0x17},
    {0x083B, 0x02},
    //Integration Time Setting
    {0x0202, 0x0A},
    {0x0203, 0xCE},
    //Gain Setting
    {0x0205, 0x00},
    {0x020E, 0x01},
    {0x020F, 0x00},
    {0x0210, 0x01},
    {0x0211, 0x00},
    {0x0212, 0x01},
    {0x0213, 0x00},
    {0x0214, 0x01},
    {0x0215, 0x00},
    //HDR Setting
    {0x0230, 0x00},
    {0x0231, 0x00},
    {0x0233, 0x00},
    {0x0234, 0x00},
    {0x0235, 0x40},
    {0x0238, 0x00},
    {0x0239, 0x04},
    {0x023B, 0x00},
    {0x023C, 0x01},
    {0x33B0, 0x04},
    {0x33B1, 0x00},
    {0x33B3, 0x00},
    {0x33B4, 0x01},
    {0x3800, 0x00},
};

static struct msm_camera_i2c_reg_conf imx134b_recommend_settings[] = {
    /* global setting 20121107*/
    {0x0101, 0x03},//2012/09/19
    {0x0105, 0x01},
    {0x0110, 0x00},
    {0x0220, 0x01},
    {0x3302, 0x11},
    {0x3833, 0x20},
    {0x3893, 0x00},
    {0x3906, 0x08},
    {0x3907, 0x01},
    {0x391B, 0x01},
    {0x3C09, 0x01},
    {0x600A, 0x00},
    {0x3008, 0xB0},
    {0x320A, 0x01},
    {0x320D, 0x10},
    {0x3216, 0x2E},
    {0x322C, 0x02},
    {0x3409, 0x0C},
    {0x340C, 0x2D},
    {0x3411, 0x39},
    {0x3414, 0x1E},
    {0x3427, 0x04},
    {0x3480, 0x1E},
    {0x3484, 0x1E},
    {0x3488, 0x1E},
    {0x348C, 0x1E},
    {0x3490, 0x1E},
    {0x3494, 0x1E},
    {0x3511, 0x8F},
    {0x3617, 0x2D},
//    {0x3494, 0x21},
	/* Image Quality adjustment setting */
    //Defect Correction Recommended Setting
    {0x380A, 0x00},
    {0x380B, 0x00},
    {0x4103, 0x00},
    //Color Artifact Recommended Setting	
    {0x4243, 0x9A},
    {0x4330, 0x01},
    {0x4331, 0x90},
    {0x4332, 0x02},
    {0x4333, 0x58},
    {0x4334, 0x03},
    {0x4335, 0x20},
    {0x4336, 0x03},
    {0x4337, 0x84},
    {0x433C, 0x01},
    {0x4340, 0x02},
    {0x4341, 0x58},
    {0x4342, 0x03},
    {0x4343, 0x52},
    //Moire reduction Parameter Setting
    {0x4364, 0x0B},
    {0x4368, 0x00},
    {0x4369, 0x0F},
    {0x436A, 0x03},
    {0x436B, 0xA8},
    {0x436C, 0x00},
    {0x436D, 0x00},
    {0x436E, 0x00},
    {0x436F, 0x06},
    {0x4225, 0xF7},
    {0x4226, 0x14},
    //CNR parameter setting
    {0x4281, 0x21},
    {0x4282, 0x18},
    {0x4283, 0x04},
    {0x4284, 0x08},
    {0x4287, 0x7F},
    {0x4288, 0x08},
    {0x428B, 0x7F},
    {0x428C, 0x08},
    {0x428F, 0x7F},
    {0x4297, 0x00},
    {0x4298, 0x7E},
    {0x4299, 0x7E},
    {0x429A, 0x7E},
    {0x42A4, 0xFB},
    {0x42A5, 0x7E},
    {0x42A6, 0xDF},
    {0x42A7, 0xB7},
    {0x42AF, 0x03},
    //ARNR Parameter Setting    
    {0x4207, 0x03},
    {0x4216, 0x08},
    {0x4217, 0x08},
    //DLC/ADP Recommended Setting		
    {0x4218, 0x00},
    {0x421B, 0x20},
    {0x421F, 0x04},
    {0x4222, 0x02},
    {0x4223, 0x22},
    {0x422E, 0x54},
    {0x422F, 0xFB},
    {0x4230, 0xFF},
    {0x4231, 0xFE},
    {0x4232, 0xFF},
    {0x4235, 0x58},
    {0x4236, 0xF7},
    {0x4237, 0xFD},
    {0x4239, 0x4E},
    {0x423A, 0xFC},
    {0x423B, 0xFD},
    //HDR Setting		
    {0x4300, 0x00},
    {0x4316, 0x12},
    {0x4317, 0x22},
    {0x4318, 0x00},
    {0x4319, 0x00},
    {0x431A, 0x00},
    {0x4324, 0x03},
    {0x4325, 0x20},
    {0x4326, 0x03},
    {0x4327, 0x84},
    {0x4328, 0x03},
    {0x4329, 0x20},
    {0x432A, 0x03},
    {0x432B, 0x20},
    {0x432C, 0x01},
    {0x432D, 0x01},
    {0x4338, 0x02},
    {0x4339, 0x00},
    {0x433A, 0x00},
    {0x433B, 0x02},
    {0x435A, 0x03},
    {0x435B, 0x84},
    {0x435E, 0x01},
    {0x435F, 0xFF},
    {0x4360, 0x01},
    {0x4361, 0xF4},
    {0x4362, 0x03},
    {0x4363, 0x84},
    {0x437B, 0x01},
    {0x4401, 0x3F},
    {0x4402, 0xFF},
    {0x4404, 0x13},
    {0x4405, 0x26},
    {0x4406, 0x07},
    {0x4408, 0x20},
    {0x4409, 0xE5},
    {0x440A, 0xFB},
    {0x440C, 0xF6},
    {0x440D, 0xEA},
    {0x440E, 0x20},
    {0x4410, 0x00},
    {0x4411, 0x00},
    {0x4412, 0x3F},
    {0x4413, 0xFF},
    {0x4414, 0x1F},
    {0x4415, 0xFF},
    {0x4416, 0x20},
    {0x4417, 0x00},
    {0x4418, 0x1F},
    {0x4419, 0xFF},
    {0x441A, 0x20},
    {0x441B, 0x00},
    {0x441D, 0x40},
//    {0x441E, 0x0E},
//    {0x441F, 0xFC},
    {0x441E, 0x1E},
    {0x441F, 0x38},
    {0x4420, 0x01},
    {0x4444, 0x00},
    {0x4445, 0x00},
//    {0x4446, 0x0E},
//    {0x4447, 0xFC},
    {0x4446, 0x1D},
    {0x4447, 0xF9},
/*S JackBB 2012/10/24 */
    {0x4452, 0x00},//Out_Noise high
    {0x4453, 0x80},//Out_Noise low
    {0x4454, 0x08},//Out_Mid high
    {0x4455, 0x00},//Out_Mid low
    {0x4456, 0x06},//Out_Sat high
    {0x4457, 0x00},//Out_Sat low
/*E JackBB 2012/10/24 */
    {0x4458, 0x18},
    {0x4459, 0x18},
    {0x445A, 0x3F},
    {0x445B, 0x3A},
    {0x445C, 0x00},
    {0x445D, 0x28},
    {0x445E, 0x01},
    {0x445F, 0x90},
    {0x4460, 0x00},
    {0x4461, 0x60},
    {0x4462, 0x00},
    {0x4463, 0x00},
    {0x4464, 0x00},
    {0x4465, 0x00},
    {0x446C, 0x00},
    {0x446D, 0x00},
    {0x446E, 0x00},
    //LSC Setting
    {0x452A, 0x02},
    //White Balance Setting
    {0x0712, 0x01},
    {0x0713, 0x00},
    {0x0714, 0x01},
    {0x0715, 0x00},
    {0x0716, 0x01},
    {0x0717, 0x00},
    {0x0718, 0x01},
    {0x0719, 0x00},
    //Shading setting
    {0x4500, 0x1F},
};
static struct msm_camera_i2c_reg_conf imx134b_hdr_video_settings[] = {
  /* 20121017
     reg_B
     HDR  movie (4:3)
      H : 1632
      V : 1232 */
//Clock Setting                
//     Address value
       {0x011E, 0x18},
       {0x011F, 0x00},
       {0x0301, 0x05},
       {0x0303, 0x01},
       {0x0305, 0x0C},
       {0x0309, 0x05},
       {0x030B, 0x02},
       {0x030C, 0x01},
       {0x030D, 0x77},
       {0x030E, 0x01},
       {0x3A06, 0x12},
//Mode setting         
//     Address value
       {0x0108, 0x03},
       {0x0112, 0x0E},
       {0x0113, 0x0A},
       {0x0381, 0x01},
       {0x0383, 0x01},
       {0x0385, 0x01},
       {0x0387, 0x01},
       {0x0390, 0x00},
       {0x0391, 0x11},
       {0x0392, 0x00},
       {0x0401, 0x00},
       {0x0404, 0x00},
       {0x0405, 0x10},
       {0x4082, 0x01},
       {0x4083, 0x01},
       {0x7006, 0x04},
//OptionnalFunction setting            
//     Address value
       {0x0700, 0x00},
       {0x3A63, 0x00},
       {0x4100, 0xF8},
       {0x4203, 0xFF},
       {0x4344, 0x00},
       {0x441C, 0x01},
//Size setting         
//     Address value
       {0x0340, 0x0A},
       {0x0341, 0xD2},
       {0x0342, 0x0E},
       {0x0343, 0x10},
       {0x0344, 0x00},
       {0x0345, 0x08},
       {0x0346, 0x00},
       {0x0347, 0x00},
       {0x0348, 0x0C},
       {0x0349, 0xC7},
       {0x034A, 0x09},
       {0x034B, 0x9F},
       {0x034C, 0x06},
       {0x034D, 0x60},
       {0x034E, 0x04},
       {0x034F, 0xD0},
       {0x0350, 0x00},
       {0x0351, 0x00},
       {0x0352, 0x00},
       {0x0353, 0x00},
       {0x0354, 0x06},
       {0x0355, 0x60},
       {0x0356, 0x04},
       {0x0357, 0xD0},
       {0x301D, 0x30},
       {0x3310, 0x06},
       {0x3311, 0x60},
       {0x3312, 0x04},
       {0x3313, 0xD0},
       {0x331C, 0x03},
       {0x331D, 0x58},
       {0x4084, 0x00},
       {0x4085, 0x00},
       {0x4086, 0x00},
       {0x4087, 0x00},
       {0x4400, 0x00},
//Global Timing Setting                
//     Address value
       {0x0830, 0x5F},
       {0x0831, 0x1F},
       {0x0832, 0x3F},
       {0x0833, 0x1F},
       {0x0834, 0x1F},
       {0x0835, 0x17},
       {0x0836, 0x67},
       {0x0837, 0x27},
       {0x0839, 0x1F},
       {0x083A, 0x17},
       {0x083B, 0x02},
//Integration Time Setting             
//     Address value
       {0x0202, 0x0A},
       {0x0203, 0xCE},
//Gain Setting         
//     Address value
       {0x0205, 0x00},
       {0x020E, 0x01},
       {0x020F, 0x00},
       {0x0210, 0x01},
       {0x0211, 0x00},
       {0x0212, 0x01},
       {0x0213, 0x00},
       {0x0214, 0x01},
       {0x0215, 0x00},
//HDR Setting          
//     Address value
       {0x0230, 0x00},
       {0x0231, 0x00},
       {0x0233, 0x00},
       {0x0234, 0x00},
       {0x0235, 0x40},
       {0x0238, 0x01},//JackBB 2012/12/20
       {0x0239, 0x04},
       {0x023B, 0x03},
       {0x023C, 0x01},
       {0x33B0, 0x06},
       {0x33B1, 0x60},
       {0x33B3, 0x02},
       {0x33B4, 0x00},
       {0x3800, 0x00},
#if 1
/*S:[bug1099] LSC Setting for HDR video feature , JackBB 20120921 */
    {0x4800, 0x02},//R00
    {0x4801, 0x45},//R00
    {0x4802, 0x02},//GR00
    {0x4803, 0x30},//GR00
    {0x4804, 0x01},//R01
    {0x4805, 0xd3},//R01
    {0x4806, 0x01},//GR01
    {0x4807, 0xc3},//GR01
    {0x4808, 0x01},//R02
    {0x4809, 0x8d},//R02
    {0x480A, 0x01},//GR02
    {0x480B, 0x85},//GR02
    {0x480C, 0x01},//R03
    {0x480D, 0x6e},//R03
    {0x480E, 0x01},//GR03
    {0x480F, 0x6d},//GR03
    {0x4810, 0x01},//R04
    {0x4811, 0x93},//R04
    {0x4812, 0x01},//GR04
    {0x4813, 0x8f},//GR04
    {0x4814, 0x01},//R05
    {0x4815, 0xed},//R05
    {0x4816, 0x01},//GR05
    {0x4817, 0xda},//GR05
    {0x4818, 0x02},//R06
    {0x4819, 0x7a},//R06
    {0x481A, 0x02},//GR06
    {0x481B, 0x5d},//GR06
    {0x481C, 0x01},//R07
    {0x481D, 0xe6},//R07
    {0x481E, 0x01},//GR07
    {0x481F, 0xd9},//GR07
    {0x4820, 0x01},//R08
    {0x4821, 0x8c},//R08
    {0x4822, 0x01},//GR08
    {0x4823, 0x86},//GR08
    {0x4824, 0x01},//R09
    {0x4825, 0x46},//R09
    {0x4826, 0x01},//GR09
    {0x4827, 0x46},//GR09
    {0x4828, 0x01},//R10
    {0x4829, 0x25},//R10
    {0x482A, 0x01},//GR10
    {0x482B, 0x27},//GR10
    {0x482C, 0x01},//R11
    {0x482D, 0x4d},//R11
    {0x482E, 0x01},//GR11
    {0x482F, 0x4c},//GR11
    {0x4830, 0x01},//R12
    {0x4831, 0x9e},//R12
    {0x4832, 0x01},//GR12
    {0x4833, 0x96},//GR12
    {0x4834, 0x02},//R13
    {0x4835, 0x0c},//R13
    {0x4836, 0x01},//GR13
    {0x4837, 0xf9},//GR13
    {0x4838, 0x01},//R14
    {0x4839, 0xc8},//R14
    {0x483A, 0x01},//GR14
    {0x483B, 0xb8},//GR14
    {0x483C, 0x01},//R15
    {0x483D, 0x6f},//R15
    {0x483E, 0x01},//GR15
    {0x483F, 0x6a},//GR15
    {0x4840, 0x01},//R16
    {0x4841, 0x25},//R16
    {0x4842, 0x01},//GR16
    {0x4843, 0x24},//GR16
    {0x4844, 0x01},//R17
    {0x4845, 0x06},//R17
    {0x4846, 0x01},//GR17
    {0x4847, 0x05},//GR17
    {0x4848, 0x01},//R18
    {0x4849, 0x2c},//R18
    {0x484A, 0x01},//GR18
    {0x484B, 0x29},//GR18
    {0x484C, 0x01},//R19
    {0x484D, 0x7f},//R19
    {0x484E, 0x01},//GR19
    {0x484F, 0x76},//GR19
    {0x4850, 0x01},//R20
    {0x4851, 0xe5},//R20
    {0x4852, 0x01},//GR20
    {0x4853, 0xd3},//GR20
    {0x4854, 0x01},//R21
    {0x4855, 0xf2},//R21
    {0x4856, 0x01},//GR21
    {0x4857, 0xe1},//GR21
    {0x4858, 0x01},//R22
    {0x4859, 0x95},//R22
    {0x485A, 0x01},//GR22
    {0x485B, 0x8c},//GR22
    {0x485C, 0x01},//R23
    {0x485D, 0x4c},//R23
    {0x485E, 0x01},//GR23
    {0x485F, 0x48},//GR23
    {0x4860, 0x01},//R24
    {0x4861, 0x2d},//R24
    {0x4862, 0x01},//GR24
    {0x4863, 0x2a},//GR24
    {0x4864, 0x01},//R25
    {0x4865, 0x56},//R25
    {0x4866, 0x01},//GR25
    {0x4867, 0x4e},//GR25
    {0x4868, 0x01},//R26
    {0x4869, 0xa8},//R26
    {0x486A, 0x01},//GR26
    {0x486B, 0x98},//GR26
    {0x486C, 0x02},//R27
    {0x486D, 0x15},//R27
    {0x486E, 0x01},//GR27
    {0x486F, 0xfc},//GR27
    {0x4870, 0x02},//R28
    {0x4871, 0x7b},//R28
    {0x4872, 0x02},//GR28
    {0x4873, 0x53},//GR28
    {0x4874, 0x01},//R29
    {0x4875, 0xf3},//R29
    {0x4876, 0x01},//GR29
    {0x4877, 0xd8},//GR29
    {0x4878, 0x01},//R30
    {0x4879, 0xa0},//R30
    {0x487A, 0x01},//GR30
    {0x487B, 0x92},//GR30
    {0x487C, 0x01},//R31
    {0x487D, 0x81},//R31
    {0x487E, 0x01},//GR31
    {0x487F, 0x76},//GR31
    {0x4880, 0x01},//R32
    {0x4881, 0xa9},//R32
    {0x4882, 0x01},//GR32
    {0x4883, 0x98},//GR32
    {0x4884, 0x01},//R33
    {0x4885, 0xfe},//R33
    {0x4886, 0x01},//GR33
    {0x4887, 0xe7},//GR33
    {0x4888, 0x02},//R34
    {0x4889, 0x96},//R34
    {0x488A, 0x02},//GR34
    {0x488B, 0x76},//GR34
    {0x488C, 0x02},//GB00
    {0x488D, 0x31},//GB00
    {0x488E, 0x02},//B00
    {0x488F, 0x41},//B00
    {0x4890, 0x01},//GB01
    {0x4891, 0xc3},//GB01
    {0x4892, 0x01},//B01
    {0x4893, 0xca},//B01
    {0x4894, 0x01},//GB02
    {0x4895, 0x85},//GB02
    {0x4896, 0x01},//B02
    {0x4897, 0x88},//B02
    {0x4898, 0x01},//GB03
    {0x4899, 0x6d},//GB03
    {0x489A, 0x01},//B03
    {0x489B, 0x6d},//B03
    {0x489C, 0x01},//GB04
    {0x489D, 0x8e},//GB04
    {0x489E, 0x01},//B04
    {0x489F, 0x90},//B04
    {0x48A0, 0x01},//GB05
    {0x48A1, 0xd9},//GB05
    {0x48A2, 0x01},//B05
    {0x48A3, 0xdb},//B05
    {0x48A4, 0x02},//GB06
    {0x48A5, 0x5b},//GB06
    {0x48A6, 0x02},//B06
    {0x48A7, 0x6a},//B06
    {0x48A8, 0x01},//GB07
    {0x48A9, 0xd9},//GB07
    {0x48AA, 0x01},//B07
    {0x48AB, 0xe3},//B07
    {0x48AC, 0x01},//GB08
    {0x48AD, 0x86},//GB08
    {0x48AE, 0x01},//B08
    {0x48AF, 0x8a},//B08
    {0x48B0, 0x01},//GB09
    {0x48B1, 0x46},//GB09
    {0x48B2, 0x01},//B09
    {0x48B3, 0x46},//B09
    {0x48B4, 0x01},//GB10
    {0x48B5, 0x27},//GB10
    {0x48B6, 0x01},//B10
    {0x48B7, 0x27},//B10
    {0x48B8, 0x01},//GB11
    {0x48B9, 0x4c},//GB11
    {0x48BA, 0x01},//B11
    {0x48BB, 0x4c},//B11
    {0x48BC, 0x01},//GB12
    {0x48BD, 0x95},//GB12
    {0x48BE, 0x01},//B12
    {0x48BF, 0x96},//B12
    {0x48C0, 0x01},//GB13
    {0x48C1, 0xf7},//GB13
    {0x48C2, 0x01},//B13
    {0x48C3, 0xfb},//B13
    {0x48C4, 0x01},//GB14
    {0x48C5, 0xb9},//GB14
    {0x48C6, 0x01},//B14
    {0x48C7, 0xbc},//B14
    {0x48C8, 0x01},//GB15
    {0x48C9, 0x6b},//GB15
    {0x48CA, 0x01},//B15
    {0x48CB, 0x6b},//B15
    {0x48CC, 0x01},//GB16
    {0x48CD, 0x24},//GB16
    {0x48CE, 0x01},//B16
    {0x48CF, 0x23},//B16
    {0x48D0, 0x01},//GB17
    {0x48D1, 0x04},//GB17
    {0x48D2, 0x01},//B17
    {0x48D3, 0x04},//B17
    {0x48D4, 0x01},//GB18
    {0x48D5, 0x29},//GB18
    {0x48D6, 0x01},//B18
    {0x48D7, 0x2a},//B18
    {0x48D8, 0x01},//GB19
    {0x48D9, 0x75},//GB19
    {0x48DA, 0x01},//B19
    {0x48DB, 0x77},//B19
    {0x48DC, 0x01},//GB20
    {0x48DD, 0xd1},//GB20
    {0x48DE, 0x01},//B20
    {0x48DF, 0xd5},//B20
    {0x48E0, 0x01},//GB21
    {0x48E1, 0xe2},//GB21
    {0x48E2, 0x01},//B21
    {0x48E3, 0xeb},//B21
    {0x48E4, 0x01},//GB22
    {0x48E5, 0x8c},//GB22
    {0x48E6, 0x01},//B22
    {0x48E7, 0x8c},//B22
    {0x48E8, 0x01},//GB23
    {0x48E9, 0x49},//GB23
    {0x48EA, 0x01},//B23
    {0x48EB, 0x49},//B23
    {0x48EC, 0x01},//GB24
    {0x48ED, 0x29},//GB24
    {0x48EE, 0x01},//B24
    {0x48EF, 0x2b},//B24
    {0x48F0, 0x01},//GB25
    {0x48F1, 0x4e},//GB25
    {0x48F2, 0x01},//B25
    {0x48F3, 0x4e},//B25
    {0x48F4, 0x01},//GB26
    {0x48F5, 0x97},//GB26
    {0x48F6, 0x01},//B26
    {0x48F7, 0x9b},//B26
    {0x48F8, 0x01},//GB27
    {0x48F9, 0xfb},//GB27
    {0x48FA, 0x02},//B27
    {0x48FB, 0x02},//B27
    {0x48FC, 0x02},//GB28
    {0x48FD, 0x54},//GB28
    {0x48FE, 0x02},//B28
    {0x48FF, 0x5a},//B28
    {0x4900, 0x01},//GB29
    {0x4901, 0xd9},//GB29
    {0x4902, 0x01},//B29
    {0x4903, 0xd8},//B29
    {0x4904, 0x01},//GB30
    {0x4905, 0x92},//GB30
    {0x4906, 0x01},//B30
    {0x4907, 0x93},//B30
    {0x4908, 0x01},//GB31
    {0x4909, 0x76},//GB31
    {0x490A, 0x01},//B31
    {0x490B, 0x77},//B31
    {0x490C, 0x01},//GB32
    {0x490D, 0x97},//GB32
    {0x490E, 0x01},//B32
    {0x490F, 0x9a},//B32
    {0x4910, 0x01},//GB33
    {0x4911, 0xe7},//GB33
    {0x4912, 0x01},//B33
    {0x4913, 0xf1},//B33
    {0x4914, 0x02},//GB34
    {0x4915, 0x74},//GB34
    {0x4916, 0x02},//B34
    {0x4917, 0x84},//B34 

    {0x4500, 0x1F},//EN_LSC
    {0x0700, 0x01},//LSC Enable
    {0x3A63, 0x01},//RAM_SEL_Toggle
#endif
};
/*E:20121103*/

/*
static struct msm_camera_i2c_reg_conf imx134_LSCTable_settings[] = {

 };*/

static struct v4l2_subdev_info imx134_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx134_init_conf[] = {
	{&imx134_recommend_settings[0],
	ARRAY_SIZE(imx134_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
//S  JackBB 2012/12/3 [Q111M]
//   {&imx134_LSCTable_settings[0],
//   ARRAY_SIZE(imx134_LSCTable_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
//E  JackBB 2012/12/3 [Q111M]
};

static struct msm_camera_i2c_conf_array imx134_confs[] = {
	{&imx134_snap_settings[0],
	ARRAY_SIZE(imx134_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134_prev_settings[0],
	ARRAY_SIZE(imx134_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134_hdr_video_settings[0],
	ARRAY_SIZE(imx134_hdr_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134_60fps_settings[0],
	ARRAY_SIZE(imx134_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134_120fps_settings[0],
	ARRAY_SIZE(imx134_120fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

/*B:20121103*/
static struct msm_camera_i2c_conf_array imx134b_init_conf[] = {
	{&imx134b_recommend_settings[0],
	ARRAY_SIZE(imx134b_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
//S  JackBB 2012/12/3 [Q111M]
//   {&imx134_LSCTable_settings[0],
//   ARRAY_SIZE(imx134_LSCTable_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
//E  JackBB 2012/12/3 [Q111M]
};

static struct msm_camera_i2c_conf_array imx134b_confs[] = {
	{&imx134b_snap_settings[0],
	ARRAY_SIZE(imx134b_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134b_prev_settings[0],
	ARRAY_SIZE(imx134b_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&imx134b_hdr_video_settings[0],
	ARRAY_SIZE(imx134b_hdr_video_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};
/*E:20121103*/

static struct msm_sensor_output_info_t imx134_dimensions[] = {
	{
	/* full size - snapshot*/
		.x_output = 0x0CD0, /* 3280 */
		.y_output = 0x09A0, /* 2464 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0B90, /* 2960 */
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 preview */
		.x_output = 0x0668, /* 1640 */
		.y_output = 0x04D0, /* 1232 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0B90, /* 2960 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2  HDR  movie (4:3)*/
		.x_output = 0x0668, /* 1640 */
		.y_output = 0x04D0, /* 1232 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0B90, /* 2960 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 60fps */ 
		.x_output = 0x0334, /* 820 */
		.y_output = 0x0268, /* 616 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x02E4, /* 740 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 120fps */
		.x_output = 0x0334, /* 820 */
		.y_output = 0x0268, /* 616 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x02E4, /* 740 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
};

/*B:20121103*/
static struct msm_sensor_output_info_t imx134b_dimensions[] = {
	{
	/* full size - snapshot*/
		.x_output = 0x0CD0, /* 3280 */
		.y_output = 0x09A0, /* 2464 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0AD2, /* 2770 */
		.vt_pixel_clk = 300000000,
		.op_pixel_clk = 300000000,
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 preview */
		.x_output = 0x0668, /* 1640 */
		.y_output = 0x04D0, /* 1232 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0AD2, /* 2770 */
		/*B:20120821*/
		.vt_pixel_clk = 300000000,
		.op_pixel_clk = 300000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2  HDR  movie (4:3)*/
		.x_output = 0x0660, /* 1632 */
		.y_output = 0x04D0, /* 1232 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x0AD2, /* 2770 */
		/*B:20120821*/
		.vt_pixel_clk = 300000000,
		.op_pixel_clk = 300000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	// No need support following 2 modes
	{
	/* 30 fps 1/2 * 1/2 60fps */ 
		.x_output = 0x0334, /* 820 */
		.y_output = 0x0268, /* 616 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x02E4, /* 740 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
	{
	/* 30 fps 1/2 * 1/2 120fps */
		.x_output = 0x0334, /* 820 */
		.y_output = 0x0268, /* 616 */
		.line_length_pclk = 0x0E10, /* 3600 */
		.frame_length_lines = 0x02E4, /* 740 */
		/*B:20120821*/
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,//20121221
		/*E:20120821*/
		.binning_factor = 1,
	},
};
/*E:20121103*/

#if 0 //remove to user space for bsp1744s
static struct msm_camera_csid_vc_cfg imx134_cid_cfg[] = {
       {0, CSI_RAW10, CSI_DECODE_10BIT},
       {1, 0x35, CSI_DECODE_10BIT},
       {2, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx134_csi_params = {
       .csid_params = {
               .lane_assign = 0xe4,
               .lane_cnt = 4,
               .lut_params = {
                       .num_cid = ARRAY_SIZE(imx134_cid_cfg),
                       .vc_cfg = imx134_cid_cfg,
               },
       },
       .csiphy_params = {
               .lane_cnt = 4,
               .settle_cnt = 0x12, // changed frim 0x12 to 0x1B
       },
};

static struct msm_camera_csi2_params *imx134_csi_params_array[] = {
	&imx134_csi_params,
	&imx134_csi_params,
	&imx134_csi_params,
	&imx134_csi_params,
	&imx134_csi_params,
};
#endif

static struct msm_sensor_output_reg_addr_t imx134_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t imx134_id_info = {
	.sensor_id_reg_addr = 0x0016,
	.sensor_id = 0x134,// come from 134 sample
};
/*B:20121103*/
static struct msm_sensor_id_info_t imx134b_id_info = {
	.sensor_id_reg_addr = 0x3B2C,
	.sensor_id = 0x00,// 0x80 ,0xA0 for imx134 bayer sensor, 0x00 for RGBW sensor
};
/*E:20121103*/

static struct msm_sensor_exp_gain_info_t imx134_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0205,
	.vert_offset = 4,
};

static const struct i2c_device_id imx134_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx134_s_ctrl},
	{ }
};

static struct i2c_driver imx134_i2c_driver = {
	.id_table = imx134_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx134_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

/*S JackBB 2012/11/13  */
#define PAGE_NUMBER 35
static int imx134_write_proc(struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	int *buf;
    int i,addr;

    //printk(KERN_NOTICE "imx134_write_proc(%d)",(int)count);
	if (count < 1)
		return -EINVAL;


    buf = (int *)buffer;

    i=0;
	msm_camera_i2c_write_tbl(
		&imx134_sensor_i2c_client,
		imx134_groupon_settings,
		ARRAY_SIZE(imx134_groupon_settings),
		MSM_CAMERA_I2C_BYTE_DATA);

    addr = 0x4800;

    for(i = 0;i < PAGE_NUMBER;i++)
    {
        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i] >> 8);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i] >> 8, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;
        
        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i] & 0x00FF);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i] & 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;

        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER] >> 8);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER] >> 8, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;

        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER] & 0x00FF);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER] & 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;
    }

    for(i = 0;i < PAGE_NUMBER;i++)
    {
        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER*2] >> 8);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER*2] >> 8, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;
        
        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER*2] & 0x00FF);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER*2] & 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;

        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER*3] >> 8);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER*3] >> 8, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;

        //printk(KERN_NOTICE "imx134_write_proc [%x]=%x",addr,buf[i+PAGE_NUMBER*3] & 0x00FF);
	    msm_camera_i2c_write(&imx134_sensor_i2c_client,addr, buf[i+PAGE_NUMBER*3] & 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
        addr++;
    }

    //printk(KERN_NOTICE "imx134_write_proc addr = %x",addr);

    msm_camera_i2c_write(&imx134_sensor_i2c_client,0x3A63, 0x01, MSM_CAMERA_I2C_BYTE_DATA);//RAM_SEL_Toggle

	msm_camera_i2c_write_tbl(
		&imx134_sensor_i2c_client,
		imx134_groupoff_settings,
		ARRAY_SIZE(imx134_groupoff_settings),
		MSM_CAMERA_I2C_BYTE_DATA);

	return count;
}

static int imx134_create_proc(void)
{
	struct proc_dir_entry *proc_gpio;
	
	//return -1;//disable 

	proc_gpio = create_proc_entry("hw_lsc_setting", 666, NULL);
	if (proc_gpio){
		proc_gpio->write_proc = imx134_write_proc;
		return 0;
	}

	return -1;
}
/*E JackBB 2012/11/13 */


static int __init imx134_sensor_init_module(void)
{
	CDBG("imx134\n");

    imx134_create_proc();

	return i2c_add_driver(&imx134_i2c_driver);
}

/*S Jim Lai 20120925 */
int32_t imx134_sensor_write_wb_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t r_gain, uint16_t g_gain, uint16_t b_gain)
{
	// pr_err("%s: %d, r=%x g=%x b=%x\n", __func__, __LINE__, r_gain, g_gain, b_gain);
	
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0712, g_gain >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0713, g_gain | 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0714, r_gain >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0715, r_gain | 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0716, b_gain >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0717, b_gain | 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0718, g_gain >> 8, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0719, g_gain | 0x00FF, MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}
/*E: Jim Lai 20120925 */

/*S: Jim Lai 20121019 */
int32_t imx134_sensor_get_stats_data(struct msm_sensor_ctrl_t *s_ctrl,
		struct stats_data_t *stats_data)
{
	int rc = 0, i = 0;
	for (i = 0; i < MAX_IMX134_AE_REGION/2; i++) {
		rc = msm_camera_i2c_read_seq(
			s_ctrl->sensor_i2c_client, 0x5000+(i*8), &stats_data->imx134_ae_stats[i*2], 2);
//		pr_err("[%d]=%X %X\n", i*2, stats_data->imx134_ae_stats[i*2], stats_data->imx134_ae_stats[(i*2)+1]);
	}

	return rc;
}
/*E: Jim Lai 20121019 */


/*S JackBB 2012/10/24 */
int32_t imx134_sensor_write_atr_control(struct msm_sensor_ctrl_t *s_ctrl,uint16_t controlvar)
{
	//pr_err("%s: ctrl=%d\n", __func__, controlvar);
	
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
    if(controlvar == 2)
    {
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x446D, 0, MSM_CAMERA_I2C_BYTE_DATA);
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x446E, 0, MSM_CAMERA_I2C_BYTE_DATA);
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x446C, 1, MSM_CAMERA_I2C_BYTE_DATA);
    }
    else if(controlvar == 1)
    {
    	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x446C, 0, MSM_CAMERA_I2C_BYTE_DATA);
    }
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}
/*E JackBB 2012/10/24 */
/*S:Jim Lai 20121108 */
int32_t imx134_sensor_write_exp_gain1(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines;
	uint8_t offset;

	uint16_t short_exp_gain;
	uint32_t short_exp_line;
	
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_BYTE_DATA);

//	pr_err("%s: %d, gain=%d line=%d res=%d\n", __func__, __LINE__, gain, line, s_ctrl->curr_res);
	if (s_ctrl->curr_res == 2)
	{
		
		if (gain > 16)
		{
			short_exp_gain = gain/16;
			short_exp_line = line;
		}
		else
		{
			short_exp_gain = 1;
			short_exp_line = line/16;
		}
		
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0230, short_exp_line,
			MSM_CAMERA_I2C_WORD_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0233, short_exp_gain,
			MSM_CAMERA_I2C_BYTE_DATA);
	}

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}
/*E: Jim Lai 20121108 */
static struct v4l2_subdev_core_ops imx134_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops imx134_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx134_subdev_ops = {
	.core = &imx134_subdev_core_ops,
	.video  = &imx134_subdev_video_ops,
};

/*B:20121103*/
static struct msm_sensor_reg_t imx134b_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx134_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx134_start_settings),
	.stop_stream_conf = imx134_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx134_stop_settings),
	.group_hold_on_conf = imx134_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx134_groupon_settings),
	.group_hold_off_conf = imx134_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx134_groupoff_settings),
	.init_settings = &imx134b_init_conf[0],
	.init_size = ARRAY_SIZE(imx134b_init_conf),
	.mode_settings = &imx134b_confs[0],
	.output_settings = &imx134b_dimensions[0],
	.num_conf = ARRAY_SIZE(imx134b_confs),
	//B 2012/12/19
	.flash_settings = imx134_flash_settings,
	.flash_settings_size = ARRAY_SIZE(imx134_flash_settings),
    //E 2012/12/19
};

int32_t imx134_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	uint16_t bayerid = 0;
    int nloop = 0;
    uint16_t otp = 0;
    pr_err("%s: E", __func__);
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	pr_err("imx134_sensor_match_id id: 0x%x\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("imx134_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
    // check sensor type , bayer or GRBW
    rc = msm_camera_i2c_write(
        s_ctrl->sensor_i2c_client,
        0x3B02, 0,
        MSM_CAMERA_I2C_BYTE_DATA);
    if (rc < 0) {
        pr_err("%s: write OTP 0x3B02 failed\n", __func__);
        return rc;
    }

    rc = msm_camera_i2c_write(
        s_ctrl->sensor_i2c_client,
        0x3B00, 1,
        MSM_CAMERA_I2C_BYTE_DATA);
    if (rc < 0) {
        pr_err("%s: write OTP 0x3B00 failed\n", __func__);
        return rc;
    }
    msleep(1);
	//usleep_range(10, 50);

    for (nloop=0; nloop < 3; nloop++)
    {
        rc = msm_camera_i2c_read(
            s_ctrl->sensor_i2c_client,
            0x3B01, &otp,
            MSM_CAMERA_I2C_BYTE_DATA);
        if (rc < 0) {
            pr_err("%s: %s: read OTP bayer id failed (%d)\n", __func__,
            s_ctrl->sensordata->sensor_name, nloop);
            return rc;
        }
        else
        {
            if (otp == 0x01)
                break;
        }
        msleep(1);
    }

    rc = msm_camera_i2c_read(
        s_ctrl->sensor_i2c_client,
        imx134b_id_info.sensor_id_reg_addr, &bayerid,
        MSM_CAMERA_I2C_BYTE_DATA);
    if (rc < 0) {
        pr_err("%s: %s: read OTP bayer id failed\n", __func__,
        s_ctrl->sensordata->sensor_name);
        return rc;
    }

    pr_err("OTP bayer id: 0x%x\n", bayerid);
    if (bayerid == imx134b_id_info.sensor_id) {
        //0x00 for RGBW
        pr_err("This is IMX134 RGBWhite sensor\n");
    }
    else
    {
        //0xA0, 0x80 for bayer
        pr_err("This is IMX134 Bayer sensor\n");
        s_ctrl->msm_sensor_reg = &imx134b_regs;
    }
    return rc;
}
/*E:20121103*/

// Luke 0701
int32_t imx134_sensor_write_digital_gain(struct msm_sensor_ctrl_t *s_ctrl, uint16_t upper_byte, uint16_t lower_byte);
int32_t imx134_sensor_write_digital_gain(struct msm_sensor_ctrl_t *s_ctrl, uint16_t upper_byte, uint16_t lower_byte)
{
           
           s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
           msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x020E, upper_byte << 8 | lower_byte, MSM_CAMERA_I2C_WORD_DATA);
           msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0210, upper_byte << 8 | lower_byte, MSM_CAMERA_I2C_WORD_DATA);
           msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0212, upper_byte << 8 | lower_byte, MSM_CAMERA_I2C_WORD_DATA);
           msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x0214, upper_byte << 8 | lower_byte, MSM_CAMERA_I2C_WORD_DATA);
           s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
           return 0;
}


//S  JackBB 2012/12/3 [Q111M]
int32_t imx134_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
               uint16_t gain, uint32_t line, int32_t luma_info, uint16_t fgain)
{
        uint32_t fl_lines;
        uint8_t offset;
       uint16_t shortshutter_gain = 0;
       uint32_t shortshutter = 0;
       uint16_t shortshutter_expratio = 8;

       uint16_t atr_out_noise = 0;
       uint16_t atr_out_mid = 0;
       //float tc_gain;
       static uint16_t deadzone_tolerence = 0;

       //uint32_t atr_threshold = 50;
       //uint32_t min_atr_threshold = 5;//Jackbb 2012/12/20

       int32_t luma_avg, AvgYmaxStat, AvgYminStat;
       uint16_t debug_luma_avg, debug_luma_min, debug_luma_max;
       uint8_t debug_luma_select;
       // Luke 0701 -->
       uint16_t digital_gain, digital_gain_mod;

       digital_gain = 1;
       digital_gain_mod = 0;
	if( gain > 224 )
	{
	    if( gain < 256 )
	    {
	       digital_gain = 1;
		digital_gain_mod =  256 - gain;
	    }
	    else
	    {
	       digital_gain = gain/256;
	       digital_gain_mod = gain%256;
	    }
	    gain = 224;
	}
       // Luke 0701 <--   
       luma_avg = luma_info & 0xFF;
       AvgYminStat = (luma_info & 0x7FF00) >> 8;
       AvgYmaxStat = (luma_info & 0x7FF00000) >> 20;

        fl_lines = s_ctrl->curr_frame_length_lines;
        fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
        offset = s_ctrl->sensor_exp_gain_info->vert_offset;
#if 0 //20130221
        if (line <= 24)
           line = 24;
#endif
        if (line > (fl_lines - offset))
                fl_lines = line + offset;

       //pr_info("imx134_write_exp_gain: line: %d gain: %d luma_avg:%d",line,gain,luma_avg);
       //pr_info("lokesh: imx134_write_exp_gain: luma_avg: %d YminStat: %d YMaxStat: %d",luma_avg,AvgYminStat,AvgYmaxStat);
        s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
                MSM_CAMERA_I2C_WORD_DATA);
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
                MSM_CAMERA_I2C_WORD_DATA);
        msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
                MSM_CAMERA_I2C_BYTE_DATA);


/* For video HDR mode */
       if (s_ctrl->curr_res == MSM_SENSOR_RES_2) {

  #if 1
         if (((AvgYmaxStat - luma_avg) < Y_STATS_DIFF_THRESHOLD) &&
                 (AvgYmaxStat < Y_OVER_EXP_THRESHOLD)) {

                 debug_luma_avg = luma_avg << 2;
                 debug_luma_min = ((AvgYminStat << 2) < FRAME_MINIMUM_INPUT) ? (AvgYminStat << 2):
                         FRAME_MINIMUM_INPUT;
                 debug_luma_max = ((AvgYmaxStat << 2) > 959) ? (AvgYmaxStat << 2) : 959;
                 debug_luma_select = 1;
                 
                 if (debug_luma_min < 24)
                         debug_luma_min = 24;

                 if(debug_luma_avg <= debug_luma_min)
                         debug_luma_avg = debug_luma_min + 1;

                 //CDBG("debug_luma_avg=%d,debug_luma_min=%d,debug_luma_max=%d,debug_luma_select=%d"
                    //,debug_luma_avg,debug_luma_min,debug_luma_max,debug_luma_select);
                  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       DEBUG_AVG_BYTE_ADDR, debug_luma_avg,
                       MSM_CAMERA_I2C_WORD_DATA);
                 
                  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       DEBUG_MIN_BYTE_ADDR, debug_luma_min,
                       MSM_CAMERA_I2C_WORD_DATA);
                 
                  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       DEBUG_MAX_BYTE_ADDR, debug_luma_max,
                       MSM_CAMERA_I2C_WORD_DATA);

                  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       DEBUG_SEL_BYTE_ADDR, debug_luma_select,
                       MSM_CAMERA_I2C_BYTE_DATA);
         } else {
                 debug_luma_select = 0;
                  msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       DEBUG_SEL_BYTE_ADDR, debug_luma_select,
                       MSM_CAMERA_I2C_BYTE_DATA);
         }
  #endif
              if(fgain < 256)
              {
                fgain = 256;
              }

              if(fgain > 2048)
              {
                fgain = 2048;
              }

              shortshutter = (line * fgain) / (Q8 * shortshutter_expratio);

              
              if(shortshutter > line)
              {
                int shortfgain;
                shortshutter = line;
                shortfgain = fgain / shortshutter_expratio;
                shortshutter_gain = (uint16_t)(256 - 256 * Q8 / shortfgain );
              }

              /* Short shutter update */
              msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                     SHORT_GAIN_BYTE_ADDR, shortshutter_gain,
                     MSM_CAMERA_I2C_BYTE_DATA);

              //pr_info("lokesh: longtshutter =%d, shortshutter=%d, longgain =%d\n",line, shortshutter, gain);
              msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                     SHORT_SHUTTER_WORD_ADDR, shortshutter,
                     MSM_CAMERA_I2C_WORD_DATA);

              /* Adaptive tone curve parameters update */
              if (luma_avg < THRESHOLD_DEAD_ZONE + deadzone_tolerence) {
                    //CDBG("lokesh: am in Deadzone tol: %d --> 5",deadzone_tolerence);//bbtest
                    deadzone_tolerence = 2;
                     /* change to fixed tone curve */
                     msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                            TC_SWITCH_BYTE_ADDR, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
              } else {
//pr_err("lokesh: am Out of Deadzone tol: %d --> 0",deadzone_tolerence);
                    deadzone_tolerence = 0;
                     /* change to adaptive tone curve */
                     msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                            TC_SWITCH_BYTE_ADDR, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
// Luke 0701 -->
#if 0
                    /* if (luma_avg < THRESHOLD_0) {  lokesh: Here assuming th0 == th_deadzone
                            atr_out_noise = 0;
                            atr_out_mid = 0;
                     } else*/ if (luma_avg < THRESHOLD_1) {
                            tc_gain = ((luma_avg - THRESHOLD_0)/(THRESHOLD_1 - THRESHOLD_0) * (1 - INIT_ATR_GAIN) + INIT_ATR_GAIN);
                            atr_out_noise = INIT_ATR_OUT_NOISE * tc_gain;
                            atr_out_mid = INIT_ATR_OUT_MID * tc_gain;
                     } else {
                            atr_out_noise = INIT_ATR_OUT_NOISE;
                            atr_out_mid = INIT_ATR_OUT_MID;
                     }
                     //atr_out_noise += ATR_OFFSET;
                     //atr_out_mid += ATR_OFFSET;
#else					 
                    if(luma_avg >= 100 ) {
                            atr_out_noise = INIT_ATR_OUT_NOISE;
                            atr_out_mid = INIT_ATR_OUT_MID;
                     }
			else
			{
                            atr_out_noise = INIT_ATR_OUT_NOISE * luma_avg / 100;
                            atr_out_mid = INIT_ATR_OUT_MID *  luma_avg / 100;
			}
#endif
// Luke 0701 <--			 
                     //CDBG("atr_out_noise=%d,atr_out_mid=%d",atr_out_noise,atr_out_mid);//bbtest
                     msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                            TC_OUT_NOISE_WORD_ADDR, atr_out_noise,
                            MSM_CAMERA_I2C_WORD_DATA);
                     msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                            TC_OUT_MID_WORD_ADDR, atr_out_mid,
                     MSM_CAMERA_I2C_WORD_DATA);
                     msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                            TC_OUT_SAT_WORD_ADDR, INIT_ATR_OUT_SAT,
                            MSM_CAMERA_I2C_WORD_DATA);
             }    
       }


#if 0 
       /* temp: short exp gain for HDR */
       if (s_ctrl->curr_res == MSM_SENSOR_RES_2) {
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       SHORT_GAIN_BYTE_ADDR, shortshutter_gain,
                       MSM_CAMERA_I2C_BYTE_DATA);

               shortshutter = (line * fgain) / (Q8 * shortshutter_expratio);
               pr_err("longtshutter =%d, shortshutter=%d, longgain =%d\n", line, shortshutter, gain);
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       SHORT_SHUTTER_WORD_ADDR, shortshutter, MSM_CAMERA_I2C_WORD_DATA);
			//S Jackbb 2012/12/20
         if (luma_avg > min_atr_threshold) {
			//E Jackbb 2012/12/20
             uint32_t atr_offset = 0;
             uint16_t atr_out_noise = 0xA0;
             uint16_t atr_out_mid = 0x800;
             uint16_t atr_noise_brate = 0;
             uint16_t atr_mid_brate = 0;

				pr_err("open ATR");
             msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                   TC_SWITCH_REGADDR, 0x00, MSM_CAMERA_I2C_BYTE_DATA);


             if (luma_avg < atr_threshold) {
                     atr_out_noise = ((0xA0 * luma_avg) / atr_threshold) + atr_offset;
                     atr_out_mid = ((0x800 * luma_avg) / atr_threshold) + atr_offset;
                   } else {
                           atr_out_noise = 0xA0 + atr_offset;
                           atr_out_mid = 0x800 + atr_offset;
                   }
                   atr_noise_brate = 0x18 * 1 + atr_offset;
                   atr_mid_brate = 0x18 * 1 + atr_offset;

                   msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                           TC_OUT_NOISE_REGADDR, atr_out_noise, MSM_CAMERA_I2C_WORD_DATA);
                   msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                           TC_OUT_MID_REGADDR, atr_out_mid, MSM_CAMERA_I2C_WORD_DATA);
                   msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                           TC_NOISE_BRATE_REGADDR, atr_noise_brate, MSM_CAMERA_I2C_BYTE_DATA);
                   msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                           TC_MID_BRATE_REGADDR, atr_mid_brate, MSM_CAMERA_I2C_BYTE_DATA);
         }
		//S Jackbb 2012/12/20
         else 
         {
               pr_err("close ATR");
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       TC_SWITCH_REGADDR, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
         }
			//E Jackbb 2012/12/20
       }
#endif
        s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

        // Luke 0701 
        imx134_sensor_write_digital_gain(s_ctrl, digital_gain, digital_gain_mod);

        return 0;
}

int32_t imx134_hdr_update(struct msm_sensor_ctrl_t *s_ctrl,
               struct sensor_hdr_update_parm_t *update_parm)
{
       //int32_t i;

       switch(update_parm->type) {
       case SENSOR_HDR_UPDATE_AWB:
               //CDBG("%s: SENSOR_HDR_UPDATE_AWB\n",__func__);
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       ABS_GAIN_R_WORD_ADDR, update_parm->awb_gain_r,
                       MSM_CAMERA_I2C_WORD_DATA);
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       ABS_GAIN_B_WORD_ADDR, update_parm->awb_gain_b,
                       MSM_CAMERA_I2C_WORD_DATA);
               //CDBG("%s: awb gains updated r=0x%x, b=0x%x\n", __func__,
                 //      update_parm->awb_gain_r,
                   //    update_parm->awb_gain_b);
               break;

       case SENSOR_HDR_UPDATE_LSC:
               //CDBG("%s: SENSOR_HDR_UPDATE_LSC\n",__func__);
               /* step1: write knot points to LSC table */
#if 1
               msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
                       LSC_TABLE_START_ADDR, &(update_parm->lsc_table[0]),
                       LSC_TABLE_LEN_BYTES);
#else
               for (i=0; i<LSC_TABLE_LEN_BYTES; i++) {
                       CDBG("lsc[%d] = %x\n", i, update_parm->lsc_table[i]);
                       msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                               LSC_TABLE_START_ADDR + i, update_parm->lsc_table[i],
                               MSM_CAMERA_I2C_BYTE_DATA);
               }
#endif
               /* step2: LSC enable */
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       EN_LSC_BYTE_ADDR, 0x1F, MSM_CAMERA_I2C_BYTE_DATA);
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       LSC_ENABLE_BYTE_ADDR, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
               /* step3: RAM_SEL_TOGGLE */
               msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
                       RAM_SEL_TOGGLE_BYTE_ADDR, 0x01, MSM_CAMERA_I2C_BYTE_DATA);

               //CDBG("%s: lsc table updated\n", __func__);
               break;
                
       default:
               pr_err("%s: invalid HDR update type %d\n",
                        __func__, update_parm->type);
               return -EINVAL;
       }
       return 0;
}
//E  JackBB 2012/12/3 [Q111M]

static struct msm_sensor_fn_t imx134_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
/*S:  Jim Lai 20121108 */
	.sensor_write_exp_gain = imx134_write_exp_gain,
	.sensor_write_snapshot_exp_gain = imx134_write_exp_gain,
/*E:  Jim Lai 20121108 */
/*S:  Jim Lai 20120925 */
	.sensor_write_wb_gain = imx134_sensor_write_wb_gain,
/*E:  Jim Lai 20120925 */
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	/*S:Jim Lai 20121019 */
	.sensor_get_stats_data = imx134_sensor_get_stats_data,
	/*E:Jim Lai 20121019 */
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
    /*S  JackBB 2012/10/24  */
    .sensor_write_atr_control = imx134_sensor_write_atr_control,
    /*E  JackBB 2012/10/24 */
    //S  JackBB 2012/12/3 [Q111M]
    .sensor_hdr_update = imx134_hdr_update,
    //E  JackBB 2012/12/3 [Q111M]
    /*B:20121103*/
    .sensor_match_id = imx134_sensor_match_id,
    /*E:20121103*/
};

static struct msm_sensor_reg_t imx134_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx134_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx134_start_settings),
	.stop_stream_conf = imx134_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx134_stop_settings),
	.group_hold_on_conf = imx134_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx134_groupon_settings),
	.group_hold_off_conf = imx134_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx134_groupoff_settings),
	.init_settings = &imx134_init_conf[0],
	.init_size = ARRAY_SIZE(imx134_init_conf),
	.mode_settings = &imx134_confs[0],
	.output_settings = &imx134_dimensions[0],
	.num_conf = ARRAY_SIZE(imx134_confs),
        //B 2012/11/06
	.flash_settings = imx134_flash_settings,
	.flash_settings_size = ARRAY_SIZE(imx134_flash_settings),
        //E 2012/11/06
};

static struct msm_sensor_ctrl_t imx134_s_ctrl = {
	.msm_sensor_reg = &imx134_regs,
	.sensor_i2c_client = &imx134_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &imx134_reg_addr,
	.sensor_id_info = &imx134_id_info,
	.sensor_exp_gain_info = &imx134_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	//.csi_params = &imx134_csi_params_array[0],
	.msm_sensor_mutex = &imx134_mut,
	.sensor_i2c_driver = &imx134_i2c_driver,
	.sensor_v4l2_subdev_info = imx134_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx134_subdev_info),
	.sensor_v4l2_subdev_ops = &imx134_subdev_ops,
	.func_tbl = &imx134_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(imx134_sensor_init_module);
MODULE_DESCRIPTION("SONY 8MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
