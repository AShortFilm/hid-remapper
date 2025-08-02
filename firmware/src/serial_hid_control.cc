#include "serial_hid_control.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"

// 全局变量
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_index = 0;
static bool uart_initialized = false;

// HID键码映射表
static const key_mapping_t key_mappings[] = {
    // 字母键
    {"A", HID_KEY_A, 0}, {"B", HID_KEY_B, 0}, {"C", HID_KEY_C, 0}, {"D", HID_KEY_D, 0},
    {"E", HID_KEY_E, 0}, {"F", HID_KEY_F, 0}, {"G", HID_KEY_G, 0}, {"H", HID_KEY_H, 0},
    {"I", HID_KEY_I, 0}, {"J", HID_KEY_J, 0}, {"K", HID_KEY_K, 0}, {"L", HID_KEY_L, 0},
    {"M", HID_KEY_M, 0}, {"N", HID_KEY_N, 0}, {"O", HID_KEY_O, 0}, {"P", HID_KEY_P, 0},
    {"Q", HID_KEY_Q, 0}, {"R", HID_KEY_R, 0}, {"S", HID_KEY_S, 0}, {"T", HID_KEY_T, 0},
    {"U", HID_KEY_U, 0}, {"V", HID_KEY_V, 0}, {"W", HID_KEY_W, 0}, {"X", HID_KEY_X, 0},
    {"Y", HID_KEY_Y, 0}, {"Z", HID_KEY_Z, 0},
    
    // 数字键
    {"1", HID_KEY_1, 0}, {"2", HID_KEY_2, 0}, {"3", HID_KEY_3, 0}, {"4", HID_KEY_4, 0},
    {"5", HID_KEY_5, 0}, {"6", HID_KEY_6, 0}, {"7", HID_KEY_7, 0}, {"8", HID_KEY_8, 0},
    {"9", HID_KEY_9, 0}, {"0", HID_KEY_0, 0},
    
    // 功能键
    {"F1", HID_KEY_F1, 0}, {"F2", HID_KEY_F2, 0}, {"F3", HID_KEY_F3, 0}, {"F4", HID_KEY_F4, 0},
    {"F5", HID_KEY_F5, 0}, {"F6", HID_KEY_F6, 0}, {"F7", HID_KEY_F7, 0}, {"F8", HID_KEY_F8, 0},
    {"F9", HID_KEY_F9, 0}, {"F10", HID_KEY_F10, 0}, {"F11", HID_KEY_F11, 0}, {"F12", HID_KEY_F12, 0},
    
    // 特殊键
    {"ENTER", HID_KEY_ENTER, 0}, {"SPACE", HID_KEY_SPACE, 0}, {"TAB", HID_KEY_TAB, 0},
    {"ESCAPE", HID_KEY_ESCAPE, 0}, {"BACKSPACE", HID_KEY_BACKSPACE, 0}, {"DELETE", HID_KEY_DELETE, 0},
    {"UP", HID_KEY_ARROW_UP, 0}, {"DOWN", HID_KEY_ARROW_DOWN, 0}, 
    {"LEFT", HID_KEY_ARROW_LEFT, 0}, {"RIGHT", HID_KEY_ARROW_RIGHT, 0},
    {"HOME", HID_KEY_HOME, 0}, {"END", HID_KEY_END, 0}, {"PAGE_UP", HID_KEY_PAGE_UP, 0}, 
    {"PAGE_DOWN", HID_KEY_PAGE_DOWN, 0},
    
    // 修饰键组合
    {"CTRL+A", HID_KEY_A, KEYBOARD_MODIFIER_LEFTCTRL},
    {"CTRL+C", HID_KEY_C, KEYBOARD_MODIFIER_LEFTCTRL},
    {"CTRL+V", HID_KEY_V, KEYBOARD_MODIFIER_LEFTCTRL},
    {"CTRL+X", HID_KEY_X, KEYBOARD_MODIFIER_LEFTCTRL},
    {"CTRL+Z", HID_KEY_Z, KEYBOARD_MODIFIER_LEFTCTRL},
    {"ALT+TAB", HID_KEY_TAB, KEYBOARD_MODIFIER_LEFTALT},
    {"ALT+F4", HID_KEY_F4, KEYBOARD_MODIFIER_LEFTALT},
    
    {NULL, 0, 0} // 结束标记
};

// 初始化串口HID控制
void serial_hid_control_init(void) {
    if (uart_initialized) {
        return;
    }
    
    // 初始化UART
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // 设置UART参数
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
    
    // 清空命令缓冲区
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    cmd_index = 0;
    uart_initialized = true;
    
    // 发送欢迎信息
    send_response("\r\n=== HID Remapper Serial Control ===");
    send_response("Ready for commands. Type HELP for available commands.");
    send_response("=====================================");
    send_response("> ");
}

// 串口HID控制任务
void serial_hid_control_task(void) {
    if (!uart_initialized) {
        return;
    }
    
    // 检查是否有数据可读
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        
        // 处理回车换行
        if (c == '\r' || c == '\n') {
            if (cmd_index > 0) {
                cmd_buffer[cmd_index] = '\0';
                
                // 解析并执行命令
                command_t cmd;
                if (parse_command(cmd_buffer, &cmd)) {
                    execute_command(&cmd);
                } else {
                    send_error("Invalid command format");
                }
                
                // 清空缓冲区
                memset(cmd_buffer, 0, sizeof(cmd_buffer));
                cmd_index = 0;
                send_response("> ");
            }
        }
        // 处理退格
        else if (c == '\b' || c == 127) {
            if (cmd_index > 0) {
                cmd_index--;
                cmd_buffer[cmd_index] = '\0';
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

// 解析命令
bool parse_command(const char* input, command_t* cmd) {
    if (!input || !cmd) {
        return false;
    }
    
    // 清空命令结构
    memset(cmd, 0, sizeof(command_t));
    
    // 复制输入字符串并转换为大写
    char temp[CMD_BUFFER_SIZE];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    // 转换为大写
    for (int i = 0; temp[i]; i++) {
        temp[i] = toupper(temp[i]);
    }
    
    // 分割字符串
    char* token = strtok(temp, " ");
    if (!token) {
        return false;
    }
    
    // 确定命令类型
    if (strcmp(token, "KEY") == 0) {
        cmd->type = CMD_KEY;
    } else if (strcmp(token, "TYPE") == 0) {
        cmd->type = CMD_TYPE;
    } else if (strcmp(token, "MOUSE") == 0) {
        // 需要下一个参数来确定鼠标命令类型
        token = strtok(NULL, " ");
        if (!token) return false;
        
        if (strcmp(token, "MOVE") == 0) {
            cmd->type = CMD_MOUSE_MOVE;
        } else if (strcmp(token, "CLICK") == 0) {
            cmd->type = CMD_MOUSE_CLICK;
        } else if (strcmp(token, "WHEEL") == 0) {
            cmd->type = CMD_MOUSE_WHEEL;
        } else {
            return false;
        }
    } else if (strcmp(token, "STATUS") == 0) {
        cmd->type = CMD_STATUS;
    } else if (strcmp(token, "HELP") == 0) {
        cmd->type = CMD_HELP;
    } else if (strcmp(token, "RESET") == 0) {
        cmd->type = CMD_RESET;
    } else {
        cmd->type = CMD_UNKNOWN;
        return false;
    }
    
    // 获取参数
    cmd->arg_count = 0;
    while ((token = strtok(NULL, " ")) != NULL && cmd->arg_count < MAX_ARGS) {
        strncpy(cmd->args[cmd->arg_count], token, MAX_ARG_LENGTH - 1);
        cmd->args[cmd->arg_count][MAX_ARG_LENGTH - 1] = '\0';
        cmd->arg_count++;
    }
    
    return true;
}

// 发送响应
void send_response(const char* message) {
    if (uart_initialized && message) {
        uart_puts(UART_ID, message);
        uart_puts(UART_ID, "\r\n");
    }
}

// 发送错误信息
void send_error(const char* error_msg) {
    if (uart_initialized && error_msg) {
        uart_puts(UART_ID, "ERROR: ");
        uart_puts(UART_ID, error_msg);
        uart_puts(UART_ID, "\r\n");
    }
}

// 执行命令
void execute_command(const command_t* cmd) {
    if (!cmd) {
        send_error("Null command");
        return;
    }

    switch (cmd->type) {
        case CMD_KEY:
            if (cmd->arg_count >= 1) {
                uint8_t keycode = get_keycode_from_name(cmd->args[0]);
                uint8_t modifier = get_modifier_from_name(cmd->args[0]);

                if (keycode != 0 || modifier != 0) {
                    if (inject_key_press(keycode, modifier)) {
                        sleep_ms(50);  // 按键持续时间
                        inject_key_release();
                        send_response("Key pressed");
                    } else {
                        send_error("Failed to send key");
                    }
                } else {
                    send_error("Unknown key");
                }
            } else {
                send_error("KEY command requires key name");
            }
            break;

        case CMD_TYPE:
            if (cmd->arg_count >= 1) {
                // 重新构造原始字符串（因为parse_command转换了大小写）
                char original[CMD_BUFFER_SIZE];
                int pos = 0;
                for (int i = 0; i < cmd->arg_count; i++) {
                    if (i > 0) original[pos++] = ' ';
                    strcpy(&original[pos], cmd->args[i]);
                    pos += strlen(cmd->args[i]);
                }
                original[pos] = '\0';

                if (inject_string(original)) {
                    send_response("String typed");
                } else {
                    send_error("Failed to type string");
                }
            } else {
                send_error("TYPE command requires text");
            }
            break;

        case CMD_MOUSE_MOVE:
            if (cmd->arg_count >= 2) {
                int x = atoi(cmd->args[0]);
                int y = atoi(cmd->args[1]);

                // 限制移动范围
                if (x > 127) x = 127;
                if (x < -127) x = -127;
                if (y > 127) y = 127;
                if (y < -127) y = -127;

                if (inject_mouse_move((int8_t)x, (int8_t)y)) {
                    char response[64];
                    sprintf(response, "Mouse moved: x=%d, y=%d", x, y);
                    send_response(response);
                } else {
                    send_error("Failed to move mouse");
                }
            } else {
                send_error("MOUSE MOVE requires X Y coordinates");
            }
            break;

        case CMD_MOUSE_CLICK:
            if (cmd->arg_count >= 1) {
                uint8_t button = get_mouse_button_from_name(cmd->args[0]);
                if (button != 0) {
                    if (inject_mouse_click(button)) {
                        char response[64];
                        sprintf(response, "Mouse clicked: %s", cmd->args[0]);
                        send_response(response);
                    } else {
                        send_error("Failed to click mouse");
                    }
                } else {
                    send_error("Unknown mouse button");
                }
            } else {
                send_error("MOUSE CLICK requires button name");
            }
            break;

        case CMD_MOUSE_WHEEL:
            if (cmd->arg_count >= 1) {
                int wheel = atoi(cmd->args[0]);
                if (wheel > 127) wheel = 127;
                if (wheel < -127) wheel = -127;

                if (inject_mouse_wheel((int8_t)wheel)) {
                    char response[64];
                    sprintf(response, "Mouse wheel: %d", wheel);
                    send_response(response);
                } else {
                    send_error("Failed to scroll wheel");
                }
            } else {
                send_error("MOUSE WHEEL requires scroll value");
            }
            break;

        case CMD_STATUS:
            print_status();
            break;

        case CMD_HELP:
            print_help();
            break;

        case CMD_RESET:
            send_response("Resetting...");
            sleep_ms(100);
            watchdog_reboot(0, 0, 0);
            break;

        default:
            send_error("Unknown command");
            break;
    }
}

// HID注入函数
bool inject_key_press(uint8_t keycode, uint8_t modifier) {
    if (!tud_hid_n_ready(0)) {
        return false;
    }

    uint8_t keycode_array[6] = {0};
    if (keycode != 0) {
        keycode_array[0] = keycode;
    }

    return tud_hid_n_keyboard_report(0, 0, modifier, keycode_array);
}

bool inject_key_release(void) {
    if (!tud_hid_n_ready(0)) {
        return false;
    }

    uint8_t keycode_array[6] = {0};
    return tud_hid_n_keyboard_report(0, 0, 0, keycode_array);
}

bool inject_string(const char* str) {
    if (!str || !tud_hid_n_ready(0)) {
        return false;
    }

    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        uint8_t keycode = 0;
        uint8_t modifier = 0;

        // 转换字符到键码
        if (c >= 'a' && c <= 'z') {
            keycode = HID_KEY_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        } else if (c >= '0' && c <= '9') {
            keycode = HID_KEY_0 + (c - '0');
        } else if (c == ' ') {
            keycode = HID_KEY_SPACE;
        } else if (c == '\n') {
            keycode = HID_KEY_ENTER;
        } else if (c == '\t') {
            keycode = HID_KEY_TAB;
        } else {
            // 跳过不支持的字符
            continue;
        }

        // 发送按键
        if (!inject_key_press(keycode, modifier)) {
            return false;
        }
        sleep_ms(10);  // 短暂延迟

        if (!inject_key_release()) {
            return false;
        }
        sleep_ms(10);  // 字符间延迟
    }

    return true;
}

bool inject_mouse_move(int8_t x, int8_t y) {
    if (!tud_hid_n_ready(0)) {
        return false;
    }

    return tud_hid_n_mouse_report(0, 0, 0, x, y, 0, 0);
}

bool inject_mouse_click(uint8_t buttons) {
    if (!tud_hid_n_ready(0)) {
        return false;
    }

    // 按下
    if (!tud_hid_n_mouse_report(0, 0, buttons, 0, 0, 0, 0)) {
        return false;
    }
    sleep_ms(50);  // 点击持续时间

    // 释放
    return tud_hid_n_mouse_report(0, 0, 0, 0, 0, 0, 0);
}

bool inject_mouse_wheel(int8_t wheel) {
    if (!tud_hid_n_ready(0)) {
        return false;
    }

    return tud_hid_n_mouse_report(0, 0, 0, 0, 0, wheel, 0);
}

// 辅助函数
uint8_t get_keycode_from_name(const char* key_name) {
    if (!key_name) return 0;

    for (int i = 0; key_mappings[i].name != NULL; i++) {
        if (strcmp(key_name, key_mappings[i].name) == 0) {
            return key_mappings[i].keycode;
        }
    }
    return 0;
}

uint8_t get_modifier_from_name(const char* key_name) {
    if (!key_name) return 0;

    for (int i = 0; key_mappings[i].name != NULL; i++) {
        if (strcmp(key_name, key_mappings[i].name) == 0) {
            return key_mappings[i].modifier;
        }
    }
    return 0;
}

uint8_t get_mouse_button_from_name(const char* button_name) {
    if (!button_name) return 0;

    if (strcmp(button_name, "LEFT") == 0) {
        return MOUSE_BUTTON_LEFT;
    } else if (strcmp(button_name, "RIGHT") == 0) {
        return MOUSE_BUTTON_RIGHT;
    } else if (strcmp(button_name, "MIDDLE") == 0) {
        return MOUSE_BUTTON_MIDDLE;
    }
    return 0;
}

void print_status(void) {
    send_response("=== HID Remapper Status ===");

    char buffer[128];
    sprintf(buffer, "HID Ready: %s", tud_hid_n_ready(0) ? "Yes" : "No");
    send_response(buffer);

    sprintf(buffer, "UART: %d baud, TX=GP%d, RX=GP%d",
            UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    send_response(buffer);

    send_response("Device Cloning: Active");
    send_response("Polling Rate: 1000Hz");
    send_response("==========================");
}

void print_help(void) {
    send_response("=== Available Commands ===");
    send_response("KEY <keyname>        - Press a key (e.g., KEY A, KEY CTRL+C)");
    send_response("TYPE <text>          - Type text string");
    send_response("MOUSE MOVE <x> <y>   - Move mouse (x,y: -127 to 127)");
    send_response("MOUSE CLICK <button> - Click mouse (LEFT/RIGHT/MIDDLE)");
    send_response("MOUSE WHEEL <value>  - Scroll wheel (-127 to 127)");
    send_response("STATUS               - Show device status");
    send_response("HELP                 - Show this help");
    send_response("RESET                - Reset device");
    send_response("");
    send_response("Examples:");
    send_response("  KEY A");
    send_response("  KEY CTRL+C");
    send_response("  TYPE Hello World");
    send_response("  MOUSE MOVE 10 -5");
    send_response("  MOUSE CLICK LEFT");
    send_response("  MOUSE WHEEL 3");
    send_response("==========================");
}

// 清理函数
void serial_hid_control_deinit(void) {
    if (uart_initialized) {
        uart_deinit(UART_ID);
        uart_initialized = false;
    }
}

// 检查HID是否就绪
bool is_hid_ready(void) {
    return tud_hid_n_ready(0);
}
