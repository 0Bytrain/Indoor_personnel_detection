/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dcmi.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "app_x-cube-ai.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mc2640.h"
#include "mc2640_sccb.h"
#include "demo.h"
#include "WIFI.h"
#include "stm32h7xx_it.h"                 // Device header
#include <stdio.h>
#include <stdarg.h>
#include "string.h"
#include "wait_mode.h"
#include "stm32h743xx.h"
#include "usart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*定义一个拍照并且处理数据的函数*/
int Capture_Process_Faces(void)
{
	int res = 0;
	demo_run();
	res = MX_X_CUBE_AI_Process();
	return res;
}

/* USER CODE BEGIN 0 */

// 定义一个全局标志位，用来记录是否是按键唤醒的
volatile uint8_t button_wakeup_flag = 0; 

// 重写外部中断回调函数 (刚刚 gpio.c 里的 EXTI0_IRQHandler 会自动调用它)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) 
    {
        button_wakeup_flag = 1; // 标记：按键把叫醒的！
    }
}

/* USER CODE END 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
	Manual_Button_EXTI_Init();
  MX_DMA_Init();
  MX_DCMI_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_X_CUBE_AI_Init();
	System_RTC_Init();
  /* USER CODE BEGIN 2 */ 
	
	extern uint8_t usart2_rx_byte;
	extern uint16_t image_buffer[PIXELS];	//图像储存区
	extern uint8_t USART2_RxBuffer[USART2_RX_BUFFER_SIZE];
  extern volatile uint8_t USART2_RxFlag;
	
	int result = 0;		//人数结果
	uint16_t history_count =  0;		//本地数组索引
  int history_faces[256] = {0};		//本地储存人数信息数组(重复定时)	// history_faces[255](单次定时)
	uint32_t last_capture_tick = 0; //用于记录定时器打卡
	
	ConfigResult_t current_config = {MODE_IDLE,0,0};	//结构体初始化
	HAL_UART_Receive_IT(&huart2, &usart2_rx_byte, 1);	//接收初始化
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {	
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
		/*空闲模式*/
		if(current_config.mode == MODE_IDLE)
		{
			current_config = Enter_Wait_Config_Mode(history_faces, &history_count);
		}
		else if(current_config.mode == MODE_TPHOTO)
		{
			/*用了指针写参数时记得用取地址符&*/
			//数组image_buffer本身就是地址所以不用&
			ESP8266_TPhoto(&current_config,image_buffer,&result);
		}
		else if(current_config.mode == MODE_DIRECT)
		{
			/*单次的人数结果*/
			result = Capture_Process_Faces();
			printf("order0people%dend",result);
			current_config.mode = MODE_IDLE;	//恢复空闲模式
		}
		else if(current_config.mode == MODE_CONTINUOUS || current_config.mode == MODE_SINGLE)
		{
			  // 停机前先把按键标志位清零，防止受之前误触的影响
        button_wakeup_flag = 0;
			  /*直接休眠interval秒*/
				Enter_Stop_Mode(current_config.interval);
			  if(button_wakeup_flag == 1)
				{
					/*按键唤醒直接进入IDLE模式*/
					current_config.mode = MODE_IDLE;
					/*直接重新开始循环*/
					continue;
				}
				/*单次的人数结果*/
				result = Capture_Process_Faces();
				/*存本地操作*/
				if (current_config.mode == MODE_CONTINUOUS)
				{
					if (history_count < 255) 
					{
						history_faces[history_count++] = result;
					}
					// 模拟按钮：存够2个回配置模式
					if (history_count >= 2) 
					{
							current_config.mode = MODE_IDLE;
					}
					//若是不够三次就会从while（1）开始发现还是这个模式接着来休眠。
				}
				else if (current_config.mode == MODE_SINGLE) // 单次模式
				{
					history_faces[255] = result;
					current_config.mode = MODE_IDLE; // 执行完一次就回去
				}
		}
		
  }
	/* USER CODE END WHILE */	
	
	
  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 20;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
//  while (1)
//  {
//  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
