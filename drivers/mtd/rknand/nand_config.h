/********************************************************************************
*********************************************************************************
			COPYRIGHT (c)   2004 BY ROCK-CHIP FUZHOU
				--  ALL RIGHTS RESERVED  --

File Name:  nand_config.h
Author:     RK28XX Driver Develop Group
Created:    25th OCT 2008
Modified:
Revision:   1.00
********************************************************************************
********************************************************************************/
#ifndef     _NAND_CONFIG_H
#define     _NAND_CONFIG_H
#define     DRIVERS_NAND
#define     LINUX

#include    <linux/kernel.h>
#include    <linux/string.h>
#include    <linux/sched.h>
#include    <linux/delay.h>
//#include 	<asm/arch-rockchip/hardware.h>
#include    "typedef.h"
#include    <mach/rk29_iomap.h>
#include    <mach/iomux.h>
//#include    "epphal.h"
#ifndef 	TRUE
#define 	TRUE    1
#endif

#ifndef	 	FALSE
#define 	FALSE   0
#endif

#ifndef	 	NULL
#define 	NULL   (void*)0
#endif

#include    "FTL_OSDepend.h"
#include    "flash.h"
#include    "ftl.h"

#ifdef CONFIG_MTD_NAND_RK29XX_DEBUG
#undef RKNAND_DEBUG
#define RKNAND_DEBUG(format, arg...) \
		printk(KERN_NOTICE format, ## arg);
#else
#undef RKNAND_DEBUG
#define RKNAND_DEBUG(n, arg...)
#endif

extern void rkNand_cond_resched(void);

#define COND_RESCHED() rkNand_cond_resched()//cond_resched()

#define PRINTF RKNAND_DEBUG
#endif
