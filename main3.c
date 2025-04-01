#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>   //仅用于宏定义 M_PI，如无定义时

#pragma region Global Vars

// Constant definitions
#define AUDIO_BASE 0xFF203040
#define KEY_BASE 0xFF200050
#define SWITCH_BASE 0xFF200040
#define TIMER_BASE 0xFF202000
#define LED_BASE 0xFF200000
#define PS2_BASE 0xFF200100
#define SIN_IDX 0
#define SQR_IDX 1
#define TRI_IDX 2
#define SAW_IDX 3

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Q15 定点常数
#define Q15_MAX 32767

// 固定采样率（单位：Hz）
#define SAMPLE_RATE 8000
// 2^32 / SAMPLE_RATE 近似值（用于计算相位增量） 
#define PHASE_SCALE 536870.912  // 2^32/8000

// 包络更新速率（单位：Q15 级别的每采样步长）
#define ATTACK_RATE 200
#define DECAY_RATE 100
#define RELEASE_RATE 100

// 用于 ADSR 控制的全局参数（均为 Q15 表示，1.0=32767）
int32_t attack_val  = Q15_MAX;      // 初始：1.0
int32_t decay_val   = Q15_MAX/2;      // 初始：0.5
int32_t sustain_val = Q15_MAX;        // 初始：1.0
int32_t release_val = 0;              // 初始：0
bool fast_knob_change = true;

// Struct definitions
typedef struct audio_s {
    volatile unsigned int CTRL;
    volatile unsigned char RARC, RALC, WSRC, WSLC;
    volatile unsigned int LDATA;
    volatile unsigned int RDATA;
} audio_s;

typedef struct parallel_port {
    volatile unsigned int DR;
    volatile unsigned int DIR;
    volatile unsigned int MASK;
    volatile unsigned int EDGE;
} parallel_port;

typedef struct ps2_port {
    volatile unsigned int DATA;
    volatile unsigned int CTRL;
} ps2_port;

typedef struct timer_s {
    volatile unsigned int STATUS;
    volatile unsigned int CONTROL;
    volatile unsigned int START_LOW;
    volatile unsigned int START_HIGH;
    volatile unsigned int SNAP_SHOT_LOW;
    volatile unsigned int SNAP_SHOT_HIGH;
} timer_s;

typedef struct led_s {
    volatile unsigned int LEDR;
} led_s;

typedef struct rectangle {
    int x_base;
    int y_base;
    int width;
    int height;
} rectangle;

typedef struct note_struct {
    bool pressed;
    char name[5];
    uint8_t code;
    char ps2_key;
    // 原 frequency 用于说明，此处不再直接使用浮点
    int frequency; 
    int num_rects;
    rectangle rectangles[2];
} note_struct;

note_struct notes[20] = {
    {false, "C4",  0x15, 'Q',  262, 2, {{40, 175, 15, 40}, {40, 215, 20, 20}}},
    {false, "C#4", 0x1E, '2',  277, 1, {{55, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "D4",  0x1D, 'W',  294, 2, {{65, 175, 10, 40}, {60, 215, 20, 20}}},
    {false, "D#4", 0x26, '3',  311, 1, {{75, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "E4",  0x24, 'E',  330, 2, {{85, 175, 15, 40}, {80, 215, 20, 20}}},
    {false, "F4",  0x2D, 'R',  349, 2, {{100, 175, 15, 40}, {100, 215, 20, 20}}},
    {false, "F#4", 0x2E, '5',  370, 1, {{115, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "G4",  0x2C, 'T',  392, 2, {{125, 175, 10, 40}, {120, 215, 20, 20}}},
    {false, "G#4", 0x36, '6',  415, 1, {{135, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "A4",  0x35, 'Y',  440, 2, {{145, 175, 10, 40}, {140, 215, 20, 20}}},
    {false, "A#4", 0x3D, '7',  466, 1, {{155, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "B4",  0x3C, 'U',  494, 2, {{165, 175, 15, 40}, {160, 215, 20, 20}}},
    {false, "C5",  0x43, 'I',  523, 2, {{180, 175, 15, 40}, {180, 215, 20, 20}}},
    {false, "C#5", 0x46, '9',  554, 1, {{195, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "D5",  0x44, 'O',  587, 2, {{205, 175, 10, 40}, {200, 215, 20, 20}}},
    {false, "D#5", 0x45, '0',  622, 1, {{215, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "E5",  0x4D, 'P',  659, 2, {{225, 175, 15, 40}, {220, 215, 20, 20}}},
    {false, "F5",  0x54, '[',  698, 2, {{240, 175, 15, 40}, {240, 215, 20, 20}}},
    {false, "F#5", 0x55, '=',  740, 1, {{255, 175, 10, 40}, {0, 0, 0, 0}}},
    {false, "G5",  0x5B, ']',  784, 2, {{265, 175, 15, 40}, {260, 215, 20, 20}}}
};

// 固定声波数据结构，全部采用定点
typedef struct wave_struct {
    uint32_t phase;    // 当前相位（Q32：0～0xFFFFFFFF 表示 0～2π）
    int32_t output;    // 当前输出（Q15 格式）
    uint32_t omega;    // 每个采样周期相位增加值（计算公式：omega = frequency * (2^32/SAMPLE_RATE)）
    bool is_playing;
    char adsr_stat;    // 包络状态：'a'（attack），'d'（decay），'s'（sustain），'r'（release），'n'（none）
    int32_t adsr_multi; // 包络乘子（Q15 格式，0～Q15_MAX）
} wave_struct;

// 用固定点实现的波形查表函数
// 本例采用 16 点正弦表，索引取相位高 4 位
#define SINE_TABLE_SIZE 256
#define SINE_INDEX_SHIFT (32 - 8)

static int16_t sine_table[SINE_TABLE_SIZE];

void generate_sine_table() {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        sine_table[i] = (int16_t)(32767 * sin(2.0 * M_PI * i / SINE_TABLE_SIZE));
    }
}

int16_t fixed_sine(uint32_t phase) {
    uint32_t index = phase >> SINE_INDEX_SHIFT;
    return sine_table[index & (SINE_TABLE_SIZE - 1)];
}

int16_t fixed_square(uint32_t phase) {
    return (phase < 0x80000000UL) ? Q15_MAX : -Q15_MAX;
}

int16_t fixed_triangle(uint32_t phase) {
    phase -= 0x40000000UL;
    if (phase < 0x80000000UL) {
        int32_t value = (int32_t)((((uint64_t)phase * (2ULL * Q15_MAX)) >> 31) - Q15_MAX);
        return value;
    } else {
        uint32_t phase_offset = phase - 0x80000000UL;
        int32_t value = Q15_MAX - (int32_t)((((uint64_t)phase_offset * (2ULL * Q15_MAX)) >> 31));
        return value;
    }
}

int16_t fixed_sawtooth(uint32_t phase) {
    phase -= 0x80000000UL;
    int32_t value = (int32_t)((((uint64_t)phase * (2ULL * Q15_MAX)) >> 32) - Q15_MAX);
    return value;
}

// 全部声波数组（20 个音符），采用定点计算（omega 以整数表示，phase 初值 0，adsr_multi 初始为 0）
wave_struct waves[20] = {
    // omega = frequency * (2^32 / SAMPLE_RATE)
    {0, 0, 140553234, false, 'n', 0}, // C4, 262 Hz
    {0, 0, 148853940, false, 'n', 0}, // C#4, 277 Hz
    {0, 0, 157154646, false, 'n', 0}, // D4, 294 Hz
    {0, 0, 167015699, false, 'n', 0}, // D#4, 311 Hz
    {0, 0, 176876752, false, 'n', 0}, // E4, 330 Hz
    {0, 0, 187737805, false, 'n', 0}, // F4, 349 Hz
    {0, 0, 198598858, false, 'n', 0}, // F#4, 370 Hz
    {0, 0, 210124907, false, 'n', 0}, // G4, 392 Hz
    {0, 0, 222650956, false, 'n', 0}, // G#4, 415 Hz
    {0, 0, 235929600, false, 'n', 0}, // A4, 440 Hz
    {0, 0, 250000000, false, 'n', 0}, // A#4, 466 Hz (约)
    {0, 0, 265000000, false, 'n', 0}, // B4, 494 Hz (约)
    {0, 0, 280000000, false, 'n', 0}, // C5, 523 Hz (约)
    {0, 0, 298000000, false, 'n', 0}, // C#5, 554 Hz (约)
    {0, 0, 315000000, false, 'n', 0}, // D5, 587 Hz (约)
    {0, 0, 334000000, false, 'n', 0}, // D#5, 622 Hz (约)
    {0, 0, 354000000, false, 'n', 0}, // E5, 659 Hz (约)
    {0, 0, 375000000, false, 'n', 0}, // F5, 698 Hz (约)
    {0, 0, 398000000, false, 'n', 0}, // F#5, 740 Hz (约)
    {0, 0, 421000000, false, 'n', 0}  // G5, 784 Hz (约)
};

bool g_update_canvas = true; // flag to update the canvas

// current_waves: 0: sine, 1: square, 2: triangle, 3: sawtooth
int current_waves[4] = {1, 0, 0, 0};

// 声波更新函数：全部采用定点运算，无浮点
void update_wave(wave_struct *w) {
    if (!w->is_playing) {
        if (w->adsr_stat == 'n') {
            w->output = 0;
            return;
        } else {
            // release 阶段
            w->adsr_stat = 'r';
            w->adsr_multi -= RELEASE_RATE;
            if (w->adsr_multi < 0) {
                w->adsr_multi = 0;
                w->adsr_stat = 'n';
            }
        }
    }
    w->phase += w->omega;
    int32_t sample = 0;
    uint32_t phase = w->phase;
    if (current_waves[0] != 0)
        sample += fixed_sine(phase) * current_waves[0];
    if (current_waves[1] != 0)
        sample += fixed_square(phase) * current_waves[1];
    if (current_waves[2] != 0)
        sample += fixed_triangle(phase) * current_waves[2];
    if (current_waves[3] != 0)
        sample += fixed_sawtooth(phase) * current_waves[3];
    // 应用包络乘子（Q15 乘 Q15 后除以 Q15_MAX）
    sample = (int32_t)(((int64_t)sample * w->adsr_multi) / Q15_MAX);
    w->output = sample;
    
    if (w->adsr_stat == 'a') {
        w->adsr_multi += ATTACK_RATE;
        if (w->adsr_multi >= Q15_MAX) {
            w->adsr_multi = Q15_MAX;
            w->adsr_stat = 'd';
        }
    } else if (w->adsr_stat == 'd') {
        if (w->adsr_multi > sustain_val) {
            w->adsr_multi -= DECAY_RATE;
            if (w->adsr_multi < sustain_val) {
                w->adsr_multi = sustain_val;
                w->adsr_stat = 's';
            }
        } else {
            w->adsr_stat = 's';
        }
    }
}

void update_all_waves() {
    for (int i = 0; i < 20; i++) {
        update_wave(&waves[i]);
    }
}

/*uint32_t get_all_waves_output() {
    int32_t temp_output = 0;
    for (int i = 0; i < 20; i++) {
        temp_output += waves[i].output;
    }
    // 将 Q15 输出映射到无符号 32 位范围
    uint32_t output = (uint32_t)(((int64_t)temp_output * 0x7FFFFFFF) / Q15_MAX);
    // printf("output: %d\n", output);
    return output;
}*/

uint32_t get_all_waves_output() {
    int32_t temp_output = 0;
    
    // Sum the wave outputs
    for (int i = 0; i < 20; i++) {
        temp_output += waves[i].output;
        
        // Clamp to prevent overflow
        if (temp_output > Q15_MAX * 20) temp_output = Q15_MAX * 20;
        if (temp_output < -Q15_MAX * 20) temp_output = -Q15_MAX * 20;
    }

    // Scale to the range 0x7FFFFFFF to 0x80000000
    int64_t scaled = ((int64_t)temp_output * 0x7FFFFFFF) / (Q15_MAX * 20);

    // Clamp again to ensure it fits in the expected range
    if (scaled > 0x7FFFFFFF) scaled = 0x7FFFFFFF;
    if (scaled < -0x80000000LL) scaled = -0x80000000LL;

    return (uint32_t)scaled;
}

int32_t get_wave_output(wave_struct *w) {
    return w->output;
}

#pragma endregion

#pragma region VGA Helper

volatile int pixel_buffer_start; // global variable
short int Buffer1[240][512]; // VGA 缓冲区

void swap(int* x, int* y) {
    int tmp = *x;
    *x = *y;
    *y = tmp;
}

void plot_pixel(int x, int y, short int line_color) {
    volatile short int *one_pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
    *one_pixel_address = line_color;
}

short int rgb_to_16bit(int r, int g, int b) {
    short int short_r = r * 31 / 255;
    short int short_g = g * 63 / 255;
    short int short_b = b * 31 / 255;
    return (short_r << 11) + (short_g << 5) + short_b;
}

void fill_background() {
    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            plot_pixel(x, y, rgb_to_16bit(40, 40, 40));
        }
    }
}

void draw_line(int x0, int y0, int x1, int y1, short int line_color) {
    bool is_steep = abs(y1 - y0) > abs(x1 - x0);

    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    int deltax = x1 - x0;
    int deltay = abs(y1 - y0);
    int error = -(deltax / 2);
    int y = y0;

    int y_step = 1;
    if (y0 >= y1) {
        y_step = -1;
    }

    for (int x = x0; x <= x1; x++) {
        if (is_steep) {
            plot_pixel(y, x, line_color);
        } else {
            plot_pixel(x, y, line_color);
        }
        error += deltay;
        if (error > 0) {
            y += y_step;
            error -= deltax;
        }
    }
}

void wait_for_vsync() {
    volatile int *pixel_ctrl_ptr = (volatile int *)0xFF203020;
    *pixel_ctrl_ptr = 1;
    while ((*(pixel_ctrl_ptr + 3) & 0x01) != 0) ;
}

void draw_rect(int x, int y, int width, int height, short int color) {
    draw_line(x, y, x + width, y, color);
    draw_line(x, y + height, x + width, y + height, color);
    draw_line(x, y, x, y + height, color);
    draw_line(x + width, y, x + width, y + height, color);
}

void fill_rect(rectangle rect, short int color) {
    for (int dx = 0; dx < rect.width; dx++) {
        for (int dy = 0; dy < rect.height; dy++) {
            plot_pixel(rect.x_base + dx, rect.y_base + dy, color);
        }
    }
}

void fill_rect_noinit(int x_base, int y_base, int width, int height, short int color) {
    rectangle temp = { x_base, y_base, width, height };
    fill_rect(temp, color);
}

#pragma endregion

#pragma region Drawing

// 这里图片数组等数据保持不变
short unsigned int sine[900] = { /* ...原始数据... */ };
void plot_image_sine(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, sine[i * 30 + j]);
}
void erase_image_sine(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, 0);
}

short unsigned int sawtooth[900] = { /* ...原始数据... */ };
void plot_image_sawtooth(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, sawtooth[i * 30 + j]);
}
void erase_image_sawtooth(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, 0);
}

short unsigned int square[900] = { /* ...原始数据... */ };
void plot_image_square(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, square[i * 30 + j]);
}
void erase_image_square(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, 0);
}

short unsigned int triangle[900] = { /* ...原始数据... */ };
void plot_image_triangle(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, triangle[i * 30 + j]);
}
void erase_image_triangle(int x, int y) {
    for (int i = 0; i < 30; i++)
        for (int j = 0; j < 30; j++)
            plot_pixel(x + j, y + i, 0);
}

void draw_wave_selection(int x, int y, int width, int height) {
    draw_rect(x, y, width, height, 0xFFFF);
    plot_image_sine(x + 20, y + 5);
    plot_image_sawtooth(x + 90, y + 5);
    plot_image_square(x + 150, y + 5);
    plot_image_triangle(x + 210, y + 5);
}

// 为预览绘制波形，全部采用整数运算
// 本例不再使用 wave_data_x 和 wave_data_y 浮点数组，直接计算每个采样点
int wave_data_plot_y[140] = {0};


void update_wave_data_y() {
    // 用 32 位相位的值，从 0 到 2π 对应 0 到 0xFFFFFFFF
    uint32_t phase_inc = 0x100000000UL / 140;
    for (int i = 0; i < 140; i++) {
        uint32_t phase = i * phase_inc;
        int32_t sum = 0;
        if (current_waves[0] != 0)
            sum -= fixed_sine(phase) * current_waves[0];
        if (current_waves[1] != 0)
            sum -= fixed_square(phase) * current_waves[1];
        if (current_waves[2] != 0)
            sum -= fixed_triangle(phase) * current_waves[2];
        if (current_waves[3] != 0)
            sum -= fixed_sawtooth(phase) * current_waves[3];
        int num_waves = 0;
        for (int j = 0; j < 4; j++) {
            num_waves += abs(current_waves[j]);
        }
        if(num_waves == 0) num_waves = 1;
        // 将 Q15 输出 sum 映射到 70 像素高度附近（基线 35 像素）
        int pixel_y = 35 + ( (int64_t)sum * 25 ) / (num_waves * Q15_MAX);
        wave_data_plot_y[i] = pixel_y;
    }
}

void draw_waveform(int x, int y, int width, int height) {
    fill_rect_noinit(x, y, width, height, rgb_to_16bit(0, 0, 0));
    update_wave_data_y();
    for (int i = 1; i < 140; i++) {
        draw_line(x + i - 1, y + wave_data_plot_y[i - 1],
                  x + i, y + wave_data_plot_y[i],
                  rgb_to_16bit(255, 182, 80));
    }
}

void draw_keybd() {
    for (int note_idx = 0; note_idx < 20; note_idx++) {
        for (int rect_idx = 0; rect_idx < notes[note_idx].num_rects; rect_idx++) {
            if (notes[note_idx].pressed)
                fill_rect(notes[note_idx].rectangles[rect_idx], rgb_to_16bit(255, 182, 80));
            else
                fill_rect(notes[note_idx].rectangles[rect_idx],
                          (notes[note_idx].num_rects == 1) ? rgb_to_16bit(0, 0, 0) : rgb_to_16bit(255, 255, 255));
        }
    }
}

void update_keybd(int note_idx) {
    for (int rect_idx = 0; rect_idx < notes[note_idx].num_rects; rect_idx++) {
        if (notes[note_idx].pressed)
            fill_rect(notes[note_idx].rectangles[rect_idx], rgb_to_16bit(255, 182, 80));
        else
            fill_rect(notes[note_idx].rectangles[rect_idx],
                      (notes[note_idx].num_rects == 1) ? rgb_to_16bit(0, 0, 0) : rgb_to_16bit(255, 255, 255));
    }
}

void draw_adsr() {
    const int shadow_disp = 5;
    const int knob_min = 145;
    const int knob_len = 70;
    // 绘制各包络基座
    fill_rect_noinit(46, 75, 8, 75, rgb_to_16bit(0, 0, 0)); // Attack
    fill_rect_noinit(66, 75, 8, 75, rgb_to_16bit(0, 0, 0)); // Delay
    fill_rect_noinit(246, 75, 8, 75, rgb_to_16bit(0, 0, 0)); // Sustain
    fill_rect_noinit(266, 75, 8, 75, rgb_to_16bit(0, 0, 0)); // Release
    // 绘制旋钮（注意：attack_val 等为 Q15，转换到像素位置时采用比例换算）
    int a_knob_pos = knob_min - ((attack_val * knob_len) / Q15_MAX);
    fill_rect_noinit(45, a_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
    fill_rect_noinit(45, a_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    int d_knob_pos = knob_min - ((decay_val * knob_len) / Q15_MAX);
    fill_rect_noinit(65, d_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
    fill_rect_noinit(65, d_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    int s_knob_pos = knob_min - ((sustain_val * knob_len) / Q15_MAX);
    fill_rect_noinit(245, s_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
    fill_rect_noinit(245, s_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    int r_knob_pos = knob_min - ((release_val * knob_len) / Q15_MAX);
    fill_rect_noinit(265, r_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
    fill_rect_noinit(265, r_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
}

void update_adsr(char changed) {
    const int shadow_disp = 5;
    const int knob_min = 145;
    const int knob_len = 70;
    if (changed == 'a') {
        fill_rect_noinit(46, 75, 8, 75, rgb_to_16bit(0, 0, 0));
        fill_rect_noinit(45, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        fill_rect_noinit(55, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        int a_knob_pos = knob_min - ((attack_val * knob_len) / Q15_MAX);
        fill_rect_noinit(45, a_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
        fill_rect_noinit(45, a_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    } else if (changed == 'd') {
        fill_rect_noinit(66, 75, 8, 75, rgb_to_16bit(0, 0, 0));
        fill_rect_noinit(65, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        fill_rect_noinit(75, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        int d_knob_pos = knob_min - ((decay_val * knob_len) / Q15_MAX);
        fill_rect_noinit(65, d_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
        fill_rect_noinit(65, d_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    } else if (changed == 's') {
        fill_rect_noinit(246, 75, 8, 75, rgb_to_16bit(0, 0, 0));
        fill_rect_noinit(245, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        fill_rect_noinit(255, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        int s_knob_pos = knob_min - ((sustain_val * knob_len) / Q15_MAX);
        fill_rect_noinit(245, s_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
        fill_rect_noinit(245, s_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    } else if (changed == 'r') {
        fill_rect_noinit(266, 75, 8, 75, rgb_to_16bit(0, 0, 0));
        fill_rect_noinit(265, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        fill_rect_noinit(275, 75, 1, 75, rgb_to_16bit(40, 40, 40));
        int r_knob_pos = knob_min - ((release_val * knob_len) / Q15_MAX);
        fill_rect_noinit(265, r_knob_pos + shadow_disp, 10, 2, rgb_to_16bit(60, 60, 60));
        fill_rect_noinit(265, r_knob_pos, 10, 5, rgb_to_16bit(90, 90, 90));
    }
}

void draw_main_screen() {
    fill_background();
    draw_wave_selection(30, 20, 260, 40);
    draw_waveform(90, 80, 140, 70);
    draw_keybd();
    draw_adsr();
}

void init_main_screen() {
    fill_background();
    draw_wave_selection(30, 20, 260, 40);
    draw_waveform(90, 80, 140, 70);
    draw_keybd();
    draw_adsr();
}

#pragma endregion

#pragma region Interrupts

// 以下中断函数保持原有逻辑，仅调用修改后的波形函数与 ADSR 更新（原来的浮点函数全部替换掉）

// 中断处理函数
static void handler(void) __attribute__ ((interrupt ("machine")));
void handler(void) {
    int mcause_value;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause_value));
    if (mcause_value == 0x80000012) // KEY port
        key_isr();
    else if (mcause_value == 0x80000016) // PS2
        ps2_isr();
    g_update_canvas = true;
}

void set_key() {
    volatile int *KEY_ptr = (int *) KEY_BASE;
    *(KEY_ptr + 3) = 0xF;
    *(KEY_ptr + 2) = 0xF;
}

void set_ps2() {
    ps2_port *ps2_ptr = (ps2_port *)PS2_BASE;
    unsigned int data = ps2_ptr->DATA;
    while ((data & 0x8000) != 0) {
        data = ps2_ptr->DATA;
    }
    ps2_ptr->CTRL |= 0x1;
}

void key_isr() {
    volatile int *key_ptr = (int *) KEY_BASE;
    volatile int *sw_ptr = (int *) SWITCH_BASE;
    int sw_state = *(sw_ptr);
    int key_pressed = *(key_ptr + 3) & 0xF;
    if ((sw_state & 0x1) == 0x1) {
        if (key_pressed == 1) {
            current_waves[SAW_IDX] += 1;
        } else if (key_pressed == 2) {
            current_waves[TRI_IDX] += 1;
        } else if (key_pressed == 4) {
            current_waves[SQR_IDX] += 1;
        } else if (key_pressed == 8) {
            current_waves[SIN_IDX] += 1;
        }
        draw_waveform(90, 80, 140, 70);
    } else if (((sw_state >> 1) & 0x1) == 0x1) {
        if (key_pressed == 1) {
            current_waves[SAW_IDX] -= 1;
        } else if (key_pressed == 2) {
            current_waves[TRI_IDX] -= 1;
        } else if (key_pressed == 4) {
            current_waves[SQR_IDX] -= 1;
        } else if (key_pressed == 8) {
            current_waves[SIN_IDX] -= 1;
        }
        draw_waveform(90, 80, 140, 70);
    } else if (((sw_state >> 2) & 0x1) == 0x1) {
        // attack
        if (key_pressed == 1) {
            fast_knob_change = !fast_knob_change;
        } else if (key_pressed == 2) {
            attack_val = Q15_MAX;
        } else if (key_pressed == 4) {
            if (attack_val - (fast_knob_change ? 3277 : 655) >= 0)
                attack_val -= (fast_knob_change ? 3277 : 655);
        } else if (key_pressed == 8) {
            if (attack_val + (fast_knob_change ? 3277 : 655) <= Q15_MAX)
                attack_val += (fast_knob_change ? 3277 : 655);
        }
        update_adsr('a');
    } else if (((sw_state >> 3) & 0x1) == 0x1) {
        // delay (对应 decay)
        if (key_pressed == 1) {
            fast_knob_change = !fast_knob_change;
        } else if (key_pressed == 2) {
            decay_val = Q15_MAX/2;
        } else if (key_pressed == 4) {
            if (decay_val - (fast_knob_change ? 3277 : 655) >= 0)
                decay_val -= (fast_knob_change ? 3277 : 655);
        } else if (key_pressed == 8) {
            if (decay_val + (fast_knob_change ? 3277 : 655) <= Q15_MAX)
                decay_val += (fast_knob_change ? 3277 : 655);
        }
        update_adsr('d');
    } else if (((sw_state >> 4) & 0x1) == 0x1) {
        // sustain
        if (key_pressed == 1) {
            fast_knob_change = !fast_knob_change;
        } else if (key_pressed == 2) {
            sustain_val = Q15_MAX;
        } else if (key_pressed == 4) {
            if (sustain_val - (fast_knob_change ? 3277 : 655) >= 0)
                sustain_val -= (fast_knob_change ? 3277 : 655);
        } else if (key_pressed == 8) {
            if (sustain_val + (fast_knob_change ? 3277 : 655) <= Q15_MAX)
                sustain_val += (fast_knob_change ? 3277 : 655);
        }
        update_adsr('s');
    } else if (((sw_state >> 5) & 0x1) == 0x1) {
        // release
        if (key_pressed == 1) {
            fast_knob_change = !fast_knob_change;
        } else if (key_pressed == 2) {
            release_val = 0;
        } else if (key_pressed == 4) {
            if (release_val - (fast_knob_change ? 3277 : 655) >= 0)
                release_val -= (fast_knob_change ? 3277 : 655);
        } else if (key_pressed == 8) {
            if (release_val + (fast_knob_change ? 3277 : 655) <= Q15_MAX)
                release_val += (fast_knob_change ? 3277 : 655);
        }
        update_adsr('r');
    } else {
        int probe_key_pressed = 1;
        for (int idx = 0; idx < 4; idx++) {
            current_waves[3 - idx] = (probe_key_pressed == key_pressed) ? 1 : 0;
            probe_key_pressed *= 2;
        }
        draw_waveform(90, 80, 140, 70);
    }
    *(key_ptr + 3) = 0xF;
}

void ps2_isr() {
    ps2_port *ps2_ptr = (ps2_port *)PS2_BASE;
    unsigned int data = ps2_ptr->DATA;
    if ((data & 0x8000) != 0) {
        bool break_code = false;
        uint8_t code = data & 0xFF;
        if (code == 0xF0) { 
            data = ps2_ptr->DATA;
            code = data & 0xFF;
            break_code = true;
        }
        for (int idx = 0; idx < 20; idx++) {
            if (code == notes[idx].code) {
                notes[idx].pressed = break_code ? false : true;
                if (waves[idx].is_playing != notes[idx].pressed) {
                    waves[idx].is_playing = notes[idx].pressed;
                    if (waves[idx].is_playing) {
                        waves[idx].adsr_stat = 'a';
                        waves[idx].adsr_multi = 0; // 从 0 开始上升
                    }
                    update_keybd(idx);
                }
            }
        }
    }
}

#pragma endregion

#pragma region Main

int main () {

    audio_s *audio_ptr = (audio_s *)AUDIO_BASE;
    led_s *led_ptr = (led_s *)LED_BASE;

    set_key();
    set_ps2();
    generate_sine_table();

    int mstatus_value, mtvec_value, mie_value;
    mstatus_value = 0b1000;
    __asm__ volatile ("csrc mstatus, %0" :: "r"(mstatus_value)); 
    mtvec_value = (int) &handler;
    __asm__ volatile ("csrw mtvec, %0" :: "r"(mtvec_value));
    __asm__ volatile ("csrr %0, mie" : "=r"(mie_value));
    __asm__ volatile ("csrc mie, %0" :: "r"(mie_value));
    mie_value = 0x40000;      // KEY interrupts IRQ14
    mie_value |= 0x400000;    // PS2 interrupts IRQ22
    __asm__ volatile ("csrs mie, %0" :: "r"(mie_value));
    __asm__ volatile ("csrs mstatus, %0" :: "r"(mstatus_value));

    volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    wait_for_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    init_main_screen();
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    init_main_screen();

    audio_ptr->CTRL = 0x8;
    audio_ptr->CTRL = 0x0;

    while (1) {
        bool status = audio_ptr->RALC == 0 || audio_ptr->WSLC == 0;
        if (!status) {
            led_ptr->LEDR = 0x1;
            update_all_waves();
            uint32_t output = get_all_waves_output();
            audio_ptr->LDATA = output;
            audio_ptr->RDATA = output;
        } else {
            led_ptr->LEDR = 0x2;
        }
        // 如需刷新屏幕，可调用 draw_main_screen();

    }
    return 0;
}

#pragma endregion
