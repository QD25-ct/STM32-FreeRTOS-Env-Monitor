/************************************
 * 工程名称：基于FreeRTOS的微型环境监控终端
 * 硬件平台：STM32F103C8T6
 * 功能：温湿度采集 + 多任务调度 + Flash掉电记忆
 * 作者：你的名字
 * 日期：2025-xx-xx
 ************************************/

#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "DHT11.h"
#include "bsp_flash.h"

/* FreeRTOS 头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*-----------------------------------------------------------
 * 1. 硬件引脚宏定义（板载LED，默认为PC13）
 *----------------------------------------------------------*/
#define LED_PIN         GPIO_Pin_13
#define LED_PORT        GPIOC
#define LED_RCC         RCC_APB2Periph_GPIOC

/*-----------------------------------------------------------
 * 2. 任务间通信的数据结构（消息队列）
 *----------------------------------------------------------*/
typedef struct {
    int16_t temperature;   // 温度（整数）
    int16_t humidity;      // 湿度（整数）
} SensorData_t;

/* 队列句柄（全局） */
QueueHandle_t xSensorQueue;

/*-----------------------------------------------------------
 * 3. 栈溢出钩子函数（调试“安全气囊”）
 *    发生溢出时，LED常亮，程序卡死在此处方便定位。
 *----------------------------------------------------------*/
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    GPIO_ResetBits(LED_PORT, LED_PIN);  // 点亮LED
    while(1);                           // 死循环，等待调试器介入
}

/*-----------------------------------------------------------
 * 任务1：传感器采集任务（优先级2，周期2秒）
 *----------------------------------------------------------*/
static int16_t g_max_temp = -99;  
static int16_t g_min_temp = 100;   

void vSensorTask(void *pvParameters)
{
    SensorData_t data;
    DHT11_Data dht;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(2000);

    // 上电时从Flash读取历史极值
    g_max_temp = (int16_t)Flash_Read_HalfWord(FLASH_BASE_ADDR + 0x0010);
    g_min_temp = (int16_t)Flash_Read_HalfWord(FLASH_BASE_ADDR + 0x0012);
    if(g_max_temp > 80 || g_max_temp < -20) g_max_temp = -99;
    if(g_min_temp > 80 || g_min_temp < -20) g_min_temp = 100;

    for(;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        if(DHT11_ReadData(&dht) == 1)
        {
            data.temperature = dht.temp_int;
            data.humidity = dht.humi_int;

            // =============================================
            // 修改点：更新历史最高温（先读后擦，避免丢Boot）
            // =============================================
            if(data.temperature > g_max_temp) {
                g_max_temp = data.temperature;
                
                // 1. 在擦除之前，先把现有的启动计数和阈值读出来存到RAM
                uint32_t boot = Flash_Get_BootCount();
                int16_t th = Flash_Get_Threshold();
                
                // 2. 擦除整页
                Flash_Erase_Page();
                
                // 3. 写入新的历史极值
                Flash_Write_HalfWord(FLASH_BASE_ADDR + 0x0010, (uint16_t)g_max_temp);
                Flash_Write_HalfWord(FLASH_BASE_ADDR + 0x0012, (uint16_t)g_min_temp);
                
                // 4. 把刚才读出来的启动计数和阈值原封不动地写回去（Boot就不会丢了）
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT, (uint16_t)(boot & 0xFFFF));
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT + 2, (uint16_t)(boot >> 16));
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_TH, (uint16_t)th);
            }

            // =============================================
            // 修改点：更新历史最低温（同样处理）
            // =============================================
            if(data.temperature < g_min_temp) {
                g_min_temp = data.temperature;
                
                // 1. 擦除前先读取保留数据
                uint32_t boot = Flash_Get_BootCount();
                int16_t th = Flash_Get_Threshold();
                
                // 2. 擦除整页
                Flash_Erase_Page();
                
                // 3. 写入新的历史极值
                Flash_Write_HalfWord(FLASH_BASE_ADDR + 0x0010, (uint16_t)g_max_temp);
                Flash_Write_HalfWord(FLASH_BASE_ADDR + 0x0012, (uint16_t)g_min_temp);
                
                // 4. 恢复启动计数和阈值
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT, (uint16_t)(boot & 0xFFFF));
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_BOOT + 2, (uint16_t)(boot >> 16));
                Flash_Write_HalfWord(FLASH_BASE_ADDR + FLASH_OFFSET_TH, (uint16_t)th);
            }

            // 发送数据到队列
            xQueueOverwrite(xSensorQueue, &data);
        }
        else
        {
            data.temperature = -99;
            data.humidity = 0;
            xQueueOverwrite(xSensorQueue, &data);
        }
    }
}


/*-----------------------------------------------------------
 * 任务2：OLED显示任务（优先级1）
 * 功能：三页面自动轮播（每5秒切换）
 *    Page 0: 当前温湿度 + 状态
 *    Page 1: 历史最高/最低温度
 *    Page 2: 系统堆栈剩余 + 运行信息
 *----------------------------------------------------------*/
void vDisplayTask(void *pvParameters)
{
    SensorData_t data;
    int16_t threshold;
    uint32_t boot_count;
    uint8_t page_index = 0;      // 0, 1, 2
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPageSwitchTime = pdMS_TO_TICKS(5000); // 5秒切换

    for(;;)
    {
        // 阻塞等待队列数据
        if(xQueueReceive(xSensorQueue, &data, portMAX_DELAY) == pdPASS)
        {
            // 读取固定参数
            threshold = Flash_Get_Threshold();
            boot_count = Flash_Get_BootCount();

            // 清屏（每次切换前清空，避免文字重叠）
            OLED_Clear();

            // ============================================================
            //  根据页面索引显示不同内容（行列索引从 1 开始）
            // ============================================================
            switch(page_index)
            {
                case 0:  // ---------- 页面0：当前值 ----------
                    OLED_ShowString(1, 1, "=== Current ===");
                    
                    // 温度
                    OLED_ShowString(2, 1, "Temp: ");
                    if(data.temperature != -99) {
                        OLED_ShowNum(2, 7, data.temperature, 2);
                        OLED_ShowString(2, 9, " C");
                    } else {
                        OLED_ShowString(2, 7, "--");
                    }
                    
                    // 湿度
                    OLED_ShowString(3, 1, "Humi: ");
                    if(data.temperature != -99) {
                        OLED_ShowNum(3, 7, data.humidity, 2);
                        OLED_ShowString(3, 9, " %RH");
                    } else {
                        OLED_ShowString(3, 7, "--");
                    }
                    
                    // 状态
                    OLED_ShowString(4, 1, "Status: ");
                    if(data.temperature > threshold && data.temperature != -99) {
                        OLED_ShowString(4, 9, "ALARM!");
                    } else {
                        OLED_ShowString(4, 9, "OK");
                    }
                    break;

                case 1:  // ---------- 页面1：历史极值 ----------
                {
                    OLED_ShowString(1, 1, "=== History ===");
                    
                    // 从Flash读取历史极值
                    int16_t max_t = (int16_t)Flash_Read_HalfWord(FLASH_BASE_ADDR + 0x0010);
                    int16_t min_t = (int16_t)Flash_Read_HalfWord(FLASH_BASE_ADDR + 0x0012);
                    if(max_t > 80 || max_t < -20) max_t = -99;
                    if(min_t > 80 || min_t < -20) min_t = 100;
                    
                    // 最高温
                    OLED_ShowString(2, 1, "Max: ");
                    if(max_t != -99) {
                        OLED_ShowNum(2, 6, max_t, 2);
                        OLED_ShowString(2, 8, " C");
                    } else {
                        OLED_ShowString(2, 6, "--");
                    }
                    
                    // 最低温
                    OLED_ShowString(3, 1, "Min: ");
                    if(min_t != 100) {
                        OLED_ShowNum(3, 6, min_t, 2);
                        OLED_ShowString(3, 8, " C");
                    } else {
                        OLED_ShowString(3, 6, "--");
                    }
                    
                    // 启动次数
                    OLED_ShowString(4, 1, "Boot: ");
                    OLED_ShowNum(4, 7, boot_count, 4);
                    break;
                }

                case 2:  // ---------- 页面2：系统信息 ----------
                {
                    OLED_ShowString(1, 1, "=== System ===");
                    
                    // 剩余堆大小
                    size_t free_heap = xPortGetFreeHeapSize();
                    OLED_ShowString(2, 1, "Heap: ");
                    OLED_ShowNum(2, 7, free_heap, 4);
                    OLED_ShowString(2, 11, "B");
                    
                    // 当前阈值
                    OLED_ShowString(3, 1, "TH: ");
                    OLED_ShowNum(3, 5, threshold, 2);
                    OLED_ShowString(3, 7, " C");
                    
                    // 页码
                    OLED_ShowString(4, 1, "Page: ");
                    OLED_ShowNum(4, 7, page_index + 1, 1);
                    OLED_ShowString(4, 8, "/3");
                    break;
                }
            }

            // ============================================================
            //  等待5秒后切换页面
            // ============================================================
            vTaskDelayUntil(&xLastWakeTime, xPageSwitchTime);
            
            // 页面循环切换 (0 -> 1 -> 2 -> 0 ...)
            page_index++;
            if(page_index > 2) page_index = 0;
        }
    }
}


/*-----------------------------------------------------------
 * 6. 任务3：LED心跳指示 + 报警闪烁（优先级0）
 *    功能：正常时1秒周期闪烁；超阈值时200ms快速闪烁。
 *----------------------------------------------------------*/
void vLedTask(void *pvParameters)
{
    int16_t threshold;
    SensorData_t data;
    TickType_t delay_time;

    for(;;)
    {
        // 读取当前温湿度（通过队列复制一份，不破坏原有数据）
        if(xQueuePeek(xSensorQueue, &data, 0) == pdPASS) {
            threshold = Flash_Get_Threshold();
            if(data.temperature > threshold && data.temperature != -99) {
                delay_time = pdMS_TO_TICKS(200); // 快闪（报警）
            } else {
                delay_time = pdMS_TO_TICKS(500); // 慢闪（正常）
            }
        } else {
            delay_time = pdMS_TO_TICKS(500);
        }

        GPIO_ResetBits(LED_PORT, LED_PIN);
        vTaskDelay(delay_time);
        GPIO_SetBits(LED_PORT, LED_PIN);
        vTaskDelay(delay_time);
    }
}

/*-----------------------------------------------------------
 * 7. 主函数
 *----------------------------------------------------------*/
int main(void)
{
    /* ----- 硬件初始化（全部用标准库驱动）----- */
    Delay_Init();      // TIM2 微秒延时
    OLED_Init();       // OLED 初始化
    DHT11_Init();      // DHT11 初始化

    /* ----- 初始化板载LED（PC13）----- */
    RCC_APB2PeriphClockCmd(LED_RCC, ENABLE);
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = LED_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &gpio);
    GPIO_SetBits(LED_PORT, LED_PIN); // 初始熄灭

    /* ----- Flash测试：启动计数器 +1 ----- */
    Flash_Init();
    Flash_Increment_BootCount();   // 每次复位，数字+1

    /* ----- 创建消息队列（深度1，存放最新数据）----- */
    xSensorQueue = xQueueCreate(1, sizeof(SensorData_t));

    /* ----- 创建3个任务 ----- */
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 2, NULL);
    xTaskCreate(vDisplayTask, "Display", 256, NULL, 1, NULL);
    xTaskCreate(vLedTask, "LED", 128, NULL, 0, NULL);

    /* ----- 启动FreeRTOS调度器（永不返回）----- */
    vTaskStartScheduler();

    /* 如果跑到这里，说明堆内存不足或启动失败 */
    while(1);
}

/************************ 文件结束 ************************/
