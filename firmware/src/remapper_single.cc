#include <tusb.h>
#include <cstdio>
#include <cstring>

#include "pio_usb.h"
#include "usb_midi_host.h"

#include "pico/platform.h"
#include "pico/time.h"

#include "descriptor_parser.h"
#include "out_report.h"
#include "remapper.h"
#include "tick.h"
#include "globals.h"
#include "interval_override.h"

// 函数声明
void clone_device_identity(uint8_t dev_addr, uint16_t vid, uint16_t pid);

static bool __no_inline_not_in_flash_func(manual_sof)(repeating_timer_t* rt) {
    pio_usb_host_frame();
    set_tick_pending();
    return true;
}

static repeating_timer_t sof_timer;

void extra_init() {
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;
    pio_cfg.skip_alarm_pool = true;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    add_repeating_timer_us(-1000, manual_sof, NULL, &sof_timer);
}

uint32_t get_gpio_valid_pins_mask() {
    return GPIO_VALID_PINS_BASE & ~(
#ifdef PICO_DEFAULT_UART_TX_PIN
                                      (1 << PICO_DEFAULT_UART_TX_PIN) |
#endif
#ifdef PICO_DEFAULT_UART_RX_PIN
                                      (1 << PICO_DEFAULT_UART_RX_PIN) |
#endif
                                      (1 << PICO_DEFAULT_PIO_USB_DP_PIN) |
                                      (1 << (PICO_DEFAULT_PIO_USB_DP_PIN + 1)));
}

static bool reports_received;

void read_report(bool* new_report, bool* tick) {
    *tick = get_and_clear_tick_pending();

    reports_received = false;
    tuh_task();
    *new_report = reports_received;
}

void interval_override_updated() {
}

void flash_b_side() {
}

void descriptor_received_callback(uint16_t vendor_id, uint16_t product_id, const uint8_t* report_descriptor, int len, uint16_t interface, uint8_t hub_port, uint8_t itf_num) {
    parse_descriptor(vendor_id, product_id, report_descriptor, len, interface, itf_num);
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    printf("tuh_hid_mount_cb\n");

    uint8_t hub_addr;
    uint8_t hub_port;
    tuh_get_hub_addr_port(dev_addr, &hub_addr, &hub_port);

    uint16_t vid;
    uint16_t pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    tuh_itf_info_t itf_info;
    tuh_hid_itf_get_info(dev_addr, instance, &itf_info);
    uint8_t itf_num = itf_info.desc.bInterfaceNumber;

    // 克隆设备信息
    clone_device_identity(dev_addr, vid, pid);

    descriptor_received_callback(vid, pid, desc_report, desc_len, (uint16_t) (dev_addr << 8) | instance, hub_port, itf_num);

    device_connected_callback((uint16_t) (dev_addr << 8) | instance, vid, pid, hub_port);

    tuh_hid_receive_report(dev_addr, instance);
}

void umount_callback(uint8_t dev_addr, uint8_t instance) {
    device_disconnected_callback(dev_addr);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("tuh_hid_umount_cb\n");
    umount_callback(dev_addr, instance);
}

void report_received_callback(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    if (len > 0) {
        handle_received_report(report, len, (uint16_t) (dev_addr << 8) | instance);

        reports_received = true;
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    report_received_callback(dev_addr, instance, report, len);

    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets) {
    uint8_t hub_addr;
    uint8_t hub_port;
    tuh_get_hub_addr_port(dev_addr, &hub_addr, &hub_port);

    uint8_t buf[4];
    while (tuh_midi_packet_read(dev_addr, buf)) {
        handle_received_midi(hub_port, buf);
    }
    reports_received = true;
}

void queue_out_report(uint16_t interface, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
    do_queue_out_report(buffer, len, report_id, interface >> 8, interface & 0xFF, OutType::OUTPUT);
}

void queue_set_feature_report(uint16_t interface, uint8_t report_id, const uint8_t* buffer, uint8_t len) {
    do_queue_out_report(buffer, len, report_id, interface >> 8, interface & 0xFF, OutType::SET_FEATURE);
}

void queue_get_feature_report(uint16_t interface, uint8_t report_id, uint8_t len) {
    do_queue_get_report(report_id, interface >> 8, interface & 0xFF, len);
}

void send_out_report() {
    do_send_out_report();
}

void __no_inline_not_in_flash_func(sof_callback)() {
}

void get_report_cb(uint8_t dev_addr, uint8_t interface, uint8_t report_id, uint8_t report_type, uint8_t* report, uint16_t len) {
    handle_get_report_response((uint16_t) (dev_addr << 8) | interface, report_id, report, len);
}

void set_report_complete_cb(uint8_t dev_addr, uint8_t interface, uint8_t report_id) {
    handle_set_report_complete((uint16_t) (dev_addr << 8) | interface, report_id);
}

// 克隆设备身份信息
void clone_device_identity(uint8_t dev_addr, uint16_t vid, uint16_t pid) {
    printf("Cloning device identity: VID=0x%04X, PID=0x%04X\n", vid, pid);

    // 保存基本设备信息
    cloned_device.vid = vid;
    cloned_device.pid = pid;
    cloned_device.is_cloned = true;

    // 强制设置1000Hz轮询率 (1ms间隔)
    set_interval_override(1);
    printf("Forced 1000Hz polling rate for cloned device\n");

    // 获取设备描述符以获取更多信息
    // 注意：在实际实现中，我们需要异步获取字符串描述符
    // 这里先设置一些默认值，实际的字符串获取需要在回调中完成
    cloned_device.bcd_device = 0x0100; // 默认版本

    // 设置一些通用的字符串，实际应用中可以通过其他方式获取
    strcpy(cloned_device.manufacturer, "Logitech");  // 示例：伪装成罗技设备
    strcpy(cloned_device.product, "G Pro X Superlight");  // 示例产品名
    strcpy(cloned_device.serial_number, "123456789ABC");  // 示例序列号

    printf("Device cloning completed: %s %s (1000Hz)\n", cloned_device.manufacturer, cloned_device.product);
}


