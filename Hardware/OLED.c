#include "stm32f10x.h"
#include "OLED_Font.h"
#include "Delay.h"

/*引脚定义 PB8=SCL PB9=SDA*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_8, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_9, (BitAction)(x))
#define OLED_R_SDA()		GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)

/*IO初始化 开漏输出*/
void OLED_I2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

// I2C等待应答函数
static uint8_t OLED_I2C_WaitAck(void)
{
	uint8_t ack = 1;
	OLED_W_SDA(1);
	Delay_us(10);
	OLED_W_SCL(1);
	Delay_us(10);
	if(OLED_R_SDA() == 0)
	{
		ack = 0;
	}
	OLED_W_SCL(0);
	return ack;
}

// I2C起始信号
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	Delay_us(10);
	OLED_W_SDA(0);
	Delay_us(10);
	OLED_W_SCL(0);
	Delay_us(10);
}

// I2C停止信号
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	Delay_us(10);
	OLED_W_SCL(1);
	Delay_us(10);
	OLED_W_SDA(1);
	Delay_us(10);
}

// I2C发送单字节
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		Delay_us(10);
		OLED_W_SCL(1);
		Delay_us(10);
		OLED_W_SCL(0);
		Delay_us(10);
	}
	OLED_I2C_WaitAck(); // 发送完毕等待应答
}

// 写OLED命令 地址0x78适配你的屏幕
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x00);
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

// 写OLED显示数据 地址0x78适配你的屏幕
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x40);
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

// 设置光标页Y(0~7) 列X(0~127)
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
	OLED_WriteCommand(0x00 | (X & 0x0F));
}

// 硬件整屏清屏
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

// 显示单个8*16字符 Line1~4 Col1~16
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
	}
}

// 显示字符串
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

// 次方工具函数
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

// 无符号十进制数字
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

// 带符号十进制数字
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

// 十六进制数字
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

// 二进制数字
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

// OLED初始化
void OLED_Init(void)
{
	Delay_ms(200);	// 上电稳定延时
	
	OLED_I2C_Init();
	
	OLED_WriteCommand(0xAE);	// 关闭显示
	
	OLED_WriteCommand(0xD5);	// 时钟分频
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	// 多路复用
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	// 显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	// 显示起始行
	
	OLED_WriteCommand(0xA1);	// 左右正常
	OLED_WriteCommand(0xC8);	// 上下正常
	
	OLED_WriteCommand(0xDA);	// COM配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	// 对比度
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);	// 预充电周期
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);	// VCOMH
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);	// 跟随显存
	OLED_WriteCommand(0xA6);	// 正常显示
	OLED_WriteCommand(0x8D);	// 开启电荷泵
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);	// 打开显示
		
	OLED_Clear();
}
