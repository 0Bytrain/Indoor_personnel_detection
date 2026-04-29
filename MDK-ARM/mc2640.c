/**
 ****************************************************************************************************
 * @file        atk_mc2640.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MC2640模块驱动代码
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
#include "mc2640_cfg.h"
#include "mc2640.h"
#include "main.h"
#include "dma.h"
#include "dcmi.h"
#include "mc2640_sccb.h"
#include <stdio.h>
#include <string.h>
#include "tim.h"
/*
    普通定时器实现us延时
*/
void delay_us(uint32_t nus)
{

    uint16_t  differ = 0xffff-nus-5;
    //设置定时器2的技术初始值
  __HAL_TIM_SetCounter(&htim2,differ);
  //开启定时器
  HAL_TIM_Base_Start(&htim2);

  while( differ<0xffff-5)
    {
        differ = __HAL_TIM_GetCounter(&htim2);
    };
 //关闭定时器
  HAL_TIM_Base_Stop(&htim2);
}
/*
	普通定时器实现ms延时，可直接使用HAL库函数HAL_delay（）
*/
void  delay_ms(uint16_t nms)
{
	uint32_t i;
	for(i=0;i<nms;i++) delay_us(1000);
}
#if (ATK_MC2640_USING_DCMI == 0)
#include "./MALLOC/malloc.h"
#endif

#if (ATK_MC2640_USING_DCMI != 0)
#include "dcmi.h"
#endif

/* ATK-MC2640模块制造商ID和产品ID */
#define ATK_MC2640_MID  0x7FA2
#define ATK_MC2640_PID  0x2642

/* ATK-MC2640寄存器块枚举 */
typedef enum
{
    ATK_MC2640_REG_BANK_DSP = 0x00, /* DSP寄存器块 */
    ATK_MC2640_REG_BANK_SENSOR,     /* Sensor寄存器块 */
} atk_mc2640_reg_bank_t;

/* ATK-MC2640模块数据结构体 */
static struct
{
    struct {
        uint16_t width;
        uint16_t height;
    } output;
    
#if (ATK_MC2640_USING_DCMI == 0)
    struct {
        uint8_t *line_buf;
        DMA_HandleTypeDef dma_handle;
    } read;
#endif
} g_atk_mc2640_sta = {0};

/**
 * @brief       ATK-MC2640模块写寄存器
 * @param       reg: 寄存器地址
 *              dat: 待写入的值
 * @retval      无
 */
static void atk_mc2640_write_reg(uint8_t reg, uint8_t dat)
{
    atk_mc2640_sccb_3_phase_write(ATK_MC2640_SCCB_ADDR, reg, dat);
}

/**
 * @brief       ATK-MC2640模块读寄存器
 * @param       reg: 寄存器的地址
 * @retval      读取到的寄存器值
 */
static uint8_t atk_mc2640_read_reg(uint8_t reg)
{
    uint8_t dat = 0;
    
    atk_mc2640_sccb_2_phase_write(ATK_MC2640_SCCB_ADDR, reg);
    atk_mc2640_sccb_2_phase_read(ATK_MC2640_SCCB_ADDR, &dat);
    
    return dat;
}

/**
 * @brief       设置ATK-MC2640模块启用的寄存器块
 * @param       set: ATK_MC2640_REG_BANK_DSP   : DSP寄存器块
 *                   ATK_MC2640_REG_BANK_SENSOR: Sensor寄存器块
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块启用的寄存器块成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
static uint8_t atk_mc2640_reg_bank_select(atk_mc2640_reg_bank_t bank)
{
    switch (bank)
    {
        case ATK_MC2640_REG_BANK_DSP:
        {
            atk_mc2640_write_reg(ATK_MC2640_REG_BANK_SEL, 0x00);
            break;
        }
        case ATK_MC2640_REG_BANK_SENSOR:
        {
            atk_mc2640_write_reg(ATK_MC2640_REG_BANK_SEL, 0x01);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       ATK-MC2640模块硬件初始化
 * @param       无
 * @retval      无
 */
static void atk_mc2640_hw_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    ATK_MC2640_RST_GPIO_CLK_ENABLE();
    

    
    /* 初始化RST引脚 */
    gpio_init_struct.Pin    = ATK_MC2640_RST_GPIO_PIN;
    gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull   = GPIO_PULLUP;
    gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(ATK_MC2640_RST_GPIO_PORT, &gpio_init_struct);
    ATK_MC2640_RST(1);
    ATK_MC2640_PWDN(1);

}

/**
 * @brief       ATK-MC2640模块退出掉电模式
 * @param       无
 * @retval      无
 */
static void atk_mc2640_exit_power_down(void)
{
    ATK_MC2640_PWDN(0);
    delay_ms(10);
}

/**
 * @brief       ATK-MC2640模块硬件复位
 * @param       无
 * @retval      无
 */
static void atk_mc2640_hw_reset(void)
{
    ATK_MC2640_RST(0);
    delay_ms(10);
    ATK_MC2640_RST(1);
    delay_ms(10);
}

/**
 * @brief       ATK-MC2640模块软件复位
 * @param       无
 * @retval      无
 */
static void atk_mc2640_sw_reset(void)
{
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM7, 0x80);
    delay_ms(50);
}

/**
 * @brief       获取ATK-MC2640模块制造商ID
 * @param       无
 * @retval      制造商ID
 */
static uint16_t atk_mc2640_get_mid(void)
{
    uint16_t mid;
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    mid = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_MIDH) << 8;
    mid |= atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_MIDL);
    
    return mid;
}

/**
 * @brief       获取ATK-MC2640模块产品ID
 * @param       无
 * @retval      产品ID
 */
static uint16_t atk_mc2640_get_pid(void)
{
    uint16_t pid;
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    pid = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_PIDH) << 8;
    pid |= atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_PIDL);
    
    return pid;
}

/**
 * @brief       初始化ATK-MC2640寄存器配置
 * @param       无
 * @retval      无
 */
static void atk_mc2640_init_reg(void)
{
    uint32_t cfg_index;
    uint8_t zmow;
    uint8_t zmoh;
    uint8_t zmhh;
    
    for (cfg_index=0; cfg_index<(sizeof(atk_mc2640_init_svga_cfg)/sizeof(atk_mc2640_init_svga_cfg[0])); cfg_index++)
    {
        atk_mc2640_write_reg(atk_mc2640_init_svga_cfg[cfg_index][0], atk_mc2640_init_svga_cfg[cfg_index][1]);
    }
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
    zmow = atk_mc2640_read_reg(ATK_MC2640_REG_DSP_ZMOW);
    zmoh = atk_mc2640_read_reg(ATK_MC2640_REG_DSP_ZMOH);
    zmhh = atk_mc2640_read_reg(ATK_MC2640_REG_DSP_ZMHH);
    
    g_atk_mc2640_sta.output.width = ((uint16_t)zmow | ((zmhh & 0x03) << 8)) << 2;
    g_atk_mc2640_sta.output.height = ((uint16_t)zmoh | ((zmhh & 0x04) << 6)) << 2;
}



/**
 * @brief       初始化ATK-MC2640模块
 * @param       无
 * @retval      ATK_MC2640_EOK   : ATK-MC2640模块初始化成功
 *              ATK_MC2640_ERROR : 通讯出错，ATK-MC2640模块初始化失败
 *              ATK_MC2640_ENOMEM: 内存不足，ATK-MC2640模块初始化失败
 */
uint8_t atk_mc2640_init(void)
{
    uint16_t mid = 0;
    uint16_t pid = 0;
    
    atk_mc2640_hw_init();           /* ATK-MC2640模块硬件初始化 */
    atk_mc2640_exit_power_down();   /* ATK-MC2640模块退出掉电模式 */
    atk_mc2640_hw_reset();          /* ATK-MC2640模块硬件复位 */
    atk_mc2640_sccb_init();         /* ATK-MC2640 SCCB接口初始化 */
    atk_mc2640_sw_reset();          /* ATK-MC2640模块软件复位 */
        
    mid = atk_mc2640_get_mid();     /* 获取制造商ID */
    if (mid != ATK_MC2640_MID)
    {
        return ATK_MC2640_ERROR;
    }
    
    pid = atk_mc2640_get_pid();     /* 获取产品ID */
    if (pid != ATK_MC2640_PID)
    {

        return ATK_MC2640_ERROR;
    }
//    printf("OV2640 detected, PID: 0x%02X, MID: 0x%02X\n\r", pid, mid);
    atk_mc2640_init_reg();          /* 初始化ATK-MC2640寄存器配置 */
    
    
#if (ATK_MC2640_USING_DCMI != 0)

//    MX_DCMI_Init();         /* 初始化ATK-MC2640模块DCMI接口 */
#endif
    
    return ATK_MC2640_EOK;
}


/**
 * @brief       设置ATK-MC2640模块灯光模式
 * @param       mode: ATK_MC2640_LIGHT_MOED_AUTO  : Auto
 *                    ATK_MC2640_LIGHT_MOED_SUNNY : Sunny
 *                    ATK_MC2640_LIGHT_MOED_CLOUDY: Cloudy
 *                    ATK_MC2640_LIGHT_MOED_OFFICE: Office
 *                    ATK_MC2640_LIGHT_MOED_HOME  : Home
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块灯光模式成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_light_mode(atk_mc2640_light_mode_t mode)
{
    switch (mode)
    {
        case ATK_MC2640_LIGHT_MODE_AUTO:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0xC7, 0x00);
            break;
        }
        case ATK_MC2640_LIGHT_MODE_SUNNY:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0xC7, 0x40);
            atk_mc2640_write_reg(0xCC, 0x5E);
            atk_mc2640_write_reg(0xCD, 0x41);
            atk_mc2640_write_reg(0xCE, 0x54);
            break;
        }
        case ATK_MC2640_LIGHT_MODE_CLOUDY:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0xC7, 0x40);
            atk_mc2640_write_reg(0xCC, 0x65);
            atk_mc2640_write_reg(0xCD, 0x41);
            atk_mc2640_write_reg(0xCE, 0x4F);
            break;
        }
        case ATK_MC2640_LIGHT_MODE_OFFICE:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0xC7, 0x40);
            atk_mc2640_write_reg(0xCC, 0x52);
            atk_mc2640_write_reg(0xCD, 0x41);
            atk_mc2640_write_reg(0xCE, 0x66);
            break;
        }
        case ATK_MC2640_LIGHT_MODE_HOME:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0xC7, 0x40);
            atk_mc2640_write_reg(0xCC, 0x42);
            atk_mc2640_write_reg(0xCD, 0x3F);
            atk_mc2640_write_reg(0xCE, 0x71);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块色彩饱和度
 * @param       saturation: ATK_MC2640_COLOR_SATURATION_0: +2
 *                          ATK_MC2640_COLOR_SATURATION_1: +1
 *                          ATK_MC2640_COLOR_SATURATION_2: 0
 *                          ATK_MC2640_COLOR_SATURATION_3: -1
 *                          ATK_MC2640_COLOR_SATURATION_4: -2
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块色彩饱和度成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_color_saturation(atk_mc2640_color_saturation_t saturation)
{
    switch (saturation)
    {
        case ATK_MC2640_COLOR_SATURATION_0:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x02);
            atk_mc2640_write_reg(0x7C, 0x03);
            atk_mc2640_write_reg(0x7D, 0x68);
            atk_mc2640_write_reg(0x7D, 0x68);
            break;
        }
        case ATK_MC2640_COLOR_SATURATION_1:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x02);
            atk_mc2640_write_reg(0x7C, 0x03);
            atk_mc2640_write_reg(0x7D, 0x58);
            atk_mc2640_write_reg(0x7D, 0x58);
            break;
        }
        case ATK_MC2640_COLOR_SATURATION_2:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x02);
            atk_mc2640_write_reg(0x7C, 0x03);
            atk_mc2640_write_reg(0x7D, 0x48);
            atk_mc2640_write_reg(0x7D, 0x48);
            break;
        }
        case ATK_MC2640_COLOR_SATURATION_3:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x02);
            atk_mc2640_write_reg(0x7C, 0x03);
            atk_mc2640_write_reg(0x7D, 0x38);
            atk_mc2640_write_reg(0x7D, 0x38);
            break;
        }
        case ATK_MC2640_COLOR_SATURATION_4:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x02);
            atk_mc2640_write_reg(0x7C, 0x03);
            atk_mc2640_write_reg(0x7D, 0x28);
            atk_mc2640_write_reg(0x7D, 0x28);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块亮度
 * @param       brightness: ATK_MC2640_BRIGHTNESS_0: +2
 *                          ATK_MC2640_BRIGHTNESS_1: +1
 *                          ATK_MC2640_BRIGHTNESS_2: 0
 *                          ATK_MC2640_BRIGHTNESS_3: -1
 *                          ATK_MC2640_BRIGHTNESS_4: -2
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块亮度成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_brightness(atk_mc2640_brightness_t brightness)
{
    switch (brightness)
    {
        case ATK_MC2640_BRIGHTNESS_0:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x09);
            atk_mc2640_write_reg(0x7D, 0x40);
            atk_mc2640_write_reg(0x7D, 0x00);
            break;
        }
        case ATK_MC2640_BRIGHTNESS_1:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x09);
            atk_mc2640_write_reg(0x7D, 0x30);
            atk_mc2640_write_reg(0x7D, 0x00);
            break;
        }
        case ATK_MC2640_BRIGHTNESS_2:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x09);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x00);
            break;
        }
        case ATK_MC2640_BRIGHTNESS_3:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x09);
            atk_mc2640_write_reg(0x7D, 0x10);
            atk_mc2640_write_reg(0x7D, 0x00);
            break;
        }
        case ATK_MC2640_BRIGHTNESS_4:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x09);
            atk_mc2640_write_reg(0x7D, 0x00);
            atk_mc2640_write_reg(0x7D, 0x00);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块对比度
 * @param       contrast: ATK_MC2640_CONTRAST_0: +2
 *                        ATK_MC2640_CONTRAST_1: +1
 *                        ATK_MC2640_CONTRAST_2: 0
 *                        ATK_MC2640_CONTRAST_3: -1
 *                        ATK_MC2640_CONTRAST_4: -2
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块对比度成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_contrast(atk_mc2640_contrast_t contrast)
{
    switch (contrast)
    {
        case ATK_MC2640_CONTRAST_0:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x07);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x28);
            atk_mc2640_write_reg(0x7D, 0x0C);
            atk_mc2640_write_reg(0x7D, 0x06);
            break;
        }
        case ATK_MC2640_CONTRAST_1:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x07);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x24);
            atk_mc2640_write_reg(0x7D, 0x16);
            atk_mc2640_write_reg(0x7D, 0x06);
            break;
        }
        case ATK_MC2640_CONTRAST_2:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x07);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x06);
            break;
        }
        case ATK_MC2640_CONTRAST_3:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x07);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x1C);
            atk_mc2640_write_reg(0x7D, 0x2A);
            atk_mc2640_write_reg(0x7D, 0x06);
            break;
        }
        case ATK_MC2640_CONTRAST_4:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x04);
            atk_mc2640_write_reg(0x7C, 0x07);
            atk_mc2640_write_reg(0x7D, 0x20);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7D, 0x34);
            atk_mc2640_write_reg(0x7D, 0x06);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}


/**
 * @brief       设置ATK-MC2640模块特殊效果
 * @param       contrast: ATK_MC2640_SPECIAL_EFFECT_ANTIQUE    : Antique
 *                        ATK_MC2640_SPECIAL_EFFECT_BLUISH     : Bluish
 *                        ATK_MC2640_SPECIAL_EFFECT_GREENISH   : Greenish
 *                        ATK_MC2640_SPECIAL_EFFECT_REDISH     : Redish
 *                        ATK_MC2640_SPECIAL_EFFECT_BW         : B&W
 *                        ATK_MC2640_SPECIAL_EFFECT_NEGATIVE   : Negative
 *                        ATK_MC2640_SPECIAL_EFFECT_BW_NEGATIVE: B&W Negative
 *                        ATK_MC2640_SPECIAL_EFFECT_NORMAL     : Normal
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块特殊效果成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_special_effect(atk_mc2640_special_effect_t effect)
{
    switch (effect)
    {
        case ATK_MC2640_SPECIAL_EFFECT_ANTIQUE:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x40);
            atk_mc2640_write_reg(0x7D, 0xA6);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_BLUISH:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0xA0);
            atk_mc2640_write_reg(0x7D, 0x40);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_GREENISH:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x40);
            atk_mc2640_write_reg(0x7D, 0x40);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_REDISH:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x40);
            atk_mc2640_write_reg(0x7D, 0xC0);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_BW:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x18);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x80);
            atk_mc2640_write_reg(0x7D, 0x80);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_NEGATIVE:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x40);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x80);
            atk_mc2640_write_reg(0x7D, 0x80);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_BW_NEGATIVE:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x58);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x80);
            atk_mc2640_write_reg(0x7D, 0x80);
            break;
        }
        case ATK_MC2640_SPECIAL_EFFECT_NORMAL:
        {
            atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
            atk_mc2640_write_reg(0x7C, 0x00);
            atk_mc2640_write_reg(0x7D, 0x00);
            atk_mc2640_write_reg(0x7C, 0x05);
            atk_mc2640_write_reg(0x7D, 0x80);
            atk_mc2640_write_reg(0x7D, 0x80);
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块输出图像格式
 * @param       mode: ATK_MC2640_OUTPUT_FORMAT_RGB565: RGB565
 *                    ATK_MC2640_OUTPUT_FORMAT_JPEG : JPEG
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块输出图像格式成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_output_format(atk_mc2640_output_format_t format)
{
    uint32_t cfg_index;
    
    switch (format)
    {
        case ATK_MC2640_OUTPUT_FORMAT_RGB565:
        {
            for (cfg_index=0; cfg_index<(sizeof(atk_mc2640_set_rgb565_cfg)/sizeof(atk_mc2640_set_rgb565_cfg[0])); cfg_index++)
            {
                atk_mc2640_write_reg(atk_mc2640_set_rgb565_cfg[cfg_index][0], atk_mc2640_set_rgb565_cfg[cfg_index][1]);
            }
            break;
        }
        case ATK_MC2640_OUTPUT_FORMAT_JPEG:
        {
            for (cfg_index=0; cfg_index<(sizeof(atk_mc2640_set_yuv422_cfg)/sizeof(atk_mc2640_set_yuv422_cfg[0])); cfg_index++)
            {
                atk_mc2640_write_reg(atk_mc2640_set_yuv422_cfg[cfg_index][0], atk_mc2640_set_yuv422_cfg[cfg_index][1]);
            }
            for (cfg_index=0; cfg_index<(sizeof(atk_mc2640_set_jpeg_cfg)/sizeof(atk_mc2640_set_jpeg_cfg[0])); cfg_index++)
            {
                atk_mc2640_write_reg(atk_mc2640_set_jpeg_cfg[cfg_index][0], atk_mc2640_set_jpeg_cfg[cfg_index][1]);
            }
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块输出图像分辨率
 * @param       width : 输出图像宽度，必须是4的倍数
 *              height: 输出图像高度，必须是4的倍数
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块输出图像大小成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 *              ATK_MC2640_ENOMEM: 内存不足
 */
uint8_t atk_mc2640_set_output_size(uint16_t width, uint16_t height)
{
    uint16_t output_width;
    uint16_t output_height;
    
    if (((width & (4 - 1)) != 0) || ((height & (4 - 1)) != 0))
    {
        return ATK_MC2640_EINVAL;
    }
    
#if (ATK_MC2640_USING_DCMI == 0)
    myfree(SRAMIN, g_atk_mc2640_sta.read.line_buf);
    g_atk_mc2640_sta.read.line_buf = mymalloc(SRAMIN, width * sizeof(uint16_t));
    if (g_atk_mc2640_sta.read.line_buf == NULL)
    {
        g_atk_mc2640_sta.read.line_buf = mymalloc(SRAMIN, g_atk_mc2640_sta.output.width * sizeof(uint16_t));
        return ATK_MC2640_ENOMEM;
    }
    else
#endif
    {
        g_atk_mc2640_sta.output.width = width;
        g_atk_mc2640_sta.output.height = height;
    }
    
    output_width = width >> 2;
    output_height = height >> 2;
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x04);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_ZMOW, (uint8_t)(output_width & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_ZMOH, (uint8_t)(output_height & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_ZMHH, ((uint8_t)(output_width >> 8) & 0x03) | ((uint8_t)(output_height >> 6) & 0x04));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x00);
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块传感器窗口
 * @param       start_x: 传感器窗口起始X坐标
 *              start_y: 传感器窗口起始Y坐标
 *              width  : 传感器窗口宽度
 *              height : 传感器窗口高度
 * @retval      无
 */
void atk_mc2640_set_sensor_window(uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height)
{
    uint16_t end_x;
    uint16_t end_y;
    uint8_t raw_com1;
    uint8_t com1;
    uint8_t raw_reg32;
    uint8_t reg32;
    
    end_x = start_x + (width >> 1);
    end_y = start_y + (height >> 1);
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    
    raw_com1 = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_COM1);
    com1 = (raw_com1 & 0xF0) | (((end_y & 0x03) << 2) | (start_y & 0x03));
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM1, com1);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_VSTRT, start_y >> 2);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_VEND, end_y >> 2);
    
    raw_reg32 = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_REG32);
    reg32 = (raw_reg32 & 0xC0) | (((end_x & 0x07) << 3) | (start_x & 0x07));
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_REG32, reg32);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_HREFST, start_x >> 3);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_HREFEND, end_x >> 3);
}

/**
 * @brief       设置ATK-MC2640模块输出图像窗口
 * @param       off_x : 输出图像窗口偏移X坐标
 *              off_y : 输出图像窗口偏移Y坐标
 *              width : 输出图像窗口宽度，必须是4的倍数
 *              height: 输出图像窗口高度，必须是4的倍数
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块输出图像大小成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_image_window(uint16_t off_x, uint16_t off_y, uint16_t width, uint16_t height)
{
    uint16_t hsize;
    uint16_t vsize;
    uint8_t vhyx;
    
    if (((width & (4 - 1)) != 0) || ((height & (4 - 1)) != 0))
    {
        return ATK_MC2640_EINVAL;
    }
    
    hsize = width >> 2;
    vsize = height >> 2;
    
    vhyx = (uint8_t)(((vsize >> 1) & 0x80) | ((off_y >> 4) & 0x70) | ((hsize >> 5) & 0x08) | ((off_x >> 8) & 0x07));
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x04);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_HSIZE, (uint8_t)(hsize & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_VSIZE, (uint8_t)(vsize & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_XOFFL, (uint8_t)(off_x & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_YOFFL, (uint8_t)(off_y & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_VHYX, vhyx);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_TEST, (uint8_t)((hsize >> 2) & 0x80));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x00);
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置ATK-MC2640模块输出图像大小
 * @param       off_x : 输出图像窗口偏移X坐标
 *              off_y : 输出图像窗口偏移Y坐标
 *              width : 输出图像窗口宽度，必须是4的倍数
 *              height: 输出图像窗口高度，必须是4的倍数
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块输出图像大小成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
void atk_mc2640_set_image_size(uint16_t width, uint16_t height)
{
    uint8_t sizel;
    
    sizel = (uint8_t)(((width & 0x07) << 3) | (height & 0x07) | ((width >> 4) & 0x80));
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x04);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_HSIZE8, (uint8_t)((width >> 3) & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_VSIZE8, (uint8_t)((height >> 3) & 0x00FF));
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_SIZEL, sizel);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_RESET, 0x00);
}

/**
 * @brief       设置ATK-MC2640模块输出速率
 * @param       clk_dev : Clock divider（0~63）
 *              pclk_dev: DVP PCLK（1~127）
 * @retval      ATK_MC2640_EOK   : 设置ATK-MC2640模块输出速率成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 */
uint8_t atk_mc2640_set_output_speed(uint8_t clk_dev, uint8_t pclk_dev)
{
    if (clk_dev > 63)
    {
        return ATK_MC2640_EINVAL;
    }
    
    if ((pclk_dev == 0) || (pclk_dev > 127))
    {
        return ATK_MC2640_EINVAL;
    }
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_DSP);
    atk_mc2640_write_reg(ATK_MC2640_REG_DSP_R_DVP_SP, pclk_dev);
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_CLKRC, clk_dev);
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       开启ATK-MC2640模块彩条测试
 * @param       无
 * @retval      无
 */
void atk_mc2640_colorbar_enable(void)
{
    uint8_t com7;
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    com7 = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_COM7);
    com7 |= (uint8_t)(1 << 1);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM7, com7);
}

/**
 * @brief       关闭ATK-MC2640模块彩条测试
 * @param       无
 * @retval      无
 */
void atk_mc2640_colorbar_disable(void)
{
    uint8_t com7;
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    com7 = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_COM7);
    com7 &= ~(uint8_t)(1 << 1);
    atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM7, com7);
}

/**
 * @brief       获取ATK-MC2640模块输出的一帧图像数据
 * @param       dts_addr : 帧数据的接收缓冲的首地址
 *              type: ATK_MC2640_GET_TYPE_DTS_8B_NOINC : 图像数据以字节方式写入目的地址，目的地址固定不变
 *                    ATK_MC2640_GET_TYPE_DTS_8B_INC   : 图像数据以字节方式写入目的地址，目的地址自动增加
 *                    ATK_MC2640_GET_TYPE_DTS_16B_NOINC: 图像数据以半字方式写入目的地址，目的地址固定不变
 *                    ATK_MC2640_GET_TYPE_DTS_16B_INC  : 图像数据以半字方式写入目的地址，目的地址自动增加
 *                    ATK_MC2640_GET_TYPE_DTS_32B_NOINC: 图像数据以字方式写入目的地址，目的地址固定不变
 *                    ATK_MC2640_GET_TYPE_DTS_32B_INC  : 图像数据以字方式写入目的地址，目的地址自动增加
 *              before_transfer: 帧数据传输前，需要完成的事务，可为NULL
 * @retval      ATK_MC2640_EOK   : 获取ATK-MC2640模块输出的一帧图像数据成功
 *              ATK_MC2640_EINVAL: 传入参数错误
 *              ATK_MC2640_EEMPTY: 图像数据为空
 */
uint8_t atk_mc2640_get_frame(uint32_t dts_addr, atk_mc2640_get_type_t type, void (*before_transfer)(void))
{
    uint32_t meminc;
    uint32_t memdataalignment;
#if (ATK_MC2640_USING_DCMI != 0)
    uint32_t len;
#else
    uint16_t pixel_cnt = 0;
    uint16_t line_cnt = 0;
    uint16_t dts_offset;
#endif
    
    switch (type)
    {
        case ATK_MC2640_GET_TYPE_DTS_8B_NOINC:
        {
            meminc = DMA_MINC_DISABLE;
            memdataalignment = DMA_MDATAALIGN_BYTE;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) / sizeof(uint8_t);
#else
            dts_offset = 0;
#endif
            break;
        }
        case ATK_MC2640_GET_TYPE_DTS_8B_INC:
        {
            meminc = DMA_MINC_ENABLE;
            memdataalignment = DMA_MDATAALIGN_BYTE;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) / sizeof(uint8_t);
#else
            dts_offset = g_atk_mc2640_sta.output.width << 1;
#endif
            break;
        }
        case ATK_MC2640_GET_TYPE_DTS_16B_NOINC:
        {
            meminc = DMA_MINC_DISABLE;
            memdataalignment = DMA_MDATAALIGN_HALFWORD;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) /sizeof(uint16_t);
#else
            dts_offset = 0;
#endif
            break;
        }
        case ATK_MC2640_GET_TYPE_DTS_16B_INC:
        {
            meminc = DMA_MINC_ENABLE;
            memdataalignment = DMA_MDATAALIGN_HALFWORD;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) / sizeof(uint16_t);
#else
            dts_offset = g_atk_mc2640_sta.output.width << 1;
#endif
            break;
        }
        case ATK_MC2640_GET_TYPE_DTS_32B_NOINC:
        {
            meminc = DMA_MINC_DISABLE;
            memdataalignment = DMA_MDATAALIGN_WORD;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) / sizeof(uint32_t);
#else
            dts_offset = 0;
#endif
            break;
        }
        case ATK_MC2640_GET_TYPE_DTS_32B_INC:
        {
            meminc = DMA_MINC_ENABLE;
            memdataalignment = DMA_MDATAALIGN_WORD;
#if (ATK_MC2640_USING_DCMI != 0)
            len = (g_atk_mc2640_sta.output.width * g_atk_mc2640_sta.output.height) / sizeof(uint32_t);
#else
            dts_offset = g_atk_mc2640_sta.output.width << 1;
#endif
            break;
        }
        default:
        {
            return ATK_MC2640_EINVAL;
        }
    }
    
    
#if (ATK_MC2640_USING_DCMI != 0)
    if (before_transfer != NULL)
    {
        before_transfer();
    }
    atk_mc2640_dcmi_start(dts_addr, meminc, memdataalignment, len);
#endif
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置OV2640曝光参数
 * @param       exposure: 总曝光时间（行数）
 *              gain: 增益值（0-255，实际增益倍数 = (gain_value + 16) * 2^(gain_multiplier)）
 *              dummy_lines: 虚拟行数（影响最大快门时间）
 *              dummy_pixels: 虚拟像素数
 * @retval      ATK_MC2640_EOK: 成功
 *              ATK_MC2640_EINVAL: 参数错误
 */
uint8_t atk_mc2640_set_exposure_params(uint32_t exposure, uint8_t gain, uint16_t dummy_lines, uint16_t dummy_pixels)
{
    uint8_t reg_val;
    uint16_t shutter;
    uint16_t extra_lines;
    uint32_t max_shutter;
    
    // 计算最大快门值（基于分辨率和虚拟行）
    // UXGA模式默认最大快门值，需要根据实际情况调整
    uint16_t default_uxga_max_shutter = 0x1000; // 需要根据实际调试确定
    max_shutter = default_uxga_max_shutter + dummy_lines;
    
    // 计算快门和额外行
    if (exposure > max_shutter) {
        shutter = max_shutter;
        extra_lines = exposure - max_shutter;
    } else {
        shutter = exposure;
        extra_lines = 0;
    }
    
    // 切换到Sensor寄存器块
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    
    // 设置虚拟像素
    reg_val = atk_mc2640_read_reg(0x2A);
    reg_val = (reg_val & 0x0F) | ((dummy_pixels & 0x0F00) >> 4);
    atk_mc2640_write_reg(0x2A, reg_val);
    atk_mc2640_write_reg(0x2B, (uint8_t)(dummy_pixels & 0x00FF));
    
    // 设置虚拟行
    atk_mc2640_write_reg(0x46, (uint8_t)(dummy_lines & 0x00FF));
    atk_mc2640_write_reg(0x47, (uint8_t)(dummy_lines >> 8));
    
    uint8_t reg04, reg45;

		reg04 = atk_mc2640_read_reg(0x04);
		reg04 = (reg04 & 0xFC) | (shutter & 0x03);
		atk_mc2640_write_reg(0x04, reg04);

		atk_mc2640_write_reg(0x10, (shutter >> 2) & 0xFF);

		reg45 = atk_mc2640_read_reg(0x45);
		reg45 = (reg45 & 0xC0) | ((shutter >> 10) & 0x3F);
		atk_mc2640_write_reg(0x45, reg45);

    // 设置额外行
    atk_mc2640_write_reg(0x2D, (uint8_t)(extra_lines & 0x00FF));
    atk_mc2640_write_reg(0x2E, (uint8_t)(extra_lines >> 8));
    
    // 设置增益
    atk_mc2640_set_gain(gain);
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置OV2640增益
 * @param       gain: 增益值（0-255）
 * @retval      ATK_MC2640_EOK: 成功
 */
uint8_t atk_mc2640_set_gain(uint8_t gain)
{
    uint8_t gain_reg = 0;
    uint8_t capture_gain16 = gain;
    
    // 按照官方手册的算法计算增益寄存器值
    if (capture_gain16 > 16) {
        capture_gain16 = capture_gain16 / 2;
        gain_reg = 0x10;
    }
    if (capture_gain16 > 16) {
        capture_gain16 = capture_gain16 / 2;
        gain_reg |= 0x20;
    }
    if (capture_gain16 > 16) {
        capture_gain16 = capture_gain16 / 2;
        gain_reg |= 0x40;
    }
    if (capture_gain16 > 16) {
        capture_gain16 = capture_gain16 / 2;
        gain_reg |= 0x80;
    }
    
    gain_reg |= (capture_gain16 - 16);
    
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    atk_mc2640_write_reg(0x00, gain_reg);
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置自动曝光
 * @param       enable: 1-启用自动曝光，0-禁用自动曝光
 * @retval      ATK_MC2640_EOK: 成功
 */
uint8_t atk_mc2640_set_auto_exposure(uint8_t enable)
{
    atk_mc2640_reg_bank_select(ATK_MC2640_REG_BANK_SENSOR);
    
    if (enable) {
        // 启用AEC/AGC自动控制
        atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM8, 0xE7);

    } else {
        // 禁用自动曝光，使用手动设置
        uint8_t com8 = atk_mc2640_read_reg(ATK_MC2640_REG_SENSOR_COM8);
        com8 &= ~0x01; // 清除AEC使能位
        atk_mc2640_write_reg(ATK_MC2640_REG_SENSOR_COM8, com8);
    }
    
    return ATK_MC2640_EOK;
}

/**
 * @brief       设置曝光级别（机器视觉/单拍优化版）
 * @param       level: 曝光级别 
 * 0: 自动曝光
 * 1-100: 手动长曝光级别（纯拉长快门时间，极低噪点）
 * @retval      ATK_MC2640_EOK: 成功
 */
uint8_t atk_mc2640_set_exposure_level(uint8_t level)
{
    if (level == 0)
    {
        return atk_mc2640_set_auto_exposure(1);
    }
    else
    {
        atk_mc2640_set_auto_exposure(0);
        uint32_t exposure_time;
        uint8_t gain_value = 16; // 永远锁死在最低增益(1倍)，保证画面纯净无噪点
        // 将 level (1-100) 线性映射到一个非常大的曝光范围
        // 假设正常 UXGA 最大快门是 4096，我们允许它突破限制，让底层函数去加 extra_lines
        // 这里的步进值可以根据你室内的实际光线调整
        exposure_time = 1000 + (level * 300); // 范围: 1300 到 31000 行
        // 仅在极端暗光需求 (level > 90) 时，才稍微妥协给一点点增益
        if (level > 90)
        {
            gain_value = 16 + (level - 90); // 增益从 16 缓增到 26
        }

        // 传入底层函数，dummy参数保持为0，因为底层会自动用 extra_lines 补偿超出的部分
        return atk_mc2640_set_exposure_params(exposure_time, gain_value, 0, 0);
    }
}