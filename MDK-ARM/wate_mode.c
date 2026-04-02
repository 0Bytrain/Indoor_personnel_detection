/**
	*************************************************************************
	* @file    wait_mode.c
  * @author  Bytrain
  * @brief   等待期间
	*************************************************************************
	*/
	
 /*
  * Description
  *   v1.0 - 等待上位机配置和上传本地数据，仅一个函数
  */
#include "main.h"
#include "dma.h"
#include "stm32h7xx_it.h"
#include "mc2640_sccb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tim.h"
#include "usart.h"
#include "WIFI.h"

extern uint8_t USART2_RxBuffer[USART2_RX_BUFFER_SIZE];
extern volatile uint8_t USART2_RxFlag;
volatile uint8_t Work_Flag = 1;//到时候放在按键控制文件里
volatile uint8_t Direct_Flag = 0;
volatile uint8_t Single_Flag = 0;
//到时候尝试用结构体返回值来写(主要是状态三用了全局变量)
/**
 * @brief  等待配置模式
 * @param  history_array : 指向保存识别结果的数组首地址
 * @param  p_count       : 指向当前数据数量的指针（传指针是为了能在函数内把它清零,否则只是形参）
 * @return uint32_t      : 返回上位机设定的新时间间隔
 */
uint32_t Enter_Wait_Config_Mode(int *history_array, uint16_t *p_count)
{
	uint32_t new_interval = 0; // 用于接收并返回的新时间间隔				
	// 清空之前的串口残留数据
	USART2_RxPacket_Clear();	
	while (1)
	{
		// 检查是否收到上位机发来的新数据（以换行符结尾）
		if (USART2_RxFlag == 1)
		{
			// 状态 0：等待上位机发送 "GET_HISTORYend" 
			if ( strstr((const char*)USART2_RxBuffer, "GET_HISTORYend") != NULL )
			{
					if(Single_Flag == 0)
					{
						// 收到 OK，开始发送本地数据 (通过解引用指针获取数量)		
						for (int i = 0; i < *p_count; i++)
						{
							printf("order%dpeople%dend\r\n",(i+1), history_array[i]);//哎，难看的回复
							delay_ms(100); // 防止发送过快导致 ESP8266 缓冲区溢出
						}
						//printf("TEST\r\n"); // 告诉上位机发完了
						// 清空本地记录：通过指针把外面的 history_count 清零
						*p_count = 0; 
				  }
					else//单次触发
					{
						printf("order0people%dend\r\n", history_array[255]);
					}
			}
			
			/* 状态 1：等待上位机发送类似 "SET_TIMER_REPEAT:60end" */
			else if (strstr((char*)USART2_RxBuffer, "SET_TIMER_REPEAT:") != NULL)
			{		
				// 使用 sscanf 提取数字
				if (sscanf((char*)USART2_RxBuffer, "SET_TIMER_REPEAT:%d", &new_interval) == 1)
				{
					if (new_interval > 0) 
					{
						Single_Flag = 0;				//  单次触发标志位置0即重复触发
						USART2_RxPacket_Clear(); // 退出前清空标志
						//直接把新的时间间隔作为函数返回值扔出去
						return new_interval; 
					}		
				}
			}
			/* 状态 2：等待上位机发送类似 "SET_TIMER_ONCE:60end" */
			else if(strstr((char*)USART2_RxBuffer, "SET_TIMER_ONCE:") != NULL)
			{
								// 使用 sscanf 提取数字
				if (sscanf((char*)USART2_RxBuffer, "SET_TIMER_ONCE:%d", &new_interval) == 1)
				{
					if (new_interval > 0) 
					{
						USART2_RxPacket_Clear(); // 退出前清空标志
						Single_Flag = 1;				//  单次触发标志位置1
						//直接把新的时间间隔作为函数返回值扔出去
						return new_interval; 
					}		
				}
			}
			/* 状态 3：等待上位机发送 "DIRECTend" ,直接拍照上传数据*/
			else if(strstr((char*)USART2_RxBuffer, "DIRECTend") != NULL)
			{
					 Direct_Flag = 1;
				  return 0;
			}
			// 处理完一帧数据后，清空接收缓冲区准备收下一帧
			USART2_RxPacket_Clear();
		}
		delay_ms(100); // 稍微延时，防止死循环卡死 CPU
	}
}

