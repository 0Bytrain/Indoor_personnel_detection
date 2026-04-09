#ifndef __WAIT_H
#define __WAIT_H
#include "main.h" 

typedef enum{
	MODE_IDLE = 0,		//这里手动写上其实是显示表达，不写也是默认为0
	MODE_DIRECT,			//1
	MODE_CONTINUOUS,	//2
	MODE_SINGLE,			//3
	MODE_TPHOTO				//4
}	WorkMode_t;
typedef struct {
	WorkMode_t mode;	//当前模式
	uint32_t interval;//时间间隔
	uint8_t force_update;
} ConfigResult_t;		//配置结果的类型（命名时加_t一般就是类型）

ConfigResult_t Enter_Wait_Config_Mode(int *history_array, uint16_t *p_count);
// 新增的停机模式与RTC配置函数
void System_RTC_Init(void);
void Enter_Stop_Mode(uint32_t interval_sec);
#endif
