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

void main () {

    audio_s *audio_ptr = (audio_s *)AUDIO_BASE;
    parallel_port *key_ptr = (parallel_port *)KEY_BASE;
    parallel_port *sw_ptr = (parallel_port *)SWITCH_BASE;
    ps2_port *ps2_ptr = (ps2_port *)PS2_BASE;

    while (1) {

    }

    return 0;
}