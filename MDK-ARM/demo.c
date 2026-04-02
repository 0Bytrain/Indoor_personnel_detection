/**
 ****************************************************************************************************
 * @file        demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MC2640模块测试实验（DCMI）
 ****************************************************************************************************
 */


#include "demo.h"
#include "mc2640.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "mc2640_sccb.h"
#include "malloc.h"
static uint8_t dump_once = 0;
extern DCMI_HandleTypeDef hdcmi;

/* ================= 配置 ================= */

#define PIXELS     (DEMO_OUTPUT_WIDTH * DEMO_OUTPUT_HEIGHT)
#define BYTES      (PIXELS * 2U)

/* DMA 写入用：AXI SRAM */
__attribute__((section(".AxSRAM"), aligned(32)))
uint16_t cam_dma_buf[PIXELS];

/* CPU / AI 使用 */
uint16_t image_buffer[PIXELS];

/* ================= Cache 处理 ================= */

static inline void dcache_invalidate(void *addr, uint32_t size)
{
    uint32_t a = ((uint32_t)addr) & ~31U;
    uint32_t s = (size + ((uint32_t)addr - a) + 31U) & ~31U;
    SCB_InvalidateDCache_by_Addr((uint32_t *)a, (int32_t)s);
    __DSB();
    __ISB();
}

/* ================= 主流程 ================= */
void demo_run(void)
{
	uint8_t ret;
	ret  = atk_mc2640_init();
	ret |= atk_mc2640_set_output_format(ATK_MC2640_OUTPUT_FORMAT_RGB565);
	/*我发现不设置窗口竟然可以把全图压缩*/
	ret |= atk_mc2640_set_output_size(DEMO_OUTPUT_WIDTH, DEMO_OUTPUT_HEIGHT);

	atk_mc2640_set_light_mode(ATK_MC2640_LIGHT_MODE_OFFICE);//白天模式
	//atk_mc2640_set_light_mode(ATK_MC2640_LIGHT_MODE_AUTO);//夜间模式
	atk_mc2640_set_color_saturation(ATK_MC2640_COLOR_SATURATION_0);
	atk_mc2640_set_brightness(ATK_MC2640_BRIGHTNESS_0);
	atk_mc2640_set_contrast(ATK_MC2640_CONTRAST_1);
	atk_mc2640_set_special_effect(ATK_MC2640_SPECIAL_EFFECT_NORMAL );
	uint8_t exposure_level = 80;//白天80夜间100即可
	uint8_t frame_count = 0;
	while (1)
	{
			atk_mc2640_set_exposure_level(exposure_level);
			delay_ms(50);

			/* DCMI DMA → AXI SRAM */
			atk_mc2640_get_frame(
					(uint32_t)cam_dma_buf,
					ATK_MC2640_GET_TYPE_DTS_16B_INC,
					NULL
			);

			/* 同步 Cache */
			dcache_invalidate(cam_dma_buf, BYTES);

			/* 拷贝给 CPU / AI */
			memcpy(image_buffer, cam_dma_buf, BYTES);
/*下面决定是否传输图片*/		
#define  PHOTO    0
#if  (PHOTO)
			// 例如：图像处理、保存到SD卡、通过网络传输等
			delay_ms(500);
			printf("[IMAGE_START]"); // 直接打印每个元素
			delay_ms(2000);
			for (int i = 0; i < DEMO_OUTPUT_WIDTH *DEMO_OUTPUT_HEIGHT; i++) 
			{ 
					printf("%04X,", image_buffer[i]); // 直接打印每个元素
					delay_ms(1); 
			}
			delay_ms(500);
			printf("[IMAGE_END]");
			delay_ms(500);
#endif			
			break;
					
	}
}
