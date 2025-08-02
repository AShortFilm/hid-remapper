#ifndef SERIAL_HID_CONTROL_H
#define SERIAL_HID_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// 配置定义 - 独占UART0，对应你的CH340C连接
#define UART_ID uart0
#define UART_TX_PIN 12  // Pico TX (GP12) → CH340C RXD (引脚4)
#define UART_RX_PIN 13  // Pico RX (GP13) → CH340C TXD (引脚3)
#define UART_BAUD_RATE 115200

#define CMD_BUFFER_SIZE 256
#define MAX_ARGS 8
#define MAX_ARG_LENGTH 32

// 命令类型枚举
typedef enum {
    CMD_UNKNOWN = 0,
    CMD_KEY,
    CMD_TYPE,
    CMD_MOUSE_MOVE,
    CMD_MOUSE_CLICK,
    CMD_MOUSE_WHEEL,
    CMD_STATUS,
    CMD_HELP,
    CMD_RESET
} command_type_t;

// 鼠标按键定义（使用TinyUSB的定义）
// MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE 已在TinyUSB中定义

// 特殊按键定义
typedef enum {
    KEY_CTRL = 0x01,
    KEY_SHIFT = 0x02,
    KEY_ALT = 0x04,
    KEY_GUI = 0x08
} modifier_key_t;

// 命令结构体
typedef struct {
    command_type_t type;
    char args[MAX_ARGS][MAX_ARG_LENGTH];
    int arg_count;
} command_t;

// HID键码映射结构
typedef struct {
    const char* name;
    uint8_t keycode;
    uint8_t modifier;
} key_mapping_t;

// 函数声明
void serial_hid_control_init(void);
void serial_hid_control_task(void);
void serial_hid_control_deinit(void);

// 命令处理函数
bool parse_command(const char* input, command_t* cmd);
void execute_command(const command_t* cmd);

// HID注入状态结构
typedef struct {
    bool active;
    uint8_t keyboard_modifier;
    uint8_t keyboard_keycode[6];
    uint8_t mouse_buttons;
    int8_t mouse_x;
    int8_t mouse_y;
    int8_t mouse_wheel;
    uint64_t expire_time_us;
} injection_state_t;

// HID注入函数
bool inject_key_press(uint8_t keycode, uint8_t modifier);
bool inject_key_release(void);
bool inject_string(const char* str);
bool inject_mouse_move(int8_t x, int8_t y);
bool inject_mouse_click(uint8_t buttons);
bool inject_mouse_wheel(int8_t wheel);

// 报告修改函数
void modify_outgoing_report(uint8_t* report, uint8_t len, uint8_t report_id);
void modify_input_report(uint8_t* report, uint16_t len, uint16_t interface);

// 辅助函数
uint8_t get_keycode_from_name(const char* key_name);
uint8_t get_modifier_from_name(const char* mod_name);
uint8_t get_mouse_button_from_name(const char* button_name);
void send_response(const char* message);
void send_error(const char* error_msg);

// 状态查询函数
bool is_hid_ready(void);
void print_status(void);
void print_help(void);

// 注入状态控制
extern injection_state_t injection_state;
void clear_injection_state(void);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_HID_CONTROL_H
