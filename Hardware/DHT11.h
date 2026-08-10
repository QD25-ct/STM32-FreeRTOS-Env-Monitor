#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

// DHT11引脚定义 PA0，可自行修改
#define DHT11_PIN    GPIO_Pin_0
#define DHT11_PORT   GPIOA
#define DHT11_RCC    RCC_APB2Periph_GPIOA

// 数据结构体：温度、湿度
typedef struct
{
	uint8_t humi_int;   // 湿度整数
	uint8_t humi_dec;   // 湿度小数(DHT11永远0)
	uint8_t temp_int;   // 温度整数
	uint8_t temp_dec;   // 温度小数(DHT11永远0)
	uint8_t check_sum;  // 校验和
} DHT11_Data;

// 函数声明
void DHT11_GPIO_OUT(void);   // 引脚设为输出
void DHT11_GPIO_IN(void);    // 引脚设为输入
void DHT11_Init(void);
uint8_t DHT11_ReadByte(void);
uint8_t DHT11_ReadData(DHT11_Data *data);

#endif
