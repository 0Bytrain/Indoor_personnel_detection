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
#include "stm32h7xx_it.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tim.h"
#include "usart.h"
#include "WIFI.h"
#include "wait_mode.h"
#include "mc2640.h"//这里有延时函数不要删！！！
extern uint8_t USART2_RxBuffer[USART2_RX_BUFFER_SIZE];
extern volatile uint8_t USART2_RxFlag;

// 定义 RTC 句柄（如果你没有在CubeMX中配置RTC，可以直接在这里定义）
RTC_HandleTypeDef hrtc;

/**
 * @brief  初始化系统 RTC 和 LSI (32kHz) 时钟
 * @note   在 main 函数的初始化阶段调用一次即可
 */
void System_RTC_Init(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // 开启内部低速时钟 (LSI)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // 选择 LSI 作为 RTC 时钟源
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // 使能 RTC 时钟
    __HAL_RCC_RTC_ENABLE();

    // 配置 RTC 基本参数 (分频后为1Hz)
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;  // (127+1) * (255+1) = 32768 Hz -> 1Hz
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    // 配置并使能 RTC WakeUp 中断
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

/**
 * @brief  进入 STOP 模式，并定时唤醒
 * @param  interval_sec : 休眠时间（单位：秒）
 */
void Enter_Stop_Mode(uint32_t interval_sec)
{
    // 清除唤醒标志
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

    // 设置 RTC 唤醒定时器 (使用 CK_SPRE_16BITS 时钟源，1 tick = 1秒)
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, interval_sec, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
        Error_Handler();
    }

    // 使能 EXTI Line 17 (RTC Wakeup专用唤醒线，非常关键)
    EXTI_D1->IMR1 |= EXTI_IMR1_IM17;
    EXTI->RTSR1 |= EXTI_RTSR1_TR17;

    // 暂停 SysTick (防止滴答定时器提前把单片机吵醒)
    HAL_SuspendTick();

    // 进入 STOP 模式 (WFI 指令)
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* ========================================================= */
    /* ================== 单片机在此处睡着了 =================== */
    /* ========================================================= */
    
    // 唤醒后，第一时间恢复 SysTick
    HAL_ResumeTick();

    // 重新配置系统主时钟 (唤醒后默认使用的是 HSI 内部低速时钟)
    extern void SystemClock_Config(void);
    SystemClock_Config();

    //关闭唤醒定时器，准备下一次使用
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
}

/**
 * @brief RTC 唤醒中断服务函数
 * @note  如果你在 stm32h7xx_it.c 里已经有了这个函数，请把它删掉或者把里面的内容写在那边
 */
void RTC_WKUP_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}

//尝试用结构体返回值来写(主要是状态三用了全局变量)

/**
 * @brief  [提取的子功能]：处理获取历史数据命令，专心做打印和清零动作
 */
static void Process_Get_History(int *history_array, uint16_t *p_count)
{
    if (*p_count == 0) // 单次触发结果（或者最开始什么都没有）
    {
        printf("order0people%dend", history_array[255]);
    }
    else // 持续触发结果
    {
        for (int i = 0; i < *p_count; i++)
        {
            printf("order%dpeople%dend", (i + 2), history_array[i]);
            delay_ms(100); // 防止发送过快导致 ESP8266 缓冲区溢出
        }
        // 清空本地记录：通过指针把外面的 history_count 清零
        *p_count = 0; 
    }
}

/**
 * @brief  等待配置模式
 * @param  history_array : 指向保存识别结果的数组首地址
 * @param  p_count       : 指向当前数据数量的指针（传指针是为了能在函数内把它清零,否则只是形参）
 * @return ConfigResult_t: 返回一个结构体数组（模式、时间间隔、强制更新）
 */
 
ConfigResult_t Enter_Wait_Config_Mode(int *history_array, uint16_t *p_count)
{
	ConfigResult_t res = {MODE_IDLE,0,0};	//初始化返回值结构体
	// 清空之前的串口残留数据
	USART2_RxPacket_Clear();	
	while(1)
	{
		// 检查是否收到上位机发来的新数据（以换行符结尾）
		if (USART2_RxFlag == 1)
		{
			// 状态 0：等待上位机发送 "GET_HISTORYend" 
			if ( strstr((const char*)USART2_RxBuffer, "GET_HISTORYend") != NULL )
			{
				Process_Get_History(history_array, p_count);
			}
			/* 状态 1：等待上位机发送类似 "SET_TIMER_REPEAT:60end" */
			else if (strstr((char*)USART2_RxBuffer, "SET_TIMER_REPEAT:") != NULL)
			{		
				// 使用 sscanf 提取数字
				if (sscanf((char*)USART2_RxBuffer, "SET_TIMER_REPEAT:%d", &res.interval) == 1)
				{
					if (res.interval > 0) 
					{
						res.mode = MODE_CONTINUOUS;				//重复触发
						USART2_RxPacket_Clear(); 					// 退出前清空标志
						return res; 
					}		
				}
			}
			/* 状态 2：等待上位机发送类似 "SET_TIMER_ONCE:60end" */
			else if(strstr((char*)USART2_RxBuffer, "SET_TIMER_ONCE:") != NULL)
			{
				// 使用 sscanf 提取数字
				if (sscanf((char*)USART2_RxBuffer, "SET_TIMER_ONCE:%d", &res.interval) == 1)
				{
					if (res.interval > 0) 
					{
						USART2_RxPacket_Clear(); 			// 退出前清空标志
						res.mode = MODE_SINGLE;				// 单次触发
						return res; 
					}		
				}
			}
			/* 状态 3：等待上位机发送 "DIRECTend" ,直接拍照上传数据*/
			else if(strstr((char*)USART2_RxBuffer, "DIRECTend") != NULL)
			{
					res.mode = MODE_DIRECT;
				  USART2_RxPacket_Clear();				// 退出前清空标志
				  return res;
			}
			/* 状态 4：等待上位机发送 "DIRECTKKK" ,直接拍照上传照片数据*/
			else if(strstr((char*)USART2_RxBuffer, "DIRECTENDKKK") != NULL)
			{
				USART2_RxPacket_Clear();				// 退出前清空标志
				res.mode = MODE_TPHOTO;
				return res;
			}
			// 处理完一帧数据后，清空接收缓冲区准备收下一帧
			USART2_RxPacket_Clear();
		}
		delay_ms(100); // 稍微延时，防止死循环卡死 CPU
	}
}


