#include <LedControl_SW_SPI.h>
#include "matrix_functions.h"

LedControl_SW_SPI lc=LedControl_SW_SPI();
byte zero[8]= {B00000000,B00000000,B01111100,B10100010,B10010010,B10001010,B01111100,B00000000};
byte one[8] = {B00000000,B00000000,B00000000,B11111110,B01000000,B00100000,B00000000,B00000000};
byte two[8] = {B00000000,B00000000,B01100010,B10010010,B10010010,B10010010,B10001110,B00000000};
byte three[8]={B00000000,B00000000,B01101100,B10010010,B10010010,B10010010,B10000010,B00000000};
byte four[8]= {B00000000,B00000000,B11111110,B00010000,B00010000,B00010000,B11110000,B00000000};
byte ltro[8]= {B00000000,B00000000,B01111100,B10000010,B10000010,B10000010,B01111100,B00000000};
byte ltrp[8]= {B00000000,B00000000,B01100000,B10010000,B10010000,B10010000,B11111110,B00000000};
byte dash[8]= {B00000000,B00000000,B00001000,B00001000,B00001000,B00000000,B00000000,B00000000};
byte plus[8]= {B00000000,B00000000,B00000000,B00010000,B00111000,B00010000,B00000000,B00000000};

void showChar(int addr, char c_char) {
  if(c_char == '0')
      for (int i=0;i<8;i++) lc.setRow(addr, i, zero[i]);
  else if(c_char == '1')
      for (int i=0;i<8;i++) lc.setRow(addr, i, one[i]);
  else if(c_char == '2')
      for (int i=0;i<8;i++) lc.setRow(addr, i, two[i]);
  else if(c_char == '3')
      for (int i=0;i<8;i++) lc.setRow(addr, i, three[i]);
  else if(c_char == '4')
      for (int i=0;i<8;i++) lc.setRow(addr, i, four[i]);
  else if(c_char == 'O')
      for (int i=0;i<8;i++) lc.setRow(addr, i, ltro[i]);
  else if(c_char == 'P')
      for (int i=0;i<8;i++) lc.setRow(addr, i, ltrp[i]);
  else if(c_char == '-')
      for (int i=0;i<8;i++) lc.setRow(addr, i, dash[i]);
  else if(c_char == '+')
      for (int i=0;i<8;i++) lc.setRow(addr, i, plus[i]);
  else //blank
      for (int i=0;i<8;i++) lc.setRow(addr, i, B00000000);

}

void setup_displays() {
  //7 segment displays are chained after the matrices
  lc.begin(DATA_PIN,CLK_PIN,CS_PIN,NUM_MATRICES);

  for (int i=0;i<NUM_MATRICES;i++) {
    Serial.println("Init display");
    lc.shutdown(i,false); //wakeup
    lc.clearDisplay(i);
  }

  //now show lane numbers
  for (int n=0;n<NUM_LANES;n++) {
    showChar(n, (char)49+n);
  }

  if (NUM_MATRICES > NUM_LANES) { //uses both sides of finish line
    for (int n=0;n<NUM_LANES;n++) {
      showChar(NUM_LANES+n, (char)48+NUM_LANES-n);
    }
  }

  delay(5000);
}

void show_brightness_pattern(int display_level) {
  for (int n=0; n<NUM_MATRICES; n++) {
    showChar(n, '0');
  }
}

void set_display_brightness(int display_level) {
  for (int i=0;i<NUM_MATRICES;i++) {
    lc.setIntensity(i,display_level);
  }
}