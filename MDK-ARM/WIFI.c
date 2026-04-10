#include "stm32h7xx_it.h"                 // Device header
#include <stdio.h>
#include <stdarg.h>
#include "string.h"
#include "demo.h"
#include "mc2640.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "stm32h743xx.h"
#include "WIFI.h"
#include "wait_mode.h"
#include "app_x-cube-ai.h"
#include <stdlib.h>  // 用于sprintf等函数
/*这段为uart配置，与WIFi无关*/
uint8_t USART2_RxBuffer[USART2_RX_BUFFER_SIZE];
volatile uint16_t USART2_RxIndex;
volatile uint8_t USART2_RxFlag;
// 全局变量定义
uint8_t usart2_rx_byte;
/**
* @brief	串口缓存清空函数
*/
void USART2_RxPacket_Clear(void)
{
    USART2_RxIndex = 0;
    USART2_RxFlag = 0;
    memset(USART2_RxBuffer, 0, USART2_RX_BUFFER_SIZE);
}

/**
* @brief	接收完成回调函数
* @note		收到\n表示收到一个命令，改变标志位表示接收完成
*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) 
    {
        uint8_t received_data = usart2_rx_byte;
        
        // 缓冲区保护
        if(USART2_RxIndex < USART2_RX_BUFFER_SIZE - 1)
        {
            USART2_RxBuffer[USART2_RxIndex++] = received_data;    
            // 只要收到完整行就检查
            if(received_data == '\n')
            {
                USART2_RxFlag = 1;  // 设置标志，让主程序处理
            }
        }
        else
        {
            // 缓冲区满，设置标志
            USART2_RxFlag = 1;
        }
        // 重新启动接收中断
        HAL_UART_Receive_IT(&huart2, &usart2_rx_byte, 1);
    }
}
/**
*	@brief	WIFI传图函数
* @param	WIFIConfig：文件"wait_mode.h"里的模式结构体
* @param	img_buf		：照片数据
* @param	Result		: 识别结果（main.c里定义了）
* @note		通过握手的方式使图传更加稳定
*/
void ESP8266_TPhoto(ConfigResult_t * WIFIConfig,uint16_t *img_buf,int *Result)
{
		  /*拍照初始化和拍摄*/
			demo_run();
			// 每一包包含 256 个像素 (256像素 * 4字节/像素 = 1024 字节的数据体)
			const uint32_t PIXELS_PER_PACKET = 256; 
			uint32_t i = 0;
			while (i < PIXELS) 
			{
				// 发送包头
				printf("image:");
				// 发送最高 1024 字节的数据体
				uint32_t current_chunk_pixels = 0;
				while (i < PIXELS && current_chunk_pixels < PIXELS_PER_PACKET) 
				{
					  /*此处传入指针*/
						uint16_t pixel = img_buf[i];
						printf("%02X%02X", (pixel >> 8) & 0xFF, pixel & 0xFF);
						i++;
						current_chunk_pixels++;
				}
				// 发送包尾
				printf("end");
				// 等待 ESP-01s 回传 "RCEOK" (带超时重传机制)
				uint32_t timeout = 0;
				uint8_t ack_received = 0;
				while (timeout < 3000) // 设置 3000ms 的超时时间
				{
					// 检查是否收到上位机发来的新数据（以换行符结尾）
					if (USART2_RxFlag == 1)
					{
						if ( strstr((const char*)USART2_RxBuffer, "RCEOK") != NULL )
						{
							ack_received = 1;
							USART2_RxPacket_Clear(); // 清空接收缓存，为下一包做准备
							break;
						}
					}
					delay_ms(1);
					timeout++;
				}
				// 超时处理逻辑
				if (!ack_received) 
				{
					// 3秒内没收到 RCEOK，说明丢包了
					WIFIConfig->mode = MODE_IDLE; //恢复空闲模式
					break;													 //退出传输直接进入等待区
				}
			}
			/*单次的人数结果*/
			*Result = MX_X_CUBE_AI_Process();
			printf("order0people%dend", *Result); 
			WIFIConfig->mode = MODE_IDLE; //恢复空闲模式
}
/****************************************************************************************************************/
/*下面是WIFI的AT指令配置函数*/
/**
  * @brief  ESP8266_STA_客户端模式连接WIFI、TCP服务器
  */
void ESP8266_STA_Init(void)
{

    //printf("1. 测试esp8266是否存在...\r\n");
    while(ESP8266_At_Test() == ESP8266_ERROR)
    {
        delay_ms(100);
    }
        
    //printf("2. 设置工作模式为STA...\r\n");
    while(ESP8266_Set_Mode(ESP8266_STA_MODE) == ESP8266_ERROR)
    {
        delay_ms(100);
    }
        
//    //printf("3. 设置单路链接模式...\r\n");
    while(ESP8266_Connection_Mode(ESP8266_SINGLE_CONNECTION) == ESP8266_ERROR)
    {
        delay_ms(500);
    }
        
    //printf("4. 连接wifi，SSID: %s, PWD: %s\r\n", WIFI_SSID, WIFI_PWD);
    while(ESP8266_Join_Ap(WIFI_SSID, WIFI_PWD) == ESP8266_ERROR)
    {
        delay_ms(1000);
    }
      
    //printf("5. 连接TCP服务器，server_ip:%s, server_port:%s\r\n", TCP_SERVER_IP, TCP_SERVER_PORT);
    while(ESP8266_Connect_Tcpserver(TCP_SERVER_IP, TCP_SERVER_PORT)== ESP8266_ERROR)
		{
        delay_ms(1000);
		}
    
    //printf("6. 进入到透传模式...\r\n");
    while(ESP8266_Enter_Unvarnished()== ESP8266_ERROR)
		{
        delay_ms(1000);
		}
    
    //printf("ESP8266已连接上TCP服务器并进入透传模式\r\n");
    //printf("ESP8266初始化完成！\r\n");
 
}
 
/**
  * @brief  ESP8266_AP_服务端模式
  */
void ESP8266_AP_Init(void)
{
    //printf("esp8266初始化开始...\r\n");
    
    //esp8266的其它初始化
    //printf("1. 测试esp8266是否存在...\r\n");
    while(ESP8266_At_Test() == ESP8266_ERROR)
    {
        delay_ms(100);
    }
    
    //printf("2. 设置工作模式为AP...\r\n");
    while(ESP8266_Set_Mode(ESP8266_AP_MODE) == ESP8266_ERROR)
    {
        delay_ms(100);
    }
 
    //printf("3. 设置wifi账号密码...\r\n");
    while(ESP8266_Set_WiFi() == ESP8266_ERROR)
    {
        delay_ms(100);
    }
    
    //printf("4. 设置多路链接模式...\r\n");
    while(ESP8266_Connection_Mode(ESP8266_MULTI_CONNECTION) == ESP8266_ERROR)
    {
        delay_ms(100);
    }
    
    //printf("5. 建立TCP服务器...\r\n");
    while(ESP8266_Build_Tcp_Server() == ESP8266_ERROR)
    {
        delay_ms(100);
    }
    
    
    //printf("ESP8266初始化完成！\r\n");
}
 


// 修改后的ESP8266发送命令函数
uint8_t ESP8266_Send_Command(char *Cmd, char *Res)
{
    uint16_t time_out = 250;
    USART2_RxPacket_Clear();  // 清空接收缓冲区
    // 使用printf发送命令，注意添加换行符
    printf("%s", Cmd);
    while(time_out--)
    {
        // 检查是否接收到完整的数据帧
        if(USART2_RxFlag)
        {
            // 在接收到的数据中查找期望的响应
            if(strstr((const char*)USART2_RxBuffer, Res) != NULL)
            {
                return ESP8266_EOK;
            }
            USART2_RxFlag = 0;  // 清除接收标志
        }
        delay_ms(10);
    }
    
    return ESP8266_ERROR;
}

// 获取WIFI模块返回的数据
char* ESP8266_GetResponse(void)
{
    return (char*)USART2_RxBuffer;
}

// 检查是否有新数据到达
uint8_t ESP8266_DataReady(void)
{
    return USART2_RxFlag;
}
/**
  * @brief  测试ESP8266是否存在
  * @param  无
  * @retval 存在返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_At_Test(void)
{
    return ESP8266_Send_Command("AT\r\n", "OK");
}
 
/**
  * @brief  设置工作模式
  * @param  三种工作模式
  * @retval 成功设置返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Set_Mode(uint8_t mode)
{
    char cmd[64];
    sprintf(cmd, "AT+CWMODE=%d\r\n", mode);
    
    return ESP8266_Send_Command(cmd, "OK");
}
 
/**
  * @brief  设置单路链接模式
  * @param  两种模式
  * @retval 成功设置返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Connection_Mode(uint8_t mode)
{
    char cmd[64];
    sprintf(cmd, "AT+CIPMUX=%d\r\n", mode);
    
    return ESP8266_Send_Command(cmd,"OK");
}
 
/**
  * @brief  连接wifi
  * @param  账号和密码
  * @retval 成功连接返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Join_Ap(char *ssid, char *pwd)
{
    char cmd[64];
	  uint8_t ret;
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
	//由于ESP8266有时候返回OK有时候返回CONNECTED故两者都要检验
    ret = ESP8266_Send_Command(cmd, "OK");
	  if(ret == ESP8266_EOK)
		{
			return ret;
		}
		else 
		{
      return ESP8266_Send_Command(cmd, "CONNECT");
		}
}
 
/**
  * @brief  连接TCP服务器
  * @param  server_ip：TCP地址
  * @param  server_port：服务器端口号
  * @retval 成功连接返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Connect_Tcpserver(char *server_ip, char *server_port)
{
    char cmd[64];
		uint8_t ret;
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", server_ip, server_port);
    	//由于ESP8266有时候返回OK有时候返回CONNECTED故两者都要检验
    ret = ESP8266_Send_Command(cmd, "OK");
	  if(ret == ESP8266_EOK)
		{
			return ret;
		}
		else 
		{
      return ESP8266_Send_Command(cmd, "CONNECT");
		}
}
 
/**
  * @brief  进入透传模式
  * @param  无
  * @retval 成功连接返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Enter_Unvarnished(void)
{
    uint8_t ret;
    ret = ESP8266_Send_Command("AT+CIPMODE=1\r\n", "OK");
    ret += ESP8266_Send_Command("AT+CIPSEND\r\n", "OK");
    if (ret == ESP8266_EOK)
        return ESP8266_EOK;
    else
        return ESP8266_ERROR;
}
//传输大量数据

/**
  * @brief  设置WiFi
  * @param  
  * @retval 成功返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Set_WiFi(void)
{
    return ESP8266_Send_Command("AT+CWSAP=\"ESP8266\",\"12345678\",1,4\r\n","OK");
}
 
/**
  * @brief  建立TCP服务器
  * @param  
  * @retval 成功返回UART_EOK，否则UART_ERROR
  */
uint8_t ESP8266_Build_Tcp_Server(void)
{
    return ESP8266_Send_Command("AT+CIPSERVER=1,8080\r\n", "OK");
}
//退出传透模式
void ESP8266_Exit(void)
{
    printf("+++");
}

//断开连接
uint8_t ESP8266_CWQAP(void)
{
    return ESP8266_Send_Command("AT+CWQAP\r\n", "OK");
}
