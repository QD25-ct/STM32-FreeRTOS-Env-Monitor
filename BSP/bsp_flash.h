#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "stm32f10x.h"

#define FLASH_BASE_ADDR     ((uint32_t)0x0800FC00)  // 最后一个扇区起始地址

// 偏移地址（避免互相覆盖）
#define FLASH_OFFSET_TH     0x0000   // 阈值存储偏移（2字节）
#define FLASH_OFFSET_BOOT   0x0004   // 启动计数偏移（4字节）

void Flash_Init(void);
uint16_t Flash_Read_HalfWord(uint32_t addr);
void Flash_Write_HalfWord(uint32_t addr, uint16_t data);
void Flash_Erase_Page(void);

// 应用接口
int16_t Flash_Get_Threshold(void);
void Flash_Set_Threshold(int16_t val);
uint32_t Flash_Get_BootCount(void);
void Flash_Increment_BootCount(void);

#endif
