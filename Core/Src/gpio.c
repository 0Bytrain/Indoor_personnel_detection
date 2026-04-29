/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PC14-OSC32_IN (OSC32_IN)   ------> RCC_OSC32_IN
     PC15-OSC32_OUT (OSC32_OUT)   ------> RCC_OSC32_OUT
     PH0-OSC_IN (PH0)   ------> RCC_OSC_IN
     PH1-OSC_OUT (PH1)   ------> RCC_OSC_OUT
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, DCMI_RESET_Pin|DCMI_PWDN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SDA_Pin|SCL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : DCMI_RESET_Pin DCMI_PWDN_Pin */
  GPIO_InitStruct.Pin = DCMI_RESET_Pin|DCMI_PWDN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : SDA_Pin SCL_Pin */
  GPIO_InitStruct.Pin = SDA_Pin|SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/**
 * @brief 纯手写配置 PA0 为外部按键中断 (唤醒停机模式 - 高电平唤醒版)
 * @note  【硬件要求修改】：按键一端接 PA0，另一端必须接 3.3V (VCC)
 */
void Manual_Button_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 开启 GPIOA 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 配置 PA0 引脚为外部中断模式 */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    
    // 【修改点 1】: 选择上升沿触发中断 (即按下按键，电平从 0V 变到 3.3V 的瞬间触发)
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING; 
    
    // 【修改点 2】: 开启内部下拉电阻 (保证松开按键时 PA0 是稳定的低电平 0V)
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;          
    
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置 NVIC 中断控制器 */
    // 设置中断优先级为 2 
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    // 使能 EXTI0 中断通道
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
 * @brief EXTI0 中断服务函数 (专门处理 PA0 触发的中断)
 * @note  把中断函数写在 USER CODE 里面是最安全的，CubeMX 绝对删不掉
 */
void EXTI0_IRQHandler(void)
{
    // 调用 HAL 库的外部中断通用处理函数
    // 它会自动清除底层的中断标志位（官方库里的 HAL_GPIO_EXTI_Callback会负责）
	  // 并跳转去执行在 main.c 里写的 HAL_GPIO_EXTI_Callback
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/* USER CODE END 2 */
