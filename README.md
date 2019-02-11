# TFT Sniff

## This is an example project for the WROVER TFT board.

 It will display the WiFi stations that are currently nearby.
 
  The example contains the display driver, the WiFi scanning code, and
a timer loop for continuous scanning. Lots of goodies also included:

 * GUI primitives,  fonts, basic shapes, (rect / line .. etc)
 * Fonts for different sizes 
   **     small, 
   ** medium, 
   ** large .. double large
 * Font generator. (compiles on linux)
 * WiFi initializer / scanner
 
  The TFT library is custom made with double buffering. It performs really 
  fast compared to the single buffered version. Screen refresh happens flicker free.
  
  See code for driving the TFT. Notice, the display code talks to two 
  half(s) of the display sequentially.
  
  Enjoy,    
 
   ![Screen Shot](./screen.jpg)
      
  Peter 



