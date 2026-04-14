#ifndef __ESP8266_H
#define __ESP8266_H
 
#include "main.h" 
#include "wait_mode.h"
#define USART2_RX_BUFFER_SIZE 256

extern uint8_t USART2_RxBuffer[USART2_RX_BUFFER_SIZE];

//本C文件的返回值
#define ESP8266_EOK                 0
#define ESP8266_ERROR               1
 
//工作模式
#define ESP8266_STA_MODE            1
#define ESP8266_AP_MODE             2
#define ESP8266_STA_AP_MODE         3
 
//单路链接模式
#define ESP8266_SINGLE_CONNECTION   0
#define ESP8266_MULTI_CONNECTION    1
 
//WIFI名称和密码
#define WIFI_SSID                   "HONOR 90"
#define WIFI_PWD                    "11111111"
 
//TCP服务器地址和端口
#define TCP_SERVER_IP               "192.168.60.17"
#define TCP_SERVER_PORT             "8080"

void USART2_RxPacket_Clear(void);
void USART2_Start_Receive(void);
void USART2_ReceiveHandler(uint8_t receivedData);
void ESP8266_TPhoto(ConfigResult_t * WIFIConfig,uint16_t *img_buf,int *Result);//传输图片函数

char* ESP8266_GetResponse(void);
uint8_t ESP8266_DataReady(void);
uint8_t ESP8266_Send_Command(char *Cmd, char *Res);

/*ESP8266的STA配置函数*/
void ESP8266_STA_Init(void);
uint8_t ESP8266_At_Test(void);                                              //1. 测试esp8266是否存在
uint8_t ESP8266_Set_Mode(uint8_t mode);                                     //2. 设置工作模式为STA
uint8_t ESP8266_Connection_Mode(uint8_t mode);                              //3. 设置单路链接模式
uint8_t ESP8266_Join_Ap(char *ssid, char *pwd);                             //4. 连接wifi，SSID: %s, PWD: %s
uint8_t ESP8266_Connect_Tcpserver(char *server_ip, char *server_port);      //5. 连接TCP服务器，server_ip:%s, server_port:%s
uint8_t ESP8266_Enter_Unvarnished(void);                                    //6. 进入到透传模式
 
/*ESP8266的AP配置函数*/
uint8_t ESP8266_Set_WiFi(void);
uint8_t ESP8266_Build_Tcp_Server(void);
void ESP8266_Printf(char *format, ...);

 
void ESP8266_AP_Init(void);
void ESP8266_AP_Send_String(char *string);
 
#endif
