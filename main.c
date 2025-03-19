#include <stdio.h>
#include <stdbool.h>

#define AUDIO_BASE 0xFF203040
#define KEY_BASE 0xFF200050
#define SWITCH_BASE 0xFF200040
#define PS2_BASE 0xFF200100

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

static void handler(void) __attribute__ ((interrupt ("machine")));
void set_key();
void set_ps2();
void key_isr();
void ps2_isr();


int main () {

    audio_s *audio_ptr = (audio_s *)AUDIO_BASE;
    parallel_port *key_ptr = (parallel_port *)KEY_BASE;
    parallel_port *sw_ptr = (parallel_port *)SWITCH_BASE;
    ps2_port *ps2_ptr = (ps2_port *)PS2_BASE;

    // Setup I/O devices to handle interrupts
    set_key();

    // Setup Control Registers to handle interrupts
    int mstatus_value, mtvec_value, mie_value;
    mstatus_value = 0b1000; // interrupt bit mask to disable interrupts
    __asm__ volatile ("csrc mstatus, %0" :: "r"(mstatus_value)); 

    mtvec_value = (int) &handler; // set trap address
    __asm__ volatile ("csrw mtvec, %0" :: "r"(mtvec_value));

    // disable all interrupts that are currently enabled
    __asm__ volatile ("csrr %0, mie" : "=r"(mie_value));
    __asm__ volatile ("csrc mie, %0" :: "r"(mie_value));

    mie_value = 0x40000; // KEY interrupts
    __asm__ volatile ("csrs mie, %0" :: "r"(mie_value)); // set interrupt enables
    __asm__ volatile ("csrs mstatus, %0" :: "r"(mstatus_value)); // enable Nios V interrupts

    printf("setup interrupts\n");

    while (1) {

    }

    return 0;
}

void handler (void){
    printf("inside handler\n");
    int mcause_value;
    __asm__ volatile ("csrr %0, mcause" : "=r"(mcause_value));
    if (mcause_value == 0x80000012) // KEY port
        key_isr();
    // else, ignore the trap
}
    

void set_key() {
    volatile int *KEY_ptr = (int *) KEY_BASE;
    *(KEY_ptr + 3) = 0xF; // clear EdgeCapture register
    *(KEY_ptr + 2) = 0xF; // enable interrupts for all KEYs
}

void key_isr() {
    printf("inside key isr\n");
    volatile int *key_ptr = (int *) KEY_BASE;
    volatile int *sw_ptr = (int *) SWITCH_BASE;

    int sw_state = *(sw_ptr);
    int key_pressed = *(key_ptr + 3); // read EdgeCapture
    key_pressed &= 0xF;

    if ((sw_state & 0x1) == 0x1) {
        printf("addition mode code goes here\n");
    } else if (((sw_state >> 1) & 0x1) == 0x1) {
        printf("subtraction mode code goes here\n");
    } else if (((sw_state >> 2) & 0x1) == 0x1) {
        printf("attack\n");
    } else if (((sw_state >> 3) & 0x1) == 0x1) {
        printf("delay\n");
    } else if (((sw_state >> 4) & 0x1) == 0x1) {
        printf("sustain\n");
    } else if (((sw_state >> 5) & 0x1) == 0x1) {
        printf("release\n");
    } 

    *(key_ptr + 3) = 0xF; // clear EdgeCapture register
}