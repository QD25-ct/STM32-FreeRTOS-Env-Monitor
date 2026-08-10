\# STM32‑FreeRTOS‑Env‑Monitor

基于STM32F103C8T6 + FreeRTOS 的嵌入式环境监控终端。



\## 项目简介

本项目实现DHT11温湿度采集、OLED屏幕多页面轮转显示、阈值报警、参数掉电保存功能。采用FreeRTOS多任务架构，任务之间通过消息队列传递数据，开启栈溢出检测，模拟真实嵌入式项目开发流程，完成驱动封装与工程目录规范化。



\## 硬件平台

\- MCU：STM32F103C8T6

\- 传感器：DHT11温湿度模块

\- 显示：0.96寸 I2C OLED（SSD1306）

\- 外设：LED报警指示灯



\## 主要功能

1\. DHT11周期采集环境温度、湿度数据

2\. OLED多页面自动轮转：实时数值、历史极值、系统堆栈信息

3\. Flash模拟EEPROM，保存报警阈值，实现参数掉电不丢失

4\. 温湿度超出设定阈值时，LED闪烁报警

5\. FreeRTOS多任务划分，消息队列完成任务间数据通信

6\. 开启任务栈溢出检测，便于调试排查内存越界问题



\## 工程目录

├── BSP // 板级外设驱动

├── system // 系统定时器、底层工具

├── User // 业务逻辑、FreeRTOS 应用任务

├── freertos // FreeRTOS 内核源码

├── Hardware // 硬件相关驱动

├── Library // STM32 标准库

├── Start // 启动文件

└── Project.uvprojx // Keil 工程文件



\## 编译环境

Keil MDK‑ARM5



\## 使用说明

1\. 使用Keil打开`Project.uvprojx`工程

2\. 编译后下载至STM32F103C8T6开发板

3\. 上电自动运行，OLED循环切换页面，数据超限触发LED报警。

