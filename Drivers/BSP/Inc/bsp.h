/******************************************************************************
      版权所有：依福斯电子
      版 本 号: 1.0
      文 件 名: bsp.h
      生成日期: 2016.09.25
      作    者：like
      功能说明：板级支持包
      其他说明： 
      修改记录：
*******************************************************************************/
#ifndef _BSP_H
#define _BSP_H


extern void bsp_init(void);
extern void SysTick_Handler(void);
extern void bsp_system_reboot(void);
extern int bsp_watchdog(void);
extern void bsp_gpio_OUT0A(char on_off);
extern void bsp_gpio_OUT0B(char on_off);
extern void bsp_gpio_OUT0C(char on_off);
extern void bsp_gpio_OUT0D(char on_off);
extern void len_run(char on_off);


#endif /*  _BSP_H */

