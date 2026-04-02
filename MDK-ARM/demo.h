/**
 ****************************************************************************************************
 * @file        demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MC2640模块测试实验（DCMI）
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 阿波罗 H743开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __DEMO_H
#define __DEMO_H
#define DEMO_OUTPUT_WIDTH   160
#define DEMO_OUTPUT_HEIGHT  160

#define DEMO_SENSOR_WIDTH   800
#define DEMO_SENSOR_HEIGHT  600

#define DEMO_IMAGE_WIDTH    600
#define DEMO_IMAGE_HEIGHT   600
#define DEMO_IMAGE_STARTX   100
#define DEMO_IMAGE_STARTY    0

#define HAEDWAREBUG          1


void demo_run(void);

#endif
