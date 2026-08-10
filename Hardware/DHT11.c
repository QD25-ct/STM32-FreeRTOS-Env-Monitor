#include "DHT11.h"
#include "Delay.h"

// 配置引脚为推挽输出
void DHT11_GPIO_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// 配置引脚为上拉输入
void DHT11_GPIO_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Pin = DHT11_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// DHT11初始化
void DHT11_Init(void)
{
	RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
	DHT11_GPIO_OUT();
	GPIO_WriteBit(DHT11_PORT, DHT11_PIN, Bit_SET); // 默认高电平
}

// 读取一个字节
uint8_t DHT11_ReadByte(void)
{
	uint8_t byte = 0;
	uint8_t i;
	for(i = 0; i < 8; i++)
	{
		// 等待低电平起始
		while(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0);
		Delay_us(50);
		if(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN))
		{
			byte |= (1 << (7 - i));
		}
		// 等待高电平结束
		while(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 1);
	}
	return byte;
}

// 读取温湿度，返回1校验成功，0失败
uint8_t DHT11_ReadData(DHT11_Data *data)
{
	uint8_t buf[5];
	uint8_t sum;

	// 主机拉低18ms发起握手
	DHT11_GPIO_OUT();
	GPIO_WriteBit(DHT11_PORT, DHT11_PIN, Bit_RESET);
	Delay_ms(20);
	GPIO_WriteBit(DHT11_PORT, DHT11_PIN, Bit_SET);
	Delay_us(50);

	// 切换输入等待DHT11应答
	DHT11_GPIO_IN();
	if(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0)
	{
		// 等待DHT11拉高80us
		while(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 0);
		while(GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == 1);

		// 读取5字节数据
		buf[0] = DHT11_ReadByte(); // 湿度整数
		buf[1] = DHT11_ReadByte(); // 湿度小数
		buf[2] = DHT11_ReadByte(); // 温度整数
		buf[3] = DHT11_ReadByte(); // 温度小数
		buf[4] = DHT11_ReadByte(); // 校验和

		sum = buf[0] + buf[1] + buf[2] + buf[3];
		if(sum == buf[4])
		{
			data->humi_int = buf[0];
			data->humi_dec = buf[1];
			data->temp_int = buf[2];
			data->temp_dec = buf[3];
			data->check_sum = buf[4];
			return 1; // 读取成功
		}
	}
	return 0; // 读取失败
}
