#ifndef MATRIX_VARS_H
#define MATRIX_VARS_H

//import global config
#define NUM_LANES    4

#define DATA_PIN       6               //Data Pin
#define CLK_PIN        7               //Clock Pin
#define CS_PIN         13              //Chip Select Pin - reusing this pin instead of solenoid
#define NUM_MATRICES   4               //Number of 8x8 matrices/modules in use

void setup_displays();
void showChar(int addr, char c_char);
void show_brightness_pattern(int display_level);
void set_display_brightness(int display_level);

#endif //MATRIX_VARS_H