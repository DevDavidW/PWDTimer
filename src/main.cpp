#include <Arduino.h>

/*================================================================================*
   Pinewood Derby Timer                                Version 3.10 - 12 Dec 2020
   www.dfgtec.com/pdt

   Flexible and affordable Pinewood Derby timer that interfaces with the 
   following software:
     - PD Test/Tune/Track Utility
     - Grand Prix Race Manager software

   Refer to the website for setup and usage instructions.


   Copyright (C) 2011-2020 David Gadberry

   This work is licensed under the Creative Commons Attribution-NonCommercial-
   ShareAlike 3.0 Unported License. To view a copy of this license, visit 
   http://creativecommons.org/licenses/by-nc-sa/3.0/ or send a letter to 
   Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 
   94041, USA.

   Modified Jan 2022 by David Williams to add ability to timeout a lane when it exceeds the NULL_TIME
   Modified Feb 2023 by David Williams to add 8x8 matrix with MAX7219 chip using LedControl library
   - Since we only use 4 lanes, using pins for lane 5 and 6 along with Solenoid pin to drive the displays
 *================================================================================*/

/*-----------------------------------------*
  - TIMER CONFIGURATION -
 *-----------------------------------------*/
#define NUM_LANES    4                 // number of lanes
#define GATE_RESET   0                 // Enable closing start gate to reset timer

//#define LED_DISPLAY  1                 // Enable lane place/time displays
//#define DUAL_DISP    1                 // dual displays per lane (4 lanes max)
//#define DUAL_MODE    1                 // dual display mode
//#define LARGE_DISP   1                 // utilize large Adafruit displays (see website)

#define SHOW_PLACE   1                 // Show place mode
#define PLACE_DELAY  3                 // Delay (secs) when displaying place/time
#define MIN_BRIGHT   0                 // minimum display brightness (0-15)
#define MAX_BRIGHT   15                // maximum display brightness (0-15)

#define MATRIX_DISPLAY 1               //Enable use of 8x8 matrices
#ifdef MATRIX_DISPLAY
#define DATA_PIN       6               //Data Pin
#define CLK_PIN        7               //Clock Pin
#define CS_PIN         13              //Chip Select Pin - reusing this pin instead of solenoid
#define NUM_MATRICES   8               //Number of 8x8 matrices/modules in use
#endif

#define ENABLE_TIMEOUT  1              // Enable line timeout (time > NULL_TIME)
/*-----------------------------------------*
  - END -
 *-----------------------------------------*/


#ifdef LED_DISPLAY                     // LED control libraries
#include "Wire.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#endif
#ifdef MATRIX_DISPLAY
#include <LedControl_SW_SPI.h>

LedControl_SW_SPI lc=LedControl_SW_SPI();
byte zero[8]={B00000000, B00000000, B01111100,B10100010,B10010010,B10001010,B01111100,B00000000};
byte one[8]={B00000000,B00000000,B00000000,B11111110,B01000000,B00100000,B00000000,B00000000};
byte two[8]={B00000000,B00000000,B01100010,B10010010,B10010010,B10010010,B10001110,B00000000};
byte three[8]={B00000000,B00000000,B01101100,B10010010,B10010010,B10010010,B10000010,B00000000};
byte four[8]={B00000000,B00000000,B11111110,B00010000,B00010000,B00010000,B11110000,B00000000};
byte ltro[8]={B00000000, B00000000, B01111100,B10000010,B10000010,B10000010,B01111100,B00000000};
byte ltrp[8]={B00000000,B00000000,B01100000,B10010000,B10010000,B10010000,B11111110,B00000000};
byte dash[8]={B00000000,B00000000,B00001000,B00001000,B00001000,B00000000,B00000000,B00000000};
byte plus[8]={B00000000,B00000000,B00000000,B00010000,B00111000,B00010000,B00000000,B00000000};
#endif

/*-----------------------------------------*
  - static definitions -
 *-----------------------------------------*/
#define PDT_VERSION  "3.20"            // software version
#define MAX_LANE     6                 // maximum number of lanes (Uno)
#define MAX_DISP     8                 // maximum number of displays (Adafruit)

#define mREADY       0                 // program modes
#define mRACING      1
#define mFINISH      2
#define mTEST        3

#define START_TRIP   HIGH              // start switch trip condition
#define NULL_TIME    9.999             // null (non-finish) time
#define NUM_DIGIT    4                 // timer resolution (# of decimals)
#define DISP_DIGIT   4                 // total number of display digits

#define PWM_LED_ON   220
#define PWM_LED_OFF  255
#define char2int(c) (c - '0') 

//
// serial messages                        <- to timer
//                                        -> from timer
//
#define SMSG_ACKNW   '.'               // -> acknowledge message

#define SMSG_POWER   'P'               // -> start-up (power on or hard reset)

#define SMSG_CGATE   'G'               // <- check gate
#define SMSG_GOPEN   'O'               // -> gate open

#define SMSG_RESET   'R'               // <- reset
#define SMSG_READY   'K'               // -> ready

#define SMSG_SOLEN   'S'               // <- start solenoid
#define SMSG_START   'B'               // -> race started
#define SMSG_FORCE   'F'               // <- force end
#define SMSG_RSEND   'Q'               // <- resend race data

#define SMSG_LMASK   'M'               // <- mask lane
#define SMSG_UMASK   'U'               // <- unmask all lanes

#define SMSG_GVERS   'V'               // <- request timer version
#define SMSG_DEBUG   'D'               // <- toggle debug on/off
#define SMSG_GNUML   'N'               // <- request number of lanes
#define SMSG_TINFO   'I'               // <- request timer information

#define SMSG_PACK    '2'               // <- show pack on displays
#define SMSG_LANES   'L'               // <- show lanes on displays
#define SMSG_CHECK   'C'               // <- start lane sensor check


/*-----------------------------------------*
  - pin assignments -
 *-----------------------------------------*/
byte BRIGHT_LEV   = A0;                // brightness level
byte RESET_SWITCH =  8;                // reset switch
byte STATUS_LED_R =  9;                // status LED (red)
byte STATUS_LED_B = 10;                // status LED (blue)
byte STATUS_LED_G = 11;                // status LED (green)
byte START_GATE   = 12;                // start gate switch
#ifndef MATRIX_DISPLAY
byte START_SOL    = 13;                // start solenoid
#endif

//                Display #    1     2     3     4     5     6     7     8
int  DISP_ADD [MAX_DISP] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};    // display I2C addresses

//                   Lane #    1     2     3     4     5     6
byte LANE_DET [MAX_LANE] = {   2,    3,    4,    5,    6,    7};                // finish detection pins

/*-----------------------------------------*
  - global variables -
 *-----------------------------------------*/
boolean       fDebug = false;          // debug flag
boolean       ready_first;             // first pass in ready state flag
boolean       finish_first;            // first pass in finish state flag

unsigned long start_time;              // race start time (microseconds)
unsigned long lane_time  [MAX_LANE];   // lane finish time (microseconds)
int           lane_place [MAX_LANE];   // lane finish place
boolean       lane_mask  [MAX_LANE];   // lane mask status

int           serial_data;             // serial data
byte          mode;                    // current program mode

float         display_level = -1.0;    // display brightness level

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

#ifdef LED_DISPLAY                     // LED display control
Adafruit_7segment disp_mat[MAX_DISP];
#endif

#ifdef DUAL_MODE                       // uses 8x8 matrix displays
Adafruit_8x8matrix disp_8x8[MAX_DISP];
#endif

//method declarations
void initialize(boolean powerup=false);
void dbg(int, const char * msg, int val=-999);
void smsg(char msg, boolean crlf=true);
void smsg_str(const char * msg, boolean crlf=true);
void timer_ready_state();
void timer_racing_state();
void set_status_led();
void set_display_brightness();
void update_display(int lane, unsigned char msg[]);
void update_display(int lane, int display_place, unsigned long display_time, int display_mode);
void send_timer_info();
void test_pdt_hw();
void check_lane_sensors();
void clear_displays();
int get_serial_data();
void unmask_all_lanes();
void send_race_results();
void display_race_results();
void process_general_msgs();
void timer_finished_state();
#ifdef MATRIX_DISPLAY
void showChar(int addr, char c_char);
#endif

/*================================================================================*
  SETUP TIMER
 *================================================================================*/
void setup()
{  
/*-----------------------------------------*
  - hardware setup -
 *-----------------------------------------*/
  pinMode(STATUS_LED_R, OUTPUT);
  pinMode(STATUS_LED_B, OUTPUT);
  pinMode(STATUS_LED_G, OUTPUT);
  #ifndef MATRIX_DISPLAY
  pinMode(START_SOL,    OUTPUT);
  #endif
  pinMode(RESET_SWITCH, INPUT);
  pinMode(START_GATE,   INPUT);
  pinMode(BRIGHT_LEV,   INPUT);
  
  digitalWrite(RESET_SWITCH, HIGH);    // enable pull-up resistor
  digitalWrite(START_GATE,   HIGH);    // enable pull-up resistor
  #ifndef MATRIX_DISPLAY
  digitalWrite(START_SOL, LOW);
  #endif

#ifdef LED_DISPLAY
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
#endif
#ifdef MATRIX_DISPLAY
  lc.begin(DATA_PIN,CLK_PIN,CS_PIN,NUM_MATRICES);

  for (int i=0;i<NUM_MATRICES;i++) {
    lc.shutdown(i,false); //wakeup
    lc.clearDisplay(i);
  }

  //show Pack number for a few seconds at startup
  showChar(0, '0');
  showChar(1, '2');
  showChar(2, '2');
  showChar(3, 'P');
  if (NUM_MATRICES==8) {
    showChar(4, '0');
    showChar(5, '2');
    showChar(6, '2');
    showChar(7, 'P');
  }

  delay(10000);

  //now show lane numbers for a few seconds
  showChar(0, '1');
  showChar(1, '2');
  showChar(2, '3');
  showChar(3, '4');
  if (NUM_MATRICES==8) {
  showChar(4, '4');
  showChar(5, '3');
  showChar(6, '2');
  showChar(7, '1');
  }
  delay(10000);

#endif

  for (int n=0; n<NUM_LANES; n++)
  {
    pinMode(LANE_DET[n], INPUT);

    digitalWrite(LANE_DET[n], HIGH);   // enable pull-up resistor
  }
  set_display_brightness();

/*-----------------------------------------*
  - software setup -
 *-----------------------------------------*/
  Serial.begin(9600);
  smsg(SMSG_POWER);

/*-----------------------------------------*
  - check for test mode -
 *-----------------------------------------*/
  if (digitalRead(RESET_SWITCH) == LOW)
  {
    mode = mTEST;
    test_pdt_hw();
  }

/*-----------------------------------------*
  - initialize timer -
 *-----------------------------------------*/
  initialize(true);
  unmask_all_lanes();
}


/*================================================================================*
  MAIN LOOP
 *================================================================================*/
void loop()
{
  process_general_msgs();

  switch (mode)
  {
    case mREADY:
      timer_ready_state();
      break;
    case mRACING:
      timer_racing_state();
      break;
    case mFINISH:
      timer_finished_state();
      break;
  }
}


/*================================================================================*
  TIMER READY STATE
 *================================================================================*/
void timer_ready_state()
{
  if (ready_first)
  {
    set_status_led();
    clear_displays();

    ready_first = false;
  }
  #ifndef MATRIX_DISPLAY
  if (serial_data == int(SMSG_SOLEN))    // activate start solenoid
  {
    digitalWrite(START_SOL, HIGH);
    smsg(SMSG_ACKNW);
  }
  #endif
  
  if (digitalRead(START_GATE) == START_TRIP)    // timer start
  {
    start_time = micros();

    #ifndef MATRIX_DISPLAY
    digitalWrite(START_SOL, LOW);
    #endif

    smsg(SMSG_START);
    delay(100); 

    mode = mRACING; 
  }

  return;
}
  

/*================================================================================*
  TIMER RACING STATE
 *================================================================================*/
void timer_racing_state()
{
  int lanes_left, finish_order, lane_status[NUM_LANES];
  unsigned long current_time, last_finish_time;


  set_status_led();
  clear_displays();

  finish_order = 0;
  last_finish_time = 0;

  lanes_left = NUM_LANES;
  for (int n=0; n<NUM_LANES; n++)
  {
    if (lane_mask[n]) lanes_left--;
  }

  while (lanes_left)
  {
    current_time = micros();

    for (int n=0; n<NUM_LANES; n++) lane_status[n] = bitRead(PIND, LANE_DET[n]);    // read status of all lanes

    for (int n=0; n<NUM_LANES; n++)
    {
      if (lane_time[n] == 0 && lane_status[n] == HIGH && !lane_mask[n])    // car has crossed finish line
      {
        lanes_left--;

        lane_time[n] = current_time - start_time;

        if (lane_time[n] > last_finish_time)
        {
          finish_order++;
          last_finish_time = lane_time[n];
        }
        lane_place[n] = finish_order;

        update_display(n, lane_place[n], lane_time[n], SHOW_PLACE);
      } 
#ifdef ENABLE_TIMEOUT
      else if (lane_time[n] == 0 && !lane_mask[n] && current_time > (start_time + (NULL_TIME * 1000000.0)) )    //lane timeout
      {
        lanes_left--;
        
        lane_time[n] = NULL_TIME * 1000000.0;
        if (lane_time[n] > last_finish_time)
        {
          finish_order++;
          last_finish_time = lane_time[n];
        }
        lane_place[n] = finish_order;        
        dbg(fDebug, "Timeout lane: ", n+1);
        
        update_display(n, lane_place[n], lane_time[n], SHOW_PLACE);
      }
#endif
    }
    
    serial_data = get_serial_data();

    if (serial_data == int(SMSG_FORCE) || serial_data == int(SMSG_RESET) || digitalRead(RESET_SWITCH) == LOW)    // force race to end
    {
      lanes_left = 0;
      smsg(SMSG_ACKNW);
    }
  }
    
  send_race_results();

  mode = mFINISH;

  return;
}


/*================================================================================*
  TIMER FINISHED STATE
 *================================================================================*/
void timer_finished_state()
{
  if (finish_first)
  {
    set_status_led();
    finish_first = false;
  }

  if (GATE_RESET && digitalRead(START_GATE) != START_TRIP)    // gate closed
  {
    delay(500);    // ignore any switch bounce

    if (digitalRead(START_GATE) != START_TRIP)    // gate still closed
    {
      initialize();    // reset timer
    } 
  } 

  if (serial_data == int(SMSG_RSEND))    // resend race data
  {
      smsg(SMSG_ACKNW);
      send_race_results();
  } 

  set_display_brightness();
  display_race_results();

  return;
}


/*================================================================================*
  PROCESS GENERAL SERIAL MESSAGES
 *================================================================================*/
void process_general_msgs()
{
  int lane;
  char tmps[50];


  serial_data = get_serial_data();

  if (serial_data == int(SMSG_GVERS))    // get software version
  {
      sprintf(tmps, "vert=%s", PDT_VERSION);
      smsg_str(tmps);
  } 

  else if (serial_data == int(SMSG_GNUML))    // get number of lanes
  {
      sprintf(tmps, "numl=%d", NUM_LANES);
      smsg_str(tmps);
  } 

  else if (serial_data == int(SMSG_TINFO))    // get timer information
  {
      send_timer_info();
  } 

  else if (serial_data == int(SMSG_DEBUG))    // toggle debug
  {
    fDebug = !fDebug;
    dbg(true, "toggle debug = ", fDebug);
  } 

  else if (serial_data == int(SMSG_CGATE))    // check start gate
  {
    if (digitalRead(START_GATE) == START_TRIP)    // gate open
    {
      smsg(SMSG_GOPEN);
    } 
    else
    {
      smsg(SMSG_ACKNW);
    } 
  } 

  else if (serial_data == int(SMSG_RESET) || digitalRead(RESET_SWITCH) == LOW)    // timer reset
  {
    if (digitalRead(START_GATE) != START_TRIP)    // only reset if gate closed
    {
      initialize();
    } 
    else
    {
      smsg(SMSG_GOPEN);
    } 
  } 

  else if (serial_data == int(SMSG_LMASK))    // lane mask
  {
    delay(100);
    serial_data = get_serial_data();

    lane = serial_data - 48;
    if (lane >= 1 && lane <= NUM_LANES)
    {
      lane_mask[lane-1] = true;

      dbg(fDebug, "set mask on lane = ", lane);
    }
    smsg(SMSG_ACKNW);
  }

  else if (serial_data == int(SMSG_UMASK))    // unmask all lanes
  {
    unmask_all_lanes();
    smsg(SMSG_ACKNW);
  }

  else if (serial_data == int(SMSG_PACK)) //show pack number on displays
  {
    showChar(0, '0');
    showChar(1, '2');
    showChar(2, '2');
    showChar(3, 'P');

    if (NUM_MATRICES==8) {
      showChar(4, '0');
      showChar(5, '2');
      showChar(6, '2');
      showChar(7, 'P');
    }
  
  }

  else if (serial_data == int(SMSG_LANES)) //show lane numbers on displays
  {
    showChar(0, '1');
    showChar(1, '2');
    showChar(2, '3');
    showChar(3, '4');

    if (NUM_MATRICES==8) {
    showChar(4, '4');
    showChar(5, '3');
    showChar(6, '2');
    showChar(7, '1');
    }
 
  }

   else if (serial_data == int(SMSG_CHECK)) //start lane sensor check
  {
    mode = mTEST;
    smsg(SMSG_ACKNW);
    check_lane_sensors();
  }

  return;
}


/*================================================================================*
  TEST PDT FINISH DETECTION HARDWARE
 *================================================================================*/
void test_pdt_hw()
{
  int  lane_status[NUM_LANES];
#ifdef LED_DISPLAY
  char ctmp[10];
#endif

  smsg_str("TEST MODE");
  set_status_led();
  delay(2000); 

/*-----------------------------------------*
   show status of lane detectors
 *-----------------------------------------*/
  while(true)
  {
    for (int n=0; n<NUM_LANES; n++)
    {
      lane_status[n] = bitRead(PIND, LANE_DET[n]);    // read status of all lanes
#ifndef MATRIX_DISPLAY
      if (lane_status[n] == HIGH)
      {
        update_display(n, msgDark);
      }
      else
      {
        update_display(n, msgLight);
      }
  #else
      if (lane_status[n] == HIGH) {
        showChar(n, '+');
      } else {
        showChar(n, 'O');
      }
  #endif
    }

    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      delay(1000); 
      break;
    }
    delay(100);
  }

/*-----------------------------------------*
   show status of start gate switch
 *-----------------------------------------*/
  while(true)
  {
#ifndef MATRIX_DISPLAY
    if (digitalRead(START_GATE) == START_TRIP) 
    {
      update_display(0, msgGateO);
    }
    else
    {
      update_display(0, msgGateC);
    }
#else
    if (digitalRead(START_GATE) == START_TRIP) {
      showChar(0, '0');
    } else {
      showChar(0, '-');
    }
#endif
    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      delay(1000); 
      break;
    }
    delay(100);
  }

/*-----------------------------------------*
   show pattern for brightness adjustment
 *-----------------------------------------*/
  while(true)
  {
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

#ifdef MATRIX_DISPLAY
    set_display_brightness();
    showChar(0, '0');
#endif

    if (digitalRead(RESET_SWITCH) == LOW)  // break out of this test
    {
      clear_displays();
      break;
    }
    delay(1000);
  }
}


/*-----------------------------------------*
   show status of lane detectors
 *-----------------------------------------*/
void check_lane_sensors() {
  int  lane_status[NUM_LANES];
  
  //smsg_str("LANE CHECK MODE");
  set_status_led();
  delay(2000); 
  set_display_brightness(); //for good measure - some cases the brightness change isn't seen by displays

  while(true)
  {
    for (int n=0; n<NUM_LANES; n++)
    {
      lane_status[n] = bitRead(PIND, LANE_DET[n]);    // read status of all lanes
#ifndef MATRIX_DISPLAY
      if (lane_status[n] == HIGH)
      {
        update_display(n, msgDark);
      }
      else
      {
        update_display(n, msgLight);
      }
  #else
      if (lane_status[n] == HIGH) {
        showChar(n, '+');
        if (NUM_MATRICES==8) { //show on both sides
          showChar(7-n, '+');
        }
      } else {
        showChar(n, 'O');
        if (NUM_MATRICES==8) { //show on both sides
          showChar(7-n, 'O');
        }
      }
  #endif
    }

    serial_data = get_serial_data();

    if (serial_data == int(SMSG_RESET) || digitalRead(RESET_SWITCH) == LOW) {
      initialize(); //perform an actual reset
      break;
    }

    delay(100);
  }
}

/*================================================================================*
  SEND RACE RESULTS TO COMPUTER
 *================================================================================*/
void send_race_results()
{
  float lane_time_sec;


  for (int n=0; n<NUM_LANES; n++)    // send times to computer
  {
    lane_time_sec = (float)(lane_time[n] / 1000000.0);    // elapsed time (seconds)

    if (lane_time_sec == 0)    // did not finish
    {
      lane_time_sec = NULL_TIME;
    }

    Serial.print(n+1);
    Serial.print(" - ");
    Serial.println(lane_time_sec, NUM_DIGIT);  // numbers are rounded to NUM_DIGIT
                                               // digits by println function
  }

  return;
}


/*================================================================================*
  RACE FINISHED - DISPLAY PLACE / TIME FOR ALL LANES
 *================================================================================*/
void display_race_results()
{
  unsigned long now;
  static boolean display_mode;
  static unsigned long last_display_update = 0;


  if (!SHOW_PLACE) return;

  now = millis();

  if (last_display_update == 0)  // first cycle
  {
    last_display_update = now;
    display_mode = false;
  }

  if ((now - last_display_update) > (unsigned long)(PLACE_DELAY * 1000))
  {
    dbg(fDebug, "display_race_results");

    for (int n=0; n<NUM_LANES; n++)
    {
      update_display(n, lane_place[n], lane_time[n], display_mode);
    }

    display_mode = !display_mode;
    last_display_update = now;
  }

  return;
}


/*================================================================================*
  SEND MESSAGE TO DISPLAY
 *================================================================================*/
void update_display(int lane, unsigned char msg[])
{

#ifdef LED_DISPLAY
  disp_mat[lane].clear();
#ifdef DUAL_DISP
#ifdef DUAL_MODE
  disp_8x8[lane+4].clear();
#else
  disp_mat[lane+4].clear();
#endif
#endif

  for (int d = 0; d<=4; d++)
  {
    disp_mat[lane].writeDigitRaw(d, msg[d]);
#ifdef DUAL_DISP
#ifdef DUAL_MODE
    if (d == 3)
    {
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
#endif

  return;
}


/*================================================================================*
  UPDATE LANE PLACE/TIME DISPLAY
 *================================================================================*/
void update_display(int lane, int display_place, unsigned long display_time, int display_mode)
{
  dbg(fDebug, "led: lane = ", lane);
  dbg(fDebug, "led: plce = ", display_place);
  dbg(fDebug, "led: time = ", display_time);

#ifdef LED_DISPLAY
  int c;
  char ctime[10], cplace[4];
  double display_time_sec;
  boolean showdot;


  if (display_mode)
  {
    if (display_place > 0)  // show place order
    {
      sprintf(cplace,"%1d", display_place);

      disp_mat[lane].clear();
      disp_mat[lane].drawColon(false);
      disp_mat[lane].writeDigitNum(3, char2int(cplace[0]), false);
      disp_mat[lane].writeDisplay();

#ifdef DUAL_DISP
      disp_mat[lane+4].clear();
      disp_mat[lane+4].drawColon(false);
      disp_mat[lane+4].writeDigitNum(3, char2int(cplace[0]), false);
      disp_mat[lane+4].writeDisplay();
#endif
    }
    else  // did not finish
    {
      update_display(lane, msgDashL);
    }
  }
  else                      // show finish time
  {
    if (display_time > 0)
    {
      disp_mat[lane].clear();
      disp_mat[lane].drawColon(false);

#ifdef DUAL_DISP
#ifdef DUAL_MODE
      disp_8x8[lane+4].clear();
      disp_8x8[lane+4].setTextSize(1);
      disp_8x8[lane+4].setRotation(3);
      disp_8x8[lane+4].setCursor(2, 0);
#else
      disp_mat[lane+4].clear();
      disp_mat[lane+4].drawColon(false);
#endif
#endif

      display_time_sec = (double)(display_time / (double)1000000.0);    // elapsed time (seconds)
      dtostrf(display_time_sec, (DISP_DIGIT+1), DISP_DIGIT, ctime);     // convert to string

//      Serial.print("ctime = ["); Serial.print(ctime); Serial.println("]");
      c = 0;
      for (int d = 0; d<DISP_DIGIT; d++)
      {
#ifdef LARGE_DISP
        showdot = false;
#else
        showdot = (ctime[c + 1] == '.');
#endif
        disp_mat[lane].writeDigitNum(d + int(d / 2), char2int(ctime[c]), showdot);    // time
#ifdef DUAL_DISP
#ifdef DUAL_MODE
        sprintf(cplace,"%1d", display_place);
        disp_8x8[lane+4].print(cplace[0]);
#else
        disp_mat[lane+4].writeDigitNum(d + int(d / 2), char2int(ctime[c]), showdot);    // time
#endif
#endif

        c++; if (ctime[c] == '.') c++;
      }

#ifdef LARGE_DISP
      disp_mat[lane].writeDigitRaw(2, 16);
#ifdef DUAL_DISP
#ifndef DUAL_MODE
      disp_mat[lane+4].writeDigitRaw(2, 16);
#endif
#endif
#endif

      disp_mat[lane].writeDisplay();
#ifdef DUAL_DISP
#ifdef DUAL_MODE
      disp_8x8[lane+4].writeDisplay();
#else
      disp_mat[lane+4].writeDisplay();
#endif
#endif
    }
    else  // did not finish
    {
      update_display(lane, msgDashT);
    }
  }
#endif
#ifdef MATRIX_DISPLAY
  char cplace[4];
  sprintf(cplace,"%1d", display_place);
    if (display_place > 0) {  // show place order
      showChar(lane, cplace[0]);
      if (NUM_MATRICES==8) {
        showChar(7-lane, cplace[0]);
      }

    } else {
      showChar(lane, '-'); //did not finish
      if (NUM_MATRICES==8) {
        showChar(7-lane, '-');
      }
    }

#endif
  return;
}


/*================================================================================*
  CLEAR LANE PLACE/TIME DISPLAYS
 *================================================================================*/
void clear_displays()
{
  dbg(fDebug, "led: CLEAR");

  for (int n=0; n<NUM_MATRICES; n++) {
    if (mode == mRACING || mode == mTEST) {
      // racing
#ifndef MATRIX_DISPLAY
      update_display(n, msgBlank);
#else
      showChar(n, ' ');
#endif
    } else {
      // ready
#ifndef MATRIX_DISPLAY
      update_display(n, msgDashT);
#else
      showChar(n, '-');
#endif
    }
  }

  return;
}


/*================================================================================*
  SET LANE DISPLAY BRIGHTNESS
 *================================================================================*/
void set_display_brightness()
{

  float new_level;

  new_level = long(1023 - analogRead(BRIGHT_LEV)) / 1023.0F * 15.0F;
  new_level = min(new_level, (float)MAX_BRIGHT);
  new_level = max(new_level, (float)MIN_BRIGHT);

  if (fabs(new_level - display_level) > 0.3F)    // deadband to prevent flickering 
  {                                              // between levels
    dbg(fDebug, "led: BRIGHT");

    display_level = new_level;

#ifdef LED_DISPLAY
    for (int n=0; n<NUM_LANES; n++)
    {
      disp_mat[n].setBrightness((int)display_level);
#ifdef DUAL_DISP
#ifdef DUAL_MODE
      disp_8x8[n+4].setBrightness((int)display_level);
#else
      disp_mat[n+4].setBrightness((int)display_level);
#endif
#endif
    }
#endif

#ifdef MATRIX_DISPLAY
  for (int i=0;i<NUM_MATRICES;i++) {
    lc.setIntensity(i,display_level);
  }
#endif

  }

  return;
}


/*================================================================================*
  SET TIMER STATUS LED
 *================================================================================*/
void set_status_led()
{
  int r_lev, b_lev, g_lev;

  dbg(fDebug, "status led = ", mode);

  r_lev = PWM_LED_OFF;
  b_lev = PWM_LED_OFF;
  g_lev = PWM_LED_OFF;

  if (mode == mREADY)         // blue
  {
    b_lev = PWM_LED_ON;
  }
  else if (mode == mRACING)  // green
  {
    g_lev = PWM_LED_ON;
  }
  else if (mode == mFINISH)  // red
  {
    r_lev = PWM_LED_ON;
  }
  else if (mode == mTEST)    // yellow
  {
    r_lev = PWM_LED_ON;
    g_lev = PWM_LED_ON;
  }

  analogWrite(STATUS_LED_R,  r_lev);
  analogWrite(STATUS_LED_B,  b_lev);
  analogWrite(STATUS_LED_G,  g_lev);

  return;
}


/*================================================================================*
  READ SERIAL DATA FROM COMPUTER
 *================================================================================*/
int get_serial_data()
{  
  int data = 0;
  
  if (Serial.available() > 0)
  {
    data = Serial.read();
    dbg(fDebug, "ser rec = ", data);
  }

  return data;
}  


/*================================================================================*
  INITIALIZE TIMER
 *================================================================================*/
void initialize(boolean powerup)
{  
  for (int n=0; n<NUM_LANES; n++)
  {
    lane_time[n] = 0;
    lane_place[n] = 0;
  }

  start_time = 0;
  set_status_led();
  #ifndef MATRIX_DISPLAY
  digitalWrite(START_SOL, LOW);
  #endif

  // if power up and gate is open -> goto FINISH state
  if (powerup && digitalRead(START_GATE) == START_TRIP) 
  {
    mode = mFINISH;
  }
  else
  {
    mode = mREADY;

    smsg(SMSG_READY);
    delay(100);
  }
  Serial.flush();

  ready_first  = true;
  finish_first  = true;

  return;
}


/*================================================================================*
  UNMASK ALL LANES
 *================================================================================*/
void unmask_all_lanes()
{  
  dbg(fDebug, "unmask all lanes");

  for (int n=0; n<NUM_LANES; n++)
  {
    lane_mask[n] = false;
  }  

  return;
}  


/*================================================================================*
  SEND DEBUG TO COMPUTER
 *================================================================================*/
void dbg(int flag, const char * msg, int val)
{  
  char tmps[50];


  if (!flag) return;

  smsg_str("dbg: ", false);
  smsg_str(msg, false);

  if (val != -999)
  {
    sprintf(tmps, "%d", val);
    smsg_str(tmps);
  }
  else
  {
    smsg_str("");
  }

  return;
}


/*================================================================================*
  SEND SERIAL MESSAGE (CHAR) TO COMPUTER
 *================================================================================*/
void smsg(char msg, boolean crlf)
{  
  if (crlf)
  {
    Serial.println(msg);
  }
  else
  {
    Serial.print(msg);
  }

  return;
}


/*================================================================================*
  SEND SERIAL MESSAGE (STRING) TO COMPUTER
 *================================================================================*/
void smsg_str(const char * msg, boolean crlf)
{  
  if (crlf)
  {
    Serial.println(msg);
  }
  else
  {
    Serial.print(msg);
  }

  return;
}


/*================================================================================*
  SEND TIMER INFORMATION TO COMPUTER
 *================================================================================*/
void send_timer_info()
{
  char tmps[50];

  Serial.println("-----------------------------");
  sprintf(tmps, " PDT            Version %s", PDT_VERSION);
  Serial.println(tmps);
  Serial.println("-----------------------------");

  sprintf(tmps, "  NUM_LANES      %d", NUM_LANES);
  Serial.println(tmps);
  sprintf(tmps, "  GATE_RESET     %d", GATE_RESET);
  Serial.println(tmps);
  sprintf(tmps, "  SHOW_PLACE     %d", SHOW_PLACE);
  Serial.println(tmps);
  sprintf(tmps, "  PLACE_DELAY    %d", PLACE_DELAY);
  Serial.println(tmps);
  sprintf(tmps, "  MIN_BRIGHT     %d", MIN_BRIGHT);
  Serial.println(tmps);
  sprintf(tmps, "  MAX_BRIGHT     %d", MAX_BRIGHT);
  Serial.println(tmps);

  Serial.println("");

#ifdef ENABLE_TIMEOUT
  Serial.println("  ENABLE_TIMEOUT 1");
#else
  Serial.println("  ENABLE_TIMEOUT 0");
#endif

#ifdef LED_DISPLAY
  Serial.println("  LED_DISPLAY    1");
#else
  Serial.println("  LED_DISPLAY    0");
#endif

#ifdef DUAL_DISP
  Serial.println("  DUAL_DISP      1");
#else
  Serial.println("  DUAL_DISP      0");
#endif
#ifdef DUAL_MODE
  Serial.println("  DUAL_MODE      1");
#else
  Serial.println("  DUAL_MODE      0");
#endif

#ifdef LARGE_DISP
  Serial.println("  LARGE_DISP     1");
#else
  Serial.println("  LARGE_DISP     0");
#endif

#ifdef MATRIX_DISPLAY
  Serial.println("  MATRIX_DISP    1");
  sprintf(tmps, "  NUM_MATRICES   %d", NUM_MATRICES);
  Serial.println(tmps);
#else
  Serial.println("  MATRIX_DISP    0");
#endif

  Serial.println("");

  sprintf(tmps, "  ARDUINO VERS   %04d", ARDUINO);
  Serial.println(tmps);
  sprintf(tmps, "  COMPILE DATE   %s", __DATE__);
  Serial.println(tmps);
  sprintf(tmps, "  COMPILE TIME   %s", __TIME__);
  Serial.println(tmps);

  Serial.println("-----------------------------");

  return;
}

#ifdef MATRIX_DISPLAY
void showChar(int addr, char c_char) {
  dbg(fDebug, "showChar: addr =" ,addr);
  dbg(fDebug, "showChar: char =" + c_char);

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
#endif