"""
Main features (most important comes first):

Synthesis from simple waveforms (sine, square, sawtooth, triangle)
Graphical display of waveform
PS2 Keyboard act as MIDI controller
Additive or subtractive synthesis to generate custom sounds.
Filtering (low pass, high pass, band pass, or even custom ones if we have time)
ADSR Envelopes (attack, decay, sustain, release)
pointwise multiplication of a signal (i.e exponential) on top of the waveform
Navigating interface through input devices such as buttons and/or switches, on the DE1-SOC board
Extra features if we have time

Incorporating effects (reverb, delay)
Detuning / Unison effects
Making a fully graphical user interface utilizing user inputs from PS2 Mouse
Incorporating simple sequencers / piano roll

User Interaction Logic
SW0: Toggle addition mode
KEY 3-0 represent selecting the basic waveforms
SW1: Toggle subtraction mode
KEY 3-0 represent selecting the basic waveforms
SW2: Attack
KEY 3 represent increase
KEY 2 represent decrease
KEY 1 represent reset
SW3: Delay
KEY 3 represent increase
KEY 2 represent decrease
KEY 1 represent reset
SW4: Sustain
KEY 3 represent increase
KEY 2 represent decrease
KEY 1 represent reset
SW5: Release
KEY 3 represent increase
KEY 2 represent decrease
KEY 1 represent reset

Program interrupt logic
Inside PS2 keyboard interrupt
Inside SW interrupt
depending on which switch is on, toggle global variable representing mode
Inside KEY interrupt
different action depending on different mode


Synthesis from existing Waveforms + PS2 Keyboard act as MIDI controller
Assumes that a complete waveform is provided
Pitch shift waveform depending on the note (key pressed) 
x(t) → x(2t) oscilate twice as fast
note: make sure to account for the scenario where multiple keys are pressed




int wave[] = {0. 1, 2, 3, 4, 3, 2, 1, 0}

display between h = 100 pixel and h = 200 pixel

(0, 100), (5, 110)
connect the points in between with a line



Graphical display of waveform
Navigating interface through input devices such as buttons and/or switches, on the DE1-SOC board
"""

import pygame
import numpy as np

SCALE = 3
FPS = 60

pygame.init()

screen = pygame.display.set_mode((320 * SCALE, 240 * SCALE))

# waveform related
N0 = 100
w0 = 2 * np.pi * 5
waveform_x = np.linspace(0, w0, N0)
waveform_y = np.sin(waveform_x)
# waveform selection related
img_sine = pygame.image.load('images/sine.png')
img_square = pygame.image.load('images/square.png')
img_sawtooth = pygame.image.load('images/sawtooth.png')
img_triangle = pygame.image.load('images/triangle.png')
img_sine = pygame.transform.scale(img_sine, (20 * SCALE, 10 * SCALE))
img_square = pygame.transform.scale(img_square, (20 * SCALE, 10 * SCALE))
img_sawtooth = pygame.transform.scale(img_sawtooth, (20 * SCALE, 10 * SCALE))
img_triangle = pygame.transform.scale(img_triangle, (20 * SCALE, 10 * SCALE))


def draw_main_screen():
    draw_waveform()
    draw_adsr()
    draw_keybd()
    draw_wave_selection()
    pygame.display.flip()


def draw_waveform():
    x_min, x_max, y_min, y_max = 40, 200, 80, 160
    x_len, y_len = x_max - x_min, y_max - y_min
    y_mean = (y_min + y_max) // 2
    pygame.draw.rect(screen, (0, 255, 0), (x_min * SCALE, y_min * SCALE, x_len * SCALE, y_len * SCALE), SCALE)
    for i in range(1, len(waveform_x)):
        x1, x2 = waveform_x[i - 1], waveform_x[i]
        y1, y2 = waveform_y[i - 1], waveform_y[i]
        x1 = x_min + x1 * x_len / w0
        x2 = x_min + x2 * x_len / w0
        y1 = y_mean - y1 * y_len / 2
        y2 = y_mean - y2 * y_len / 2
        pygame.draw.line(screen, (255, 200, 200), (x1 * SCALE, y1 * SCALE), (x2 * SCALE, y2 * SCALE), SCALE)


def draw_adsr():
    x_min, x_max, y_min, y_max = 220, 280, 80, 160
    x_len, y_len = x_max - x_min, y_max - y_min
    pygame.draw.rect(screen, (255, 255, 0), (x_min * SCALE, y_min * SCALE, x_len * SCALE, y_len * SCALE), SCALE)


def draw_keybd():
    x_min, x_max, y_min, y_max = 40, 280, 180, 220
    x_len, y_len = x_max - x_min, y_max - y_min
    pygame.draw.rect(screen, (255, 0, 0), (x_min * SCALE, y_min * SCALE, x_len * SCALE, y_len * SCALE), SCALE)


def draw_wave_selection():
    x_min, x_max, y_min, y_max = 40, 280, 20, 60
    x_len, y_len = x_max - x_min, y_max - y_min
    pygame.draw.rect(screen, (0, 0, 255), (x_min * SCALE, y_min * SCALE, x_len * SCALE, y_len * SCALE), SCALE)
    dx_sine, dx_square, dx_sawtooth, dx_triangle = 20, 60, 100, 140
    dy_img = 20
    screen.blit(img_sine, (dx_sine * SCALE + x_min * SCALE, dy_img * SCALE + y_min * SCALE))
    screen.blit(img_square, (dx_square * SCALE + x_min * SCALE, dy_img * SCALE + y_min * SCALE))
    screen.blit(img_sawtooth, (dx_sawtooth * SCALE + x_min * SCALE, dy_img * SCALE + y_min * SCALE))
    screen.blit(img_triangle, (dx_triangle * SCALE + x_min * SCALE, dy_img * SCALE + y_min * SCALE))


if __name__ == "__main__":
    clock = pygame.time.Clock()
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        draw_main_screen()
        clock.tick(FPS)
    pygame.quit()