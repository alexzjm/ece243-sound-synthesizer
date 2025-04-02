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

void init_ps2() {
    ps2->DATA = 0xFF;  // 发送复位命令，触发自检
}

void enable_scanning() {
    ps2->DATA = 0xF4;  // 启用扫描
}

int main() {
    uint32_t data;
    uint8_t code;

    init_ps2();

    while (1) {
        data = ps2->DATA;

        if ((data & 0x8000) != 0) {  // 数据有效
            code = data & 0xFF;

            if (code == 0xAA) {
                // 自检成功，发送启用扫描命令
                enable_scanning();
            } else if (code == 0xF0) {
                // 收到 break code 标记
                got_break = true;
            } else {
                if (got_break) {
                    // 按键被松开后又按下一个键
                    led->LEDR = 0x2;  // LED1 亮
                    got_break = false;
                } else {
                    led->LEDR = 0x1;  // LED0 亮
                }
            }
        }
    }

    return 0;
}
