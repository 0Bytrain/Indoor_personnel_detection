
/**
  ******************************************************************************
  * @file    app_x-cube-ai.c
  * @author  X-CUBE-AI C code generator
  * @brief   AI program body
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

 /*
  * Description
  *   v1.0 - Minimum template to show how to use the Embedded Client API
  *          model. Only one input and one output is supported. All
  *          memory resources are allocated statically (AI_NETWORK_XX, defines
  *          are used).
  *          Re-target of the printf function is out-of-scope.
  *   v2.0 - add multiple IO and/or multiple heap support
  *
  *   For more information, see the embeded documentation:
  *
  *       [1] %X_CUBE_AI_DIR%/Documentation/index.html
  *
  *   X_CUBE_AI_DIR indicates the location where the X-CUBE-AI pack is installed
  *   typical : C:\Users\[user_name]\STM32Cube\Repository\STMicroelectronics\X-CUBE-AI\7.1.0
  */

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#if defined ( __ICCARM__ )
#elif defined ( __CC_ARM ) || ( __GNUC__ )
#endif

/* System headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "app_x-cube-ai.h"
#include "main.h"
#include "ai_datatypes_defines.h"
#include "network.h"
#include "network_data.h"
#include "demo.h"
#include "mc2640.h"
/* USER CODE BEGIN includes */

#define IMGHW 160 // 图像输入尺寸
#define FEATHW 500 // 特征图尺寸(使用最小特征图5×5)
#define NUM_ANCHOR 1 // 锚点数量(YOLO标准配置)
#define NUM_CLASS 1 // 类别数量
#define MAX_BOXNUM 30// 最大检测框数量
// 使用与Python代码相同的阈值
#define CONF_THRESHOLD 0.12f
#define NMS_IOU 0.2f 
#define MAX(a,b) ((a>b)? a:b)
#define MIN(a,b) ((a<b)? a:b)
/* USER CODE END includes */

/* IO buffers ----------------------------------------------------------------*/

#if !defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_in_1[AI_NETWORK_IN_1_SIZE_BYTES];
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
data_in_1
};
#else
ai_i8* data_ins[AI_NETWORK_IN_NUM] = {
NULL
};
#endif

#if !defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
AI_ALIGNED(4) ai_i8 data_out_1[AI_NETWORK_OUT_1_SIZE_BYTES];
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
data_out_1
};
#else
ai_i8* data_outs[AI_NETWORK_OUT_NUM] = {
NULL
};
#endif

/* Activations buffers -------------------------------------------------------*/

AI_ALIGNED(32)
static uint8_t pool0[AI_NETWORK_DATA_ACTIVATION_1_SIZE];

ai_handle data_activations0[] = {pool0};

/* AI objects ----------------------------------------------------------------*/

static ai_handle network = AI_HANDLE_NULL;

static ai_buffer* ai_input;
static ai_buffer* ai_output;

static void ai_log_err(const ai_error err, const char *fct)
{
  /* USER CODE BEGIN log */
  if (fct)
    printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct,
        err.type, err.code);
  else
    printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type, err.code);

  do {} while (1);
  /* USER CODE END log */
}

static int ai_boostrap(ai_handle *act_addr)
{
  ai_error err;

  /* Create and initialize an instance of the model */
  err = ai_network_create_and_init(&network, act_addr, NULL);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_network_create_and_init");
    return -1;
  }

  ai_input = ai_network_inputs_get(network, NULL);
  ai_output = ai_network_outputs_get(network, NULL);

#if defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-inputs" option is used, memory buffer can be
   *  used from the activations buffer. This is not mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	data_ins[idx] = ai_input[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_IN_NUM; idx++) {
	  ai_input[idx].data = data_ins[idx];
  }
#endif

#if defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
  /*  In the case where "--allocate-outputs" option is used, memory buffer can be
   *  used from the activations buffer. This is no mandatory.
   */
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	data_outs[idx] = ai_output[idx].data;
  }
#else
  for (int idx=0; idx < AI_NETWORK_OUT_NUM; idx++) {
	ai_output[idx].data = data_outs[idx];
  }
#endif

  return 0;
}

static int ai_run(void)
{
  ai_i32 batch;

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_network_get_error(network),
        "ai_network_run");
    return -1;
  }

  return 0;
}

/* USER CODE BEGIN 2 */
extern uint16_t image_buffer[160*160];
AI_ALIGNED(32)
static ai_i8 *in_data = NULL;   // 只是指针，省下 75KB

#include <math.h>  // 用于round函数
int acquire_and_process_data(ai_i8* data[])
{
    float input_scale = 0.003337178146466613f;
    ai_i8 input_zero_point = -128;

    if (in_data == NULL) return -1;   //  防止未初始化就写

    for(int c = 0; c < 3; c++)
    for(int h = 0; h < 160; h++)
    for(int w = 0; w < 160; w++)
    {
        uint16_t color = image_buffer[w + h * 160];

        float normalized_value;
        switch(c) {
            case 0: normalized_value = ((color >> 11) & 0x1F) / 31.0f; break;
            case 1: normalized_value = ((color >>  5) & 0x3F) / 63.0f; break;
            default:normalized_value = ( color        & 0x1F) / 31.0f; break;
        }

        int chw_index = c * 160 * 160 + h * 160 + w;

        float q = normalized_value / input_scale + input_zero_point;
        if (q < -128) q = -128;
        if (q > 127)  q = 127;

        in_data[chw_index] = (ai_i8)roundf(q);
    }

    /* 删掉：不要再改 ai_input[0].data */
    /* ai_input[0].data = AI_HANDLE_PTR(in_data); */

    return 0;
}


//找到前三十大的置信度框
int sort(float boxes[MAX_BOXNUM][NUM_CLASS+4], float box[NUM_CLASS+4])
{
	for (uint16_t i=0; i<MAX_BOXNUM;i++)
	{
		if(box[4]>boxes[i][4])
		{
			for (uint16_t j=MAX_BOXNUM-1; j>i; j--)
			{
				memcpy(boxes[j],boxes[j-1],(NUM_CLASS+4)*4);
			}
			
			memcpy(boxes[i],box,(NUM_CLASS+4)*4);
			return 0;
		}
	}
	return 0;
}

// 计算两个矩形的IoU
float calculate_iou(float box1[4], float box2[4])
{
    float x1 = MAX(box1[0], box2[0]);
    float y1 = MAX(box1[1], box2[1]);
    float x2 = MIN(box1[2], box2[2]);
    float y2 = MIN(box1[3], box2[3]);
    
    float inter_area = MAX(0, x2 - x1) * MAX(0, y2 - y1);
    float box1_area = (box1[2] - box1[0]) * (box1[3] - box1[1]);
    float box2_area = (box2[2] - box2[0]) * (box2[3] - box2[1]);
    
    return inter_area / (box1_area + box2_area - inter_area);
}

// 非极大值抑制 (NMS)
int NMS(float boxes[MAX_BOXNUM][5])
{
    int count = 0;
    
    // 找到所有有效框
    for (int i = 0; i < MAX_BOXNUM; i++)
    {
        if(boxes[i][4] >= CONF_THRESHOLD)
        {
            count++;
        }
        else
        {
            break;
        }
    }
    
    if(count <= 1) return count;
    
    // 按置信度排序（从高到低）
    for(int i = 0; i < count - 1; i++)
    {
        for(int j = i + 1; j < count; j++)
        {
            if(boxes[i][4] < boxes[j][4])
            {
                float temp[5];
                memcpy(temp, boxes[i], 5 * sizeof(float));
                memcpy(boxes[i], boxes[j], 5 * sizeof(float));
                memcpy(boxes[j], temp, 5 * sizeof(float));
            }
        }
    }
    
    // 应用NMS
    for(int i = 0; i < count; i++)
    {
        if(boxes[i][4] < CONF_THRESHOLD) continue;
        
        for(int j = i + 1; j < count; j++)
        {
            if(boxes[j][4] < CONF_THRESHOLD) continue;
            
            float iou = calculate_iou(boxes[i], boxes[j]);
            if(iou > NMS_IOU)
            {
                boxes[j][4] = 0; // 标记为抑制
            }
        }
    }
    
    // 重新排序，将抑制的框移到最后
    int new_count = 0;
    for(int i = 0; i < count; i++)
    {
        if(boxes[i][4] >= CONF_THRESHOLD)
        {
            if(i != new_count)
            {
                memcpy(boxes[new_count], boxes[i], 5 * sizeof(float));
            }
            new_count++;
        }
    }    
    // 将剩余位置清零
    for(int i = new_count; i < MAX_BOXNUM; i++)
    {
        memset(boxes[i], 0, 5 * sizeof(float));
    }
    
    return new_count;
}

int post_process(float boxes[MAX_BOXNUM][5])
{
    float* out_data = (float*)ai_output[0].data;
    
    // 初始化boxes数组
    memset(boxes, 0, MAX_BOXNUM * 5 * sizeof(float));
    
    // 计数器，用于统计检测到的有效框数量
    int detected_count = 0;
		delay_ms(500);
//    printf("[FACES_START]");
		delay_ms(500);
  // 处理500个预测
    for(int i = 0; i < 500; i++)
    {
        // 按照属性顺序读取数据（需要转置）
        // 前500个是x_center，接着500个是y_center，然后是width、height，接着500个是confidence1（彩图），最后500个是confidence2（黑白图）
        float x_center = out_data[i];
        float y_center = out_data[i + 500];
        float width = out_data[i + 1000];
        float height = out_data[i + 1500];
        float confidence1 = out_data[i + 2000];  // 彩图图置信度
        float confidence2 = out_data[i + 2500];  // 黑白置信度
        
        // 选择最大的置信度作为最终置信度
        float confidence = (confidence1 > confidence2) ? confidence1 : confidence2;
//        
        // 可选：打印两个置信度用于调试
//         printf("Box %d: conf1=%.4f, conf2=%.4f, final_conf=%.4f\n", 
//                i, confidence1, confidence2, confidence);
        
        if(confidence > CONF_THRESHOLD)
        {
            // 转换为左上角坐标
            float x1 = x_center - width / 2;
            float y1 = y_center - height / 2;
            
            // 计算右下角坐标
            float x2 = x1 + width;
            float y2 = y1 + height;
            
            // 边界检查
            if(x1 < 0) x1 = 0;
            if(y1 < 0) y1 = 0;
            if(x2 > IMGHW-1) x2 = IMGHW-1;
            if(y2 > IMGHW-1) y2 = IMGHW-1;         	
//						printf("x1=%.2f, y1=%.2f, x2=%.2f, y2=%.2f, conf=%.4f\n\r",
//                    x1, y1, x2, y2, confidence);
            
            // 存储检测结果 [x1, y1, x2, y2, confidence]
            float box[5] = {x1, y1, x2, y2, confidence};
            sort(boxes, box);
            
            // 增加检测计数器
            detected_count++;
        }
		
				
    }
//		if(detected_count==0){
//        printf("x1=0.00, y1=0.00, x2=0.00, y2=0.00, conf=0.0000\n\r");
//    }
    
//    // 打印总检测数量
//    printf("Total detected boxes: %d\n\r", detected_count);
    
    return 0;
}
/* USER CODE END 2 */

/* Entry points --------------------------------------------------------------*/

void MX_X_CUBE_AI_Init(void)
{
    /* USER CODE BEGIN 5 */
//  printf("\r\nTEMPLATE - initialization\r\n");

  ai_boostrap(data_activations0);
	in_data = (ai_i8*)data_ins[0];
    /* USER CODE END 5 */
}


int MX_X_CUBE_AI_Process(void)
{
    int res = -1;
    //float boxes[MAX_BOXNUM][5] = {0};  // 每个框有5个值: x1, y1, x2, y2, confidence
    static float boxes[MAX_BOXNUM][5];  // 使用静态存储 
//    printf("TEMPLATE - run - main loop\r\n");
    
    if (network) 
			{
        // 1 - 获取并预处理输入数据
        res = acquire_and_process_data(data_ins);
        // 2 - 处理数据 - 调用推理引擎
        if (res == 0)
            res = ai_run(); 
        // 3- 后处理预测结果
        if (res == 0)
            res = post_process(boxes);
				// 应用NMS
        uint16_t Face_count = NMS(boxes);
        return (int)Face_count;
    } 
    if (res) 
		{
        ai_error err = {AI_ERROR_INVALID_STATE, AI_ERROR_CODE_NETWORK};
        ai_log_err(err, "Process has FAILED");
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
