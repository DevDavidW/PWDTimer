# PWDTimer

**Pinewood Derby Race Timer** created by David Gadberry and obtained from https://www.dfgtec.com/pdt 

This was modified some to fit the needs of our pack. 
Our old timer used 9.999 as the non-finish time so that was changed and we use 4 lanes. 
Added ability to enable a timeout of a lane when it exceeds the NULL_TIME.

Feb 2023 updated to PlatformIO and added use of cheaper 8x8 LED matrix modules with MAX7219 chip in leu of Adafruit I2C based ones.
   - Since we only use 4 lanes, using pins for lane 5 and 6 along with Solenoid pin to drive the displays