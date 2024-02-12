#ifndef LED_VARS_H
#define LED_VARS_H

//#define LED_DISPLAY  1                 // Enable lane place/time displays

#ifdef LED_DISPLAY

#include "Wire.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#define MAX_DISP       8                 // number of displays

#ifdef LARGE_DISP
unsigned char msgGateC[] = {0x6D, 0x41, 0x00, 0x0F, 0x07};  // S=CL
unsigned char msgGateO[] = {0x6D, 0x41, 0x00, 0x3F, 0x5E};  // S=OP
unsigned char msgLight[] = {0x41, 0x41, 0x00, 0x00, 0x07};  // == L
unsigned char msgDark [] = {0x41, 0x41, 0x00, 0x00, 0x73};  // == d
#else
unsigned char msgGateC[] = {0x6D, 0x48, 0x00, 0x39, 0x38};  // S=CL
unsigned char msgGateO[] = {0x6D, 0x48, 0x00, 0x3F, 0x73};  // S=OP
unsigned char msgLight[] = {0x48, 0x48, 0x00, 0x00, 0x38};  // == L
unsigned char msgDark [] = {0x48, 0x48, 0x00, 0x00, 0x5e};  // == d
#endif
unsigned char msgDashT[] = {0x40, 0x40, 0x00, 0x40, 0x40};  // ----
unsigned char msgDashL[] = {0x00, 0x00, 0x00, 0x40, 0x00};  //   -
unsigned char msgBlank[] = {0x00, 0x00, 0x00, 0x00, 0x00};  // (blank)

Adafruit_7segment disp_mat[MAX_DISP];

#ifdef DUAL_MODE                       // uses 8x8 matrix displays
Adafruit_8x8matrix disp_8x8[MAX_DISP];
#endif

void setup_displays();
void show_brightness_pattern();
void set_display_brightness(int display_level);
void update_display(int lane, unsigned char msg[]);

#endif //LED_DISPLAY

#endif //LED_VARS_H
