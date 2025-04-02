#include <stdint.h>
#include <stdbool.h>

#define PS2_BASE  0xFF200100
#define LED_BASE  0xFF200000

typedef struct {
    volatile uint32_t DATA;
    volatile uint32_t CTRL;
} ps2_t;

typedef struct {
    volatile uint32_t LEDR;
} led_t;

ps2_t* ps2 = (ps2_t*) PS2_BASE;
led_t* led = (led_t*) LED_BASE;

bool got_break = false;
bool ps2_ready = false;

// 简单延时函数（使用 volatile 防止被优化掉）
void delay(int loops) {
    for (volatile int i = 0; i < loops; i++);
}

// 写命令前检查 ready
void ps2_wait_ready() {
    while (ps2->DATA & 0x8000); // bit15=1 表示 data busy
}

// 发送命令到键盘（带ready检查和延迟）
void ps2_send_cmd(uint8_t cmd) {
    ps2_wait_ready();
    delay(50000); // 防止硬件太快
    ps2->DATA = cmd;
}

// 初始化键盘：发送0xFF，等待0xAA，发送0xF4
void ps2_init_keyboard() {
    ps2_send_cmd(0xFF);  // Reset command

    // 等待返回0xAA
    while (1) {
        if ((ps2->DATA & 0x8000) != 0) {
            uint8_t code = ps2->DATA & 0xFF;
            if (code == 0xAA) {
                led->LEDR = 0x8;  // LED显示：收到0xAA
                delay(100000);
                break;
            }
        }
    }

    ps2_send_cmd(0xF4);  // 启用扫描
    led->LEDR = 0x4;      // LED显示：发送了F4
    delay(100000);
}

int main() {
    ps2_init_keyboard();  // 一开始先初始化

    while (1) {
        if ((ps2->DATA & 0x8000) != 0) {
            uint8_t code = ps2->DATA & 0xFF;

            if (code == 0xF0) {
                // Break code: 下一个字节是释放的键码
                got_break = true;
                led->LEDR = 0x4;  // 显示break阶段
            } else {
                if (got_break) {
                    // 刚松开一个键，又按下一个键
                    led->LEDR = 0x2;  // LED1亮
                    got_break = false;
                } else {
                    // 正常按下
                    led->LEDR = 0x1;  // LED0亮
                }
            }
        }
    }

    return 0;
}
