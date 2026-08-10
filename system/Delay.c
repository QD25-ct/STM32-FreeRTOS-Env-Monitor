#include "stm32f10x.h"
#include "Delay.h"

/**
  * @brief  TIM2 硬件初始化，作为 1us 递增的计数器
  * @note   72MHz 主频下，APB1 总线为 36MHz，但 TIM2 挂在 APB1 上会得到 2 倍频，即 72MHz。
  *         因此预分频器设为 72-1 = 71，计数器每 1us 加 1。
  *         此延时函数不依赖任何中断，纯硬件计数，可与 FreeRTOS 完美共存。
  */
void Delay_Init(void)
{
    // 1. 开启 TIM2 时钟（挂在 APB1 上）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 2. 配置 TIM2 基础参数
    TIM_TimeBaseInitTypeDef TIM_InitStruct;
    TIM_InitStruct.TIM_Prescaler = 71;          // 72MHz / 72 = 1MHz，即每 tick = 1us
    TIM_InitStruct.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
    TIM_InitStruct.TIM_Period = 0xFFFF;         // 最大计数值 65535（16位溢出）
    TIM_InitStruct.TIM_ClockDivision = 0;       // 时钟不分频
    // 注意：STM32F1 标准库的 TIM_TimeBaseInitTypeDef 没有 RepetitionCounter 成员，直接略过
    TIM_TimeBaseInit(TIM2, &TIM_InitStruct);

    // 3. 启动 TIM2 计数器（不开启任何中断，只读它的计数值）
    TIM_Cmd(TIM2, ENABLE);
}

/**
  * @brief  微秒级延时（最大支持 65535us，即 65ms）
  * @param  nus 延时的微秒数（0~65535）
  * @retval 无
  * @note   由于 DHT11 读取中最长的延时为 20ms（即 20000us），完全在安全范围内。
  *         内部已处理 TIM2 的 16位计数器溢出情况。
  */
void Delay_us(uint32_t nus)
{
    // 防止用户传入 0 导致死等
    if (nus == 0) return;

    uint16_t u16Start = TIM_GetCounter(TIM2);
    uint16_t u16End = u16Start + (uint16_t)nus;

    // 如果未发生溢出（u16End > u16Start）
    if (u16End > u16Start)
    {
        // 一直等到计数器值 >= u16End
        while (TIM_GetCounter(TIM2) < u16End);
    }
    else
    {
        // 发生了 16位溢出（例如从 65530 加 100 变成 94）
        // 第一步：等计数器从当前值一直加到 0xFFFF（即溢出）
        while (TIM_GetCounter(TIM2) >= u16Start);
        // 第二步：等计数器从 0 一直加到 u16End
        while (TIM_GetCounter(TIM2) < u16End);
    }
}

/**
  * @brief  毫秒级延时
  * @param  nms 延时的毫秒数（0~4294967295）
  * @retval 无
  * @note   循环调用 Delay_us(1000) 实现，每 1ms 检查一次。
  *         虽然在裸机下是阻塞延时，但后期引入 FreeRTOS 后，此函数仅供 DHT11 初始化时短暂使用，
  *         不会影响系统实时性。
  */
void Delay_ms(uint32_t nms)
{
    while (nms--)
    {
        Delay_us(1000);
    }
}

/**
  * @brief  秒级延时
  * @param  ns 延时的秒数
  * @retval 无
  */
void Delay_s(uint32_t ns)
{
    while (ns--)
    {
        Delay_ms(1000);
    }
}
