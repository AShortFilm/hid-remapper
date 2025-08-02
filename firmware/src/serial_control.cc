#include "serial_control.h"
#include "globals.h"
#include "remapper.h"
#include "interval_override.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// 函数声明
void process_text_command(const char* cmd);
void handle_status_command();
void handle_info_command();
void handle_key_command(const char* params);
void handle_mouse_command(const char* params);
void handle_reset_command();
void handle_help_command();
void handle_type_command(const char* text);
void handle_config_command(const char* params);

// HID注入函数
void inject_keyboard_key(uint8_t modifiers, uint8_t keycode);
void inject_mouse_move(int16_t x, int16_t y);
void inject_mouse_click(uint8_t buttons);
void inject_mouse_wheel(int8_t wheel);
void type_string(const char* text);

// 串口配置
#define UART_ID uart0
#define UART_TX_PIN 12  // CH340C RXD
#define UART_RX_PIN 13  // CH340C TXD
#define UART_BAUD_RATE 115200

// 简单的命令缓冲区
static char cmd_buffer[128];
static uint8_t cmd_index = 0;

// 状态变量
static uint32_t start_time = 0;

void serial_control_init() {
    // 初始化UART
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // 设置数据格式: 8位数据, 1位停止位, 无校验
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);

    start_time = to_ms_since_boot(get_absolute_time());

    printf("Serial control initialized on UART0 (TX=GP%d, RX=GP%d) @ %d baud\n",
           UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);

    // 发送欢迎信息
    uart_puts(UART_ID, "\r\n=== HID Remapper Custom Firmware ===\r\n");
    uart_puts(UART_ID, "Serial Control Interface Ready\r\n");
    uart_puts(UART_ID, "Type HELP for available commands\r\n");
    uart_puts(UART_ID, "====================================\r\n");
}

void serial_control_task() {
    // 检查是否有数据可读
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);

        // 处理回车换行
        if (c == '\r' || c == '\n') {
            if (cmd_index > 0) {
                cmd_buffer[cmd_index] = '\0';
                process_text_command(cmd_buffer);
                cmd_index = 0;
            }
        }
        // 处理退格
        else if (c == '\b' || c == 127) {
            if (cmd_index > 0) {
                cmd_index--;
                uart_putc(UART_ID, '\b');
                uart_putc(UART_ID, ' ');
                uart_putc(UART_ID, '\b');
            }
        }
        // 普通字符
        else if (c >= 32 && c < 127 && cmd_index < sizeof(cmd_buffer) - 1) {
            cmd_buffer[cmd_index++] = c;
            uart_putc(UART_ID, c);  // 回显
        }
    }
}

void process_text_command(const char* cmd) {
    printf("Processing command: %s\n", cmd);

    // 转换为大写便于比较
    char upper_cmd[128];
    for (int i = 0; cmd[i] && i < 127; i++) {
        upper_cmd[i] = (cmd[i] >= 'a' && cmd[i] <= 'z') ? cmd[i] - 32 : cmd[i];
        upper_cmd[i+1] = '\0';
    }

    if (strncmp(upper_cmd, "STATUS", 6) == 0) {
        handle_status_command();
    }
    else if (strncmp(upper_cmd, "INFO", 4) == 0) {
        handle_info_command();
    }
    else if (strncmp(upper_cmd, "KEY ", 4) == 0) {
        handle_key_command(upper_cmd + 4);
    }
    else if (strncmp(upper_cmd, "MOUSE ", 6) == 0) {
        handle_mouse_command(upper_cmd + 6);
    }
    else if (strncmp(upper_cmd, "RESET", 5) == 0) {
        handle_reset_command();
    }
    else if (strncmp(upper_cmd, "HELP", 4) == 0) {
        handle_help_command();
    }
    else if (strncmp(upper_cmd, "TYPE ", 5) == 0) {
        handle_type_command(upper_cmd + 5);
    }
    else if (strncmp(upper_cmd, "CONFIG ", 7) == 0) {
        handle_config_command(upper_cmd + 7);
    }
    else {
        uart_puts(UART_ID, "Unknown command. Type HELP for available commands.\r\n");
    }

    uart_puts(UART_ID, "> ");  // 提示符
}

void handle_status_command() {
    uint32_t uptime = (to_ms_since_boot(get_absolute_time()) - start_time) / 1000;

    uart_puts(UART_ID, "=== HID Remapper Status ===\r\n");

    char buffer[128];
    sprintf(buffer, "Uptime: %lu seconds\r\n", uptime);
    uart_puts(UART_ID, buffer);

    sprintf(buffer, "Device Connected: %s\r\n", cloned_device.is_cloned ? "Yes" : "No");
    uart_puts(UART_ID, buffer);

    if (cloned_device.is_cloned) {
        sprintf(buffer, "VID: 0x%04X\r\n", cloned_device.vid);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "PID: 0x%04X\r\n", cloned_device.pid);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "Polling Rate: 1000Hz\r\n");
        uart_puts(UART_ID, buffer);
    }

    uart_puts(UART_ID, "========================\r\n");
}

void handle_info_command() {
    uart_puts(UART_ID, "=== Device Information ===\r\n");

    if (cloned_device.is_cloned) {
        char buffer[128];

        sprintf(buffer, "Manufacturer: %s\r\n", cloned_device.manufacturer);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "Product: %s\r\n", cloned_device.product);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "Serial: %s\r\n", cloned_device.serial_number);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "VID: 0x%04X\r\n", cloned_device.vid);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "PID: 0x%04X\r\n", cloned_device.pid);
        uart_puts(UART_ID, buffer);

        sprintf(buffer, "Version: 0x%04X\r\n", cloned_device.bcd_device);
        uart_puts(UART_ID, buffer);
    } else {
        uart_puts(UART_ID, "No device connected\r\n");
    }

    uart_puts(UART_ID, "========================\r\n");
}

void handle_key_command(const char* params) {
    uart_puts(UART_ID, "Key command: ");
    uart_puts(UART_ID, params);
    uart_puts(UART_ID, "\r\n");

    // 解析按键参数
    uint8_t modifiers = 0;
    uint8_t keycode = 0;

    // 跳过前导空格
    while (*params == ' ') params++;

    // 解析修饰键
    if (strstr(params, "CTRL+") || strstr(params, "CONTROL+")) {
        modifiers |= 0x01;  // Left Ctrl
        params = strchr(params, '+') + 1;
    }
    if (strstr(params, "SHIFT+")) {
        modifiers |= 0x02;  // Left Shift
        params = strchr(params, '+') + 1;
    }
    if (strstr(params, "ALT+")) {
        modifiers |= 0x04;  // Left Alt
        params = strchr(params, '+') + 1;
    }
    if (strstr(params, "WIN+") || strstr(params, "GUI+")) {
        modifiers |= 0x08;  // Left GUI (Windows key)
        params = strchr(params, '+') + 1;
    }

    // 解析按键码
    if (strlen(params) == 1) {
        // 单个字符
        char c = params[0];
        if (c >= 'A' && c <= 'Z') {
            keycode = 0x04 + (c - 'A');  // A-Z
        } else if (c >= '1' && c <= '9') {
            keycode = 0x1E + (c - '1');  // 1-9
        } else if (c == '0') {
            keycode = 0x27;  // 0
        } else {
            // 特殊字符
            switch (c) {
                case ' ': keycode = 0x2C; break;  // Space
                case '\t': keycode = 0x2B; break; // Tab
                case '\n': case '\r': keycode = 0x28; break; // Enter
            }
        }
    } else {
        // 特殊按键名称
        if (strcmp(params, "ENTER") == 0) keycode = 0x28;
        else if (strcmp(params, "ESC") == 0 || strcmp(params, "ESCAPE") == 0) keycode = 0x29;
        else if (strcmp(params, "BACKSPACE") == 0) keycode = 0x2A;
        else if (strcmp(params, "TAB") == 0) keycode = 0x2B;
        else if (strcmp(params, "SPACE") == 0) keycode = 0x2C;
        else if (strcmp(params, "F1") == 0) keycode = 0x3A;
        else if (strcmp(params, "F2") == 0) keycode = 0x3B;
        else if (strcmp(params, "F3") == 0) keycode = 0x3C;
        else if (strcmp(params, "F4") == 0) keycode = 0x3D;
        else if (strcmp(params, "F5") == 0) keycode = 0x3E;
        else if (strcmp(params, "F6") == 0) keycode = 0x3F;
        else if (strcmp(params, "F7") == 0) keycode = 0x40;
        else if (strcmp(params, "F8") == 0) keycode = 0x41;
        else if (strcmp(params, "F9") == 0) keycode = 0x42;
        else if (strcmp(params, "F10") == 0) keycode = 0x43;
        else if (strcmp(params, "F11") == 0) keycode = 0x44;
        else if (strcmp(params, "F12") == 0) keycode = 0x45;
        else if (strcmp(params, "UP") == 0) keycode = 0x52;
        else if (strcmp(params, "DOWN") == 0) keycode = 0x51;
        else if (strcmp(params, "LEFT") == 0) keycode = 0x50;
        else if (strcmp(params, "RIGHT") == 0) keycode = 0x4F;
        else if (strcmp(params, "DELETE") == 0) keycode = 0x4C;
        else if (strcmp(params, "HOME") == 0) keycode = 0x4A;
        else if (strcmp(params, "END") == 0) keycode = 0x4D;
        else if (strcmp(params, "PAGEUP") == 0) keycode = 0x4B;
        else if (strcmp(params, "PAGEDOWN") == 0) keycode = 0x4E;
    }

    if (keycode == 0) {
        uart_puts(UART_ID, "Invalid key. Examples: A, CTRL+C, F1, ENTER\r\n");
        return;
    }

    // 发送按键
    inject_keyboard_key(modifiers, keycode);

    char response[64];
    sprintf(response, "Key injected: mod=0x%02X, key=0x%02X\r\n", modifiers, keycode);
    uart_puts(UART_ID, response);
}

void handle_mouse_command(const char* params) {
    uart_puts(UART_ID, "Mouse command: ");
    uart_puts(UART_ID, params);
    uart_puts(UART_ID, "\r\n");

    // 跳过前导空格
    while (*params == ' ') params++;

    if (strncmp(params, "MOVE ", 5) == 0) {
        // 鼠标移动: MOVE x y
        int x, y;
        if (sscanf(params + 5, "%d %d", &x, &y) == 2) {
            inject_mouse_move(x, y);
            char response[64];
            sprintf(response, "Mouse moved: x=%d, y=%d\r\n", x, y);
            uart_puts(UART_ID, response);
        } else {
            uart_puts(UART_ID, "Usage: MOUSE MOVE <x> <y>\r\n");
        }
    }
    else if (strncmp(params, "CLICK ", 6) == 0) {
        // 鼠标点击: CLICK LEFT/RIGHT/MIDDLE
        const char* button = params + 6;
        uint8_t button_mask = 0;

        if (strcmp(button, "LEFT") == 0) {
            button_mask = 0x01;
        } else if (strcmp(button, "RIGHT") == 0) {
            button_mask = 0x02;
        } else if (strcmp(button, "MIDDLE") == 0) {
            button_mask = 0x04;
        } else {
            uart_puts(UART_ID, "Usage: MOUSE CLICK LEFT/RIGHT/MIDDLE\r\n");
            return;
        }

        inject_mouse_click(button_mask);
        uart_puts(UART_ID, "Mouse clicked\r\n");
    }
    else if (strncmp(params, "WHEEL ", 6) == 0) {
        // 鼠标滚轮: WHEEL UP/DOWN/number
        const char* direction = params + 6;
        int8_t wheel = 0;

        if (strcmp(direction, "UP") == 0) {
            wheel = 1;
        } else if (strcmp(direction, "DOWN") == 0) {
            wheel = -1;
        } else {
            wheel = atoi(direction);
        }

        inject_mouse_wheel(wheel);
        char response[64];
        sprintf(response, "Mouse wheel: %d\r\n", wheel);
        uart_puts(UART_ID, response);
    }
    else {
        uart_puts(UART_ID, "Usage: MOUSE MOVE <x> <y> | CLICK LEFT/RIGHT/MIDDLE | WHEEL UP/DOWN\r\n");
    }
}

void handle_reset_command() {
    uart_puts(UART_ID, "Resetting device...\r\n");
    sleep_ms(100);  // 等待消息发送完成

    // 重启设备
    watchdog_enable(1, 1);
    while(1);
}

void handle_help_command() {
    uart_puts(UART_ID, "=== Commands ===\r\n");
    uart_puts(UART_ID, "STATUS - Device status\r\n");
    uart_puts(UART_ID, "INFO - Device info\r\n");
    uart_puts(UART_ID, "KEY <key> - Send key\r\n");
    uart_puts(UART_ID, "TYPE <text> - Type text\r\n");
    uart_puts(UART_ID, "MOUSE MOVE <x> <y>\r\n");
    uart_puts(UART_ID, "MOUSE CLICK LEFT/RIGHT\r\n");
    uart_puts(UART_ID, "CONFIG POLLING <1-8>\r\n");
    uart_puts(UART_ID, "RESET - Restart\r\n");
    uart_puts(UART_ID, "===============\r\n");
}

// HID注入函数实现
void inject_keyboard_key(uint8_t modifiers, uint8_t keycode) {
    if (tud_hid_ready()) {
        uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
        tud_hid_keyboard_report(2, modifiers, keycodes);
        sleep_ms(10);  // 按键持续时间
        tud_hid_keyboard_report(2, 0, NULL);  // 释放按键
    }
}

void inject_mouse_move(int16_t x, int16_t y) {
    if (tud_hid_ready()) {
        tud_hid_mouse_report(1, 0, x, y, 0, 0);
    }
}

void inject_mouse_click(uint8_t buttons) {
    if (tud_hid_ready()) {
        tud_hid_mouse_report(1, buttons, 0, 0, 0, 0);  // 按下
        sleep_ms(50);  // 点击持续时间
        tud_hid_mouse_report(1, 0, 0, 0, 0, 0);        // 释放
    }
}

void inject_mouse_wheel(int8_t wheel) {
    if (tud_hid_ready()) {
        tud_hid_mouse_report(1, 0, 0, 0, wheel, 0);
    }
}

void type_string(const char* text) {
    for (int i = 0; text[i]; i++) {
        char c = text[i];
        uint8_t modifiers = 0;
        uint8_t keycode = 0;

        // 转换字符到按键码
        if (c >= 'a' && c <= 'z') {
            keycode = 0x04 + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = 0x04 + (c - 'A');
            modifiers = 0x02;  // Shift
        } else if (c >= '1' && c <= '9') {
            keycode = 0x1E + (c - '1');
        } else if (c == '0') {
            keycode = 0x27;
        } else if (c == ' ') {
            keycode = 0x2C;
        } else if (c == '\n' || c == '\r') {
            keycode = 0x28;  // Enter
        } else {
            // 跳过不支持的字符
            continue;
        }

        inject_keyboard_key(modifiers, keycode);
        sleep_ms(20);  // 字符间延迟
    }
}

// 新命令实现
void handle_type_command(const char* text) {
    uart_puts(UART_ID, "Typing: ");
    uart_puts(UART_ID, text);
    uart_puts(UART_ID, "\r\n");

    type_string(text);
    uart_puts(UART_ID, "Text typed successfully\r\n");
}



void handle_config_command(const char* params) {
    uart_puts(UART_ID, "Config: ");
    uart_puts(UART_ID, params);
    uart_puts(UART_ID, "\r\n");

    if (strncmp(params, "POLLING ", 8) == 0) {
        int rate = atoi(params + 8);
        if (rate >= 1 && rate <= 8) {
            interval_override = rate;
            char response[64];
            sprintf(response, "Polling rate set to %dms (%dHz)\r\n", rate, 1000/rate);
            uart_puts(UART_ID, response);
        } else {
            uart_puts(UART_ID, "Invalid polling rate. Use 1-8 (1=1000Hz, 8=125Hz)\r\n");
        }
    }
    else if (strncmp(params, "DEBUG ", 6) == 0) {
        const char* state = params + 6;
        if (strcmp(state, "ON") == 0) {
            uart_puts(UART_ID, "Debug output enabled\r\n");
            // 这里可以设置调试标志
        } else if (strcmp(state, "OFF") == 0) {
            uart_puts(UART_ID, "Debug output disabled\r\n");
            // 这里可以清除调试标志
        } else {
            uart_puts(UART_ID, "Usage: CONFIG DEBUG ON/OFF\r\n");
        }
    }
    else {
        uart_puts(UART_ID, "Available configs: POLLING <1-8>, DEBUG ON/OFF\r\n");
    }
}

// 简化的监控功能
void serial_monitor_task() {
    // 简化实现，减少内存使用
}
