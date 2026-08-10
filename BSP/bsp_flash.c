#include "bsp_flash.h"

// 初始化（空函数，保持接口一致）
void Flash_Init(void) { }

// 读取半字（16位）
uint16_t Flash_Read_HalfWord(uint32_t addr)
{
    return *(volatile uint16_t*)addr;
}

// 擦除整个扇区（1KB）
void Flash_Erase_Page(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    FLASH_ErasePage(FLASH_BASE_ADDR);
    FLASH_Lock();
}

// 写入半字
void Flash_Write_HalfWord(uint32_t addr, uint16_t data)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    FLASH_ProgramHalfWord(addr, data);
    FLASH_Lock();
}

// -------- 应用层接口 --------

// 获取温度阈值（默认30℃）
int16_t Flash_Get_Threshold(void)
{
    uint16_t val = Flash_Read_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_TH);
    int16_t val_signed = (int16_t)val;  // 转为有符号数再比较，消除警告

    // 如果是未擦除状态(0xFFFF)或超出合理范围(-20~80)，返回默认30
    if(val == 0xFFFF || val_signed > 80 || val_signed < -20) {
        return 30;
    }
    return val_signed;
}

// 存储温度阈值
void Flash_Set_Threshold(int16_t val)
{
    // 先擦除整页（会清除所有数据）
    Flash_Erase_Page();

    // 写入新阈值
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_TH, (uint16_t)val);

    // 恢复启动计数（因为擦除了整页，必须重写）
    uint32_t boot = Flash_Get_BootCount();
    if(boot == 0xFFFFFFFF) boot = 0;
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT, (uint16_t)(boot & 0xFFFF));
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT + 2, (uint16_t)(boot >> 16));
}

// 获取启动次数（首次上电返回0）
uint32_t Flash_Get_BootCount(void)
{
    uint32_t low = Flash_Read_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT);
    uint32_t high = Flash_Read_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT + 2);
    if(low == 0xFFFF && high == 0xFFFF) return 0;
    return (high << 16) | low;
}

// 启动计数+1并保存
void Flash_Increment_BootCount(void)
{
    uint32_t count = Flash_Get_BootCount() + 1;
    int16_t th = Flash_Get_Threshold();

    // 擦除整页
    Flash_Erase_Page();

    // 重新写入阈值
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_TH, (uint16_t)th);

    // 写入新的启动计数（低16位 + 高16位）
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT, (uint16_t)(count & 0xFFFF));
    Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT + 2, (uint16_t)(count >> 16));
}
