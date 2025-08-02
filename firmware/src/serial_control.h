#ifndef _SERIAL_CONTROL_H_
#define _SERIAL_CONTROL_H_

#include <stdint.h>

// 串口命令定义
#define CMD_HEADER 0xAA
#define CMD_FOOTER 0x55

// 命令类型
enum SerialCommand {
    CMD_GET_STATUS = 0x01,      // 获取设备状态
    CMD_SEND_KEY = 0x02,        // 发送按键
    CMD_SEND_MOUSE = 0x03,      // 发送鼠标动作
    CMD_SET_CONFIG = 0x04,      // 设置配置
    CMD_GET_DEVICE_INFO = 0x05, // 获取克隆设备信息
    CMD_INJECT_REPORT = 0x06,   // 注入自定义HID报告
    CMD_RESET = 0xFF            // 重置设备
};

// 按键命令结构
struct __attribute__((packed)) KeyCommand {
    uint8_t modifiers;    // 修饰键 (Ctrl, Alt, Shift等)
    uint8_t keycode;      // 按键码
    uint8_t action;       // 0=释放, 1=按下, 2=点击(按下+释放)
};

// 鼠标命令结构
struct __attribute__((packed)) MouseCommand {
    int16_t x;           // X轴移动
    int16_t y;           // Y轴移动
    int8_t wheel;        // 滚轮
    uint8_t buttons;     // 按键状态 (bit0=左键, bit1=右键, bit2=中键)
    uint8_t action;      // 0=移动, 1=点击, 2=拖拽
};

// 设备状态结构
struct __attribute__((packed)) DeviceStatus {
    uint8_t connected;        // 是否有设备连接
    uint8_t cloned;          // 是否已克隆
    uint16_t vid;            // 克隆的VID
    uint16_t pid;            // 克隆的PID
    uint8_t polling_rate;    // 当前轮询率 (1=1000Hz, 8=125Hz)
    uint32_t uptime;         // 运行时间(秒)
};

// 串口数据包结构
struct __attribute__((packed)) SerialPacket {
    uint8_t header;      // 0xAA
    uint8_t command;     // 命令类型
    uint8_t length;      // 数据长度
    uint8_t data[64];    // 数据内容
    uint8_t checksum;    // 校验和
    uint8_t footer;      // 0x55
};

// 函数声明
void serial_control_init();
void serial_control_task();
void serial_monitor_task();  // 新增监控任务

// 命令处理函数
void handle_get_status();
void handle_send_key(const KeyCommand* cmd);
void handle_send_mouse(const MouseCommand* cmd);
void handle_set_config(const uint8_t* data, uint8_t length);
void handle_get_device_info();
void handle_inject_report(const uint8_t* data, uint8_t length);
void handle_reset();

#endif
