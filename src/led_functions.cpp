#include "led_functions.h"

#ifdef LED_DISPLAY
//                Display #    1     2     3     4     5     6     7     8
int  DISP_ADD [MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};    // display I2C addresses

void setup_displays() {
  for (int n=0; n<MAX_DISP; n++)
  {
    disp_mat[n] = Adafruit_7segment();
    disp_mat[n].begin(DISP_ADD[n]);
    disp_mat[n].clear();
    disp_mat[n].drawColon(false);
    disp_mat[n].writeDisplay();

#ifdef DUAL_MODE
    disp_8x8[n] = Adafruit_8x8matrix();
    disp_8x8[n].begin(DISP_ADD[n]);
    disp_8x8[n].clear();
    disp_8x8[n].writeDisplay();
#endif
  }

}

void show_brightness_pattern() {
    char ctmp[10];

#ifdef LED_DISPLAY
    set_display_brightness();

    for (int n=0; n<NUM_LANES; n++)
    {
      sprintf(ctmp,"%d%03d", (n+1), (int)display_level);

      disp_mat[n].clear();

      disp_mat[n].writeDigitNum(0, char2int(ctmp[0]), false);
      disp_mat[n].writeDigitNum(1, char2int(ctmp[1]), false);
      disp_mat[n].writeDigitNum(3, char2int(ctmp[2]), false);
      disp_mat[n].writeDigitNum(4, char2int(ctmp[3]), false);

      disp_mat[n].drawColon(false);
      disp_mat[n].writeDisplay();

#ifdef DUAL_DISP
#ifdef DUAL_MODE
      disp_8x8[n+4].clear();
      disp_8x8[n+4].setTextSize(1);
      disp_8x8[n+4].setRotation(3);
      disp_8x8[n+4].setCursor(2, 0);
      disp_8x8[n+4].print("X");
      disp_8x8[n+4].writeDisplay();
#else
      disp_mat[n+4].clear();

      disp_mat[n+4].writeDigitNum(0, char2int(ctmp[0]), false);
      disp_mat[n+4].writeDigitNum(1, char2int(ctmp[1]), false);
      disp_mat[n+4].writeDigitNum(3, char2int(ctmp[2]), false);
      disp_mat[n+4].writeDigitNum(4, char2int(ctmp[3]), false);

      disp_mat[n+4].drawColon(false);
      disp_mat[n+4].writeDisplay();
#endif
#endif
    }
#endif  
}

void set_display_brightness(int display_level) {
    for (int n=0; n<MAX_DISP; n++) {
      disp_mat[n].setBrightness((int)display_level);
#ifdef DUAL_DISP
#ifdef DUAL_MODE
      disp_8x8[n+4].setBrightness((int)display_level);
#else
      disp_mat[n+4].setBrightness((int)display_level);
#endif
#endif
    }
}

/*================================================================================*
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void update_display(int lane, unsigned char msg[]) {
    disp_mat[lane].clear();

    #ifdef DUAL_DISP
    #ifdef DUAL_MODE
    disp_8x8[lane+4].clear();
    #else
    disp_mat[lane+4].clear();
    #endif
    #endif

  for (int d = 0; d<=4; d++)  {
    disp_mat[lane].writeDigitRaw(d, msg[d]);
#ifdef DUAL_DISP
#ifdef DUAL_MODE
    if (d == 3) {
      disp_8x8[lane+4].setTextSize(1);
      disp_8x8[lane+4].setRotation(3);
      disp_8x8[lane+4].setCursor(2, 0);
      if (msg == msgBlank)
         disp_8x8[lane+4].print(" ");
      else
         disp_8x8[lane+4].print("-");
    }
#else
    disp_mat[lane+4].writeDigitRaw(d, msg[d]);
#endif
#endif
  }

  disp_mat[lane].writeDisplay();
#ifdef DUAL_DISP
#ifdef DUAL_MODE
  disp_8x8[lane+4].writeDisplay();
#else
  disp_mat[lane+4].writeDisplay();
#endif
#endif

  return;
}

#endif