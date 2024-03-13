// Libraries. Huge thanks to this guy https://www.penguintutor.com/projects/arduino-rp2040-pixelstrip as a lot this code is originally theirs.
//#include <SPI.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <VescUart.h>
VescUart vesc;

#include "config.h"
#include "modes.h"
#include "index.html.h"
#include "pixels.css.h"
#include "pixels.js.h"
#include "jquery.min.js.h"
#include "jqueryui.min.js.h"



//=====================================================================CONTENTS============================================================================
/*
   1. CLASS & VARIABLE SETUP......................
   2. PATTERNS....................................
    2.0 Standard..................................
    2.1 Rainbow...................................
    2.2 Color Wipe................................
    2.3 Color Fade................................
    2.4 Lava......................................
    2.5 Canopy....................................
    2.6 Ocean.....................................
    2.7 Rolling Wave..............................
    2.8 Color Wave................................
    2.9 Fireflies.................................
    2.10 Confetti.................................
    2.11 Comet....................................
    2.12 PacMan...................................
   3. PATTERN TOOLS...............................
   4. COMMAND UPDATER FOR LEDs....................
   5. TRANSITION ROUTINES.........................
   6. COMPLETION ROUTINES.........................
   7. GETTING WEB & UART DATA.....................
   8. ARDUINO MAIN LOOP...........................
*/



// 1. CLASS & VARIABLE SETUP==========================================================================================================================================
// Web stuff -------------------------------------------------
// what level of debug from serial console
// higher number the more debug messages
// recommend level 1 (includes occassional messages)
#define DEBUG 2
#define LEDR 46
#define LEDG 0
#define LEDB 45
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#define LED_BUILTIN 48
#endif

int status = WL_IDLE_STATUS;
WiFiServer server(80);

#define URL_DEFAULT 0
#define URL_HTML 0
#define URL_SEQUENCE 1
#define URL_CSS 2
#define URL_JS 3
#define URL_JSON 4
#define URL_JQUERY 5
#define URL_JQUERYUI 6                     // This is all WiFi Stuff ^^^ IDK What's going on her tbh but it makes it work.


long int lastWebmillis = millis();         // tracks time for WiFi updates every 500ms
long int lastVescUpdate = millis();        // tracks time for VESC UART data every 50ms

int cmdArr[8];                             // array of commands [0/1 OnOff, 0-13 Mode, 0-255 Red, 0-255 Geen, 0-255 Blue, 0-100 Brightness, unused...] 
int usrColor [3];                          // stores website requested color, Updates cmdArr if Colors dont match.

// For Asymmetric Fade Patterns Modes 4-8
int fadeFromArr[pts + 1][3];               // [3] for RGB Values
int fadeArr[pts + 1][3];                   // fadeFrom is starting configuration ...
int fadeToArr[pts + 1][3];                 // ... fadeTo, is ending configuration
int count = 1;                             // used to track overall colorshift in ColorWave[M7]

// For Fireflies Pattern [M9]
int stripArr[numFiFl][2];                  // [2] because first datapoint is location, second is direction (right -1, left, 1, stay 0)

// For Comet Pattern [M11]
int cometDir;                              // direction: forwards of backwards
int cometStrt;                             // starting pixel
int cometWheel;                            // color

int tailCol[3];

// VESC UART Data
double rpmHist[6] = {0, 0, 0, 0, 0, 0};    // take past 6 rpm value, 300 ms window
int vescDir = 0;                           // current OW direction: 1 forward, -1 backward, 0 idle
bool braking = false;                      // true when rpm decel detected
bool brakingFaded = true;                  // flag for transitions between brakelights and tail lights
int brakingFadeSteps = 20;                 // 20 steps to fade to normal after braking
int brakingFadeIndex = 0;                  // fade step index

bool dirFaded = true;                      // tracks changing direction red/white fading progress
int fadeDir = 0;                           // requested fade direction
int dirSet = 0;                            // which direction was just set as front -1 rev, 1 forw, 0 idle

const double idleThresholdRpm = idleThresholdMph / (11.0 * PI * 60.0) * 63360.0 * 15.0;     // converts mph to erpm (15 for 30/2 motor poles)

// Pattern Types (any new pattern must be added here):
enum  pattern { NONE, STANDARD, RAINBOW, COLOR_WIPE, FADE, LAVA, CANOPY, OCEAN, ROLLING_WAVE, COLOR_WAVE, FIREFLIES, CONFETTI, COMET, PACMAN, TAIL_COLOR, PIXEL_FINDER};

// Patern directions supported:
//enum  direction { FORWARD, REVERSE };



// NeoPattern Class - Derived from the Adafruit_NeoPixel Class
class NeoPatterns : public Adafruit_NeoPixel
{
  public:
    // Each Pattern may Have These Charachteristics
    pattern   ActivePattern;  // current pattern

    unsigned long Interval;   // milliseconds between updates
    unsigned long lastUpdate; // last update

    uint32_t Color1, Color2;  // colors in use
    uint16_t TotalSteps;      // total number of steps in a pattern
    uint16_t Index;           // current step within the pattern

    void (*OnComplete)();     // Callback on completion of pattern

    // Constructor: Initializes the Strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
      : Adafruit_NeoPixel(pixels, pin, type)
    {
      OnComplete = callback;
    }

    // Called every time step to update current pattern
    void Update()
    {
      if ((millis() - lastUpdate) > Interval) {
        lastUpdate = millis();
        switch (ActivePattern)
        {
          case STANDARD:
            StandardUpdate();
            break;
          case RAINBOW:
            RainbowUpdate();
            break;
          case COLOR_WIPE:
            ColorWipeUpdate();
            break;
          case FADE:
            FadeUpdate();
            break;
          case LAVA:
            LavaUpdate();
            break;
          case CANOPY:
            CanopyUpdate();
            break;
          case OCEAN:
            OceanUpdate();
            break;
          case ROLLING_WAVE:
            RollingWaveUpdate();
            break;
          case COLOR_WAVE:
            ColorWaveUpdate();
            break;
          case FIREFLIES:
            FirefliesUpdate();
            break;
          case CONFETTI:
            ConfettiUpdate();
            break;
          case COMET:
            CometUpdate();
            break;
          case PACMAN:
            PacManUpdate();
            break;
          case TAIL_COLOR:
            TailColorUpdate();
            break;
          case PIXEL_FINDER:
            PixelFinderUpdate();
            break;
          default:
            break;
        }
        // Use onboard LED to copy first pixel
        if (cmdArr[1] == 0){
          analogWrite(LEDR,  255 - cmdArr[2] * 0.75);
          analogWrite(LEDG,  255 - cmdArr[3] * 0.75);
          analogWrite(LEDB,  255 - cmdArr[4] * 0.75); 
        } else {
          analogWrite(LEDR,  255 - Red(getPixelColor(0)) * 0.75);
          analogWrite(LEDG,  255 - Green(getPixelColor(0)) * 0.75);
          analogWrite(LEDB,  255 - Blue(getPixelColor(0)) * 0.75); 
        }
      }
    }

    // Increment Keeps Track of the Step Number
    void Increment()
    {
      Index++;
      if (Index >= TotalSteps)
      {
        Index = 0;
        if (OnComplete != NULL)
        {
          OnComplete();              // Do something when pattern is done; dont just stop. Ususally, repeat.
        }
      }
    }





// 2. PATTERNS==========================================================================================================================================
// 2.0 Standard-------------------------------------------------------------
    void Standard()//direction dir = FORWARD)
    {
      ActivePattern = STANDARD;
      Interval = 50;
      TotalSteps = 10;
      Index = 0;
    }

    void StandardUpdate()
    { 
      if((rpmHist[0] * rpmHist[1] < 0 || rpmHist[0] * rpmHist[2] < 0 ) && dirFaded){        // detects direction flip, checks two steps back into history in case "0" is captured
        dirFaded = false;                                     // flag for fading
        if (rpmHist[0] < 0){
          fadeDir = -1;                                       // set reversing
        }
        else{
          fadeDir = 1;                                        // set forward
        } 
      } else if(dirFaded && dirSet == 1 && rpmHist[1] < 0){   // if fade completed but current direction is wrong 
        dirFaded = false;                                     // flag for fade again
        fadeDir = -1;                                         // correct the direction
        dirSet = 0;                                           // set direction check idle 
      } else if(dirFaded && dirSet == -1 && rpmHist[1] > 0){  
        dirFaded = false;
        fadeDir = 1;
        dirSet = 0;
      } else if(rpmHist[0] >= 0 && dirFaded){                  // if forward & fade is correct 
        dirSet = 0;                                            // set direction check to finished
        for(int i = 0; i < breakPoint; i++){                   // set nose color
          setPixelColorIdleDim(i, DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
        } 
        for(int i = breakPoint; i < numPixels(); i++){          // set tail red, brightness scaled to rpm
          setPixelColorIdleDim(i, Color(min(15.0 + 125.0 * pow(fabs(rpmHist[0]) / 6000.0, 2.0) , 255.0), 0, 0));
        } 
      } else if(rpmHist[0] < 0 && dirFaded){                    // if backward & fade is correct
        dirSet = 0;                                             // set direction check idle
        for(int i = breakPoint; i < numPixels(); i++){          // set tail to color
          setPixelColorIdleDim(i, DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
        } 
        for(int i = 0; i < breakPoint; i++){                    // set nose red, brightness scaled to rpm
          setPixelColorIdleDim(i, Color(min(15.0 + 125.0 * pow(fabs(rpmHist[0]) / 6000.0, 2.0) , 255.0), 0, 0));
        }
      } else{
        ;
      }
      
      if (!dirFaded) {                                          // if requested direction change
        Increment();                                            // start fade index count
        if(fadeDir == 1){                                       // if requested fade nose forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(60.0 * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + cmdArr[2] * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = cmdArr[3] * (double(Index + 1)/double(TotalSteps));
            uint8_t blue = cmdArr[4] * (double(Index + 1)/double(TotalSteps));
            setPixelColorIdleDim(i, DimColor(Color(red, green, blue), cmdArr[5]));
          }
          for(int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(cmdArr[2] * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + 60 * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = cmdArr[3] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            uint8_t blue = cmdArr[4] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            setPixelColorIdleDim(i, DimColor(Color(red, green, blue), cmdArr[5]));
          }
        } else if (fadeDir == -1){                               // if requested fade tail forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(cmdArr[2] * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + 60 * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = cmdArr[3] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            uint8_t blue = cmdArr[4] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            setPixelColorIdleDim(i, DimColor(Color(red, green, blue), cmdArr[5]));
          }
          for(int i = breakPoint; i <numPixels(); i++){
            uint8_t red = min(60 * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + cmdArr[2] * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = cmdArr[3] * (double(Index + 1)/double(TotalSteps));
            uint8_t blue = cmdArr[4] * (double(Index + 1)/double(TotalSteps));
            setPixelColorIdleDim(i, DimColor(Color(red, green, blue), cmdArr[5]));
          }
        }
      }
      
      //checkBraking();
      show();
    }
    


// 2.1 Rainbow--------------------------------------------------------------
    // Initialize for a RainbowCycle
    void Rainbow(uint8_t interval)//, direction dir = FORWARD)
    {
      ActivePattern = RAINBOW;
      Interval = interval;
      TotalSteps = 255;
      Index = 0;
      //Direction = dir;
    }

    // Update the Rainbow Pattern
    void RainbowUpdate()
    {
      for (int i = 0; i < numPixels(); i++)
      {

        setPixelColorIdleDim(i, DimColor(Wheel(int((double(i) * 256.0 / (stripLength / numRainbow)) + double(Index)) & 255), cmdArr[5]));
      }
      checkBraking();
      show();
      Increment();
    }



// 2.2 ColorWipe--------------------------------------------------------------
    // Initialize for a ColorWipe
    void ColorWipe()//direction dir = FORWARD)
    {
      ActivePattern = COLOR_WIPE;
      Interval = max(70.0 - (fabs(rpmHist[0]) / 164.0) + 5.0, 1.0);
      TotalSteps = stripLength - 1;
      Index = 0;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
      ColorSetRange(DimColor(Color2, cmdArr[5]), 0, Index + 1);
      ColorSetRange(DimColor(Color1, cmdArr[5]), Index + 1, stripLength);
      checkBraking();
      show();
      Increment();
      Interval = max(70.0 - (fabs(rpmHist[0]) / 164.0) + 5.0, 1.0);
    }



// 2.3 Color Fade--------------------------------------------------------------
    // Moves from one random color to the Next
    void Fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval)//, direction dir = FORWARD)
    {
      ActivePattern = FADE;
      Interval = interval;
      TotalSteps = steps;
      Color1 = color1;
      Color2 = color2;
      Index = 0;
      //Direction = dir;
    }

    // Update the Fade Pattern
    void FadeUpdate()
    {
      // Calculate linear interpolation between Color1 and Color2
      // Optimise order of operations to minimize truncation error
      uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
      uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
      uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;

      //checkBraking();
      //show();
      ColorSet(DimColor(Color(red, green, blue), cmdArr[5]));
      TailTint();
      checkBraking();
      show();
      Increment();
    }



// 2.4 Lava--------------------------------------------------------------
    // Initialize for Lava
    void Lava(uint16_t steps, uint8_t interval)
    {
      ActivePattern = LAVA;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;

    }

    // Update the Lava Pattern
    void LavaUpdate()
    {
      double pixR = stripLength / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          setPixelColorIdleDim(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts - 1][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts - 1][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts - 1][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        setPixelColorIdleDim((modif / divBy - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }
      
      checkBraking();
      show();
      Increment();

      for (int i = 0; i < pts + 1; i++)
      {
        fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
        fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
        fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
      }
    }



// 2.5 Canopy--------------------------------------------------------------
    // Initialize for Canopy
    void Canopy(uint16_t steps, uint8_t interval) // Similar to Lava
    {
      ActivePattern = CANOPY;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
    }

    // Update the Canopy Pattern
    void CanopyUpdate()
    {
      double pixR = stripLength / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          setPixelColorIdleDim(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts - 1][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts - 1][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts - 1][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        setPixelColorIdleDim((modif / divBy - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
        fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
        fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
      }
    }



// 2.6 Ocean--------------------------------------------------------------
    // Initialize for Ocean
    void Ocean(uint16_t steps, uint8_t interval) //Similar to Lava
    {
      ActivePattern = OCEAN;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
    }



    // Update the Ocean Pattern
    void OceanUpdate()
    {
      double pixR = stripLength / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          setPixelColorIdleDim(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts - 1][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts - 1][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts - 1][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        setPixelColorIdleDim((modif / divBy - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
        fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
        fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
      }
    }



// 2.7 RollingWave--------------------------------------------------------------
    // Initialize for Rolling Wave
    void RollingWave(uint16_t steps, uint8_t interval)
    {
      ActivePattern = ROLLING_WAVE;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
    }

    // Update the RollingWave Pattern
    void RollingWaveUpdate() // Similar to Lava
    {
      double pixR = stripLength / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          setPixelColorIdleDim(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts - 1][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts - 1][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts - 1][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        setPixelColorIdleDim((modif / divBy - 1)  * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
        fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
        fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
      }
    }



// 2.8 Color Wave-------------------------------------------------------------
    void ColorWave(uint16_t steps, uint8_t interval)
    {
      ActivePattern = COLOR_WAVE;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
    }

    void ColorWaveUpdate()
    {
      double pixR = stripLength / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          setPixelColorIdleDim(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts - 1][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts - 1][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts - 1][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        setPixelColorIdleDim((modif / divBy - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }

      TailTint();
      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
        fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
        fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
      }
    }



// 2.9 Fireflies--------------------------------------------------------------
    // Initialize for Fireflies
    void Fireflies(uint8_t interval)
    {
      ActivePattern = FIREFLIES;
      Interval = interval;
      TotalSteps = random(10, 50);
      Index = 0;
      for (int i = 0; i < numFiFl; i++) {
        stripArr[i][0] = random(0, stripLength); // pick the location of fireflies
      }
    }

    // Update the Fireflies Pattern
    void FirefliesUpdate()
    {
      for (int i = 0; i < numFiFl; i++) {
        stripArr[i][1] = random(0, 3);     // pick left, right, or stay
        if (stripArr[i][1] == 0) {         // left
          setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          setPixelColorIdleDim(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] + 1) % (int(stripLength) + 1)), 30), cmdArr[5]));
          setPixelColorIdleDim((stripArr[i][0] + 2) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelColorIdleDim((stripArr[i][0] - 1) % (int(stripLength) + 1), Color(0, 0, 0));
          stripArr[i][0] = (stripArr[i][0] + 1) % (int(stripLength) + 1);
        } else if (stripArr[i][1] == 1) {  // stay
          setPixelColorIdleDim((stripArr[i][0] - 1) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelColorIdleDim(stripArr[i][0], DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), Color(0, 0, 0));
        } else if (stripArr[i][1] == 2) {  // right
          setPixelColorIdleDim((stripArr[i][0] - 1) % (int(stripLength) + 1), DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          setPixelColorIdleDim(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] - 1) % (int(stripLength) + 1)), 30), cmdArr[5]));
          setPixelColorIdleDim((stripArr[i][0] - 2) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), Color(0, 0, 0));
          stripArr[i][0] = (stripArr[i][0] - 1) % (int(stripLength) + 1);
        }
      }
      
      checkBraking();
      show();
      Increment();
    }



// 2.10 Confetti--------------------------------------------------------------
    //Initialize for Confetti
    void Confetti(int interval, int steps)
    {
      ActivePattern = CONFETTI;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
    }

    //Update the Confetti Pattern
    void ConfettiUpdate()
    {
      for (int  i = 0; i < numConf / 3; i++) {
        setPixelColorIdleDim(random(0, stripLength), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index))))));
        setPixelColorIdleDim(random(0, stripLength), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index + 24))))));
        setPixelColorIdleDim(random(0, stripLength), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index + 48))))));
      }
      for (int  i = 0; i < stripLength; i++) {
        setPixelColor(i, DimColor(getPixelColor(i), 80));
      }
      
      checkBraking();
      show();
      Increment();
    }



// 2.11 Comet--------------------------------------------------------------
    // Initialize for Comet
    void Comet(uint32_t Color1, uint8_t interval, uint8_t steps)
    {
      ActivePattern = COMET;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      cometDir = random(0, 2);
      cometStrt = random(0, stripLength);
      cometWheel = random(0, 255);
    }
    // Update the Comet Pattern
    void CometUpdate()
    {
      int loc = 0;
      for (int i = 0; i < numPixels(); i++) {
        if (i == Index) {                     // if chosen pixel
          if (cometDir == 0) {
            loc = i + cometStrt;
            if (loc > int(stripLength)) {
              loc -= int(stripLength);
            }
          } else if (cometDir == 1) {
            loc = cometStrt - i;
            if (loc < 0) {
              loc += int(stripLength);
            }
          }
          setPixelColorIdleDim(loc, DimColor(Wheel(cometWheel + random(0, 30) - 15), cmdArr[5]));
        }
        else // Fading tail
        {
          setPixelColorIdleDim(i, DimColor(DimColor(getPixelColor(i), 80), cmdArr[5]));
        }
      }
      
      checkBraking();
      show();
      Increment();
    }



// 2.12 PacMan--------------------------------------------------------------
    void PacMan(){
      ActivePattern = PACMAN;
      Interval = max(20.0 , 200.0 - (fabs(rpmHist[0]) / 57.0) + 20.0);   // equals to 0 + 5.0 ms at 25mph
      TotalSteps = 5;
      Index = 0;
    }

    void PacManUpdate(){
      if (rpmHist[0] < 0){
        ColorSetRange(DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]), breakPoint, numPixels());
        for(int i = 0; i < breakPoint - 1; i++){
          if(i % 5 == Index){
            setPixelColor(breakPoint - (i + 1), Color(25, 25, 10));
          }else{
            setPixelColor(breakPoint - (i + 1), Color(0, 0, 0));
          }
        }
        if(Index % 4 == 0 || Index % 4 == 1){
          setPixelColor(0, Color(255, 168, 0));
        } else {
          setPixelColor(0, Color(0, 0, 0));
        }
      } else {
        ColorSetRange(DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]), 0, breakPoint);
        for(int i = breakPoint; i < numPixels() -  1; i++){
          if(i % 5 == Index){
            setPixelColor(breakPoint + numPixels() - (i + 1), Color(25, 25, 10));
          }else{
            setPixelColor(breakPoint + numPixels() - (i + 1), Color(0, 0, 0));
          }
        }

        if(Index % 4 == 0 || Index % 4 == 1){
          setPixelColor(breakPoint, Color(255, 168, 0));
        } else {
          setPixelColor(breakPoint, Color(0, 0, 0));
        }
      }

      show();
      Increment();
      Interval = max(20.0 , 200.0 - (fabs(rpmHist[0]) / 57.0) + 20.0);
    }



    void TailColor(){
      ActivePattern = TAIL_COLOR;
      Interval = 80;
      TotalSteps = 1;
      Index = 0;
    }

    void TailColorUpdate(){
      if(rpmHist[0] < 0){
        for(int i = 0; i < breakPoint; i++){
          setPixelColorIdleDim(i, DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
        }
        for(int i = breakPoint; i < numPixels(); i++){
          setPixelColorIdleDim(i, DimColor(Color(255, 255, 255), cmdArr[5]));
        }
      }else{
        for(int i = 0; i < breakPoint; i++){
          setPixelColorIdleDim(i, DimColor(Color(255, 255, 255), cmdArr[5]));
        }
        for(int i = breakPoint; i < numPixels(); i++){
          setPixelColorIdleDim(i, DimColor(Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
        }
      }
      show();
      Increment();
    }

// 2.13 PixelFinder--------------------------------------------------------------
    void PixelFinder()
    {
      ActivePattern = PIXEL_FINDER;
      Interval = 10;
      TotalSteps = 1;
      Index = 0;
      ColorSet(Color(0, 0, 0));
      checkBraking();
      show();
    }

    void PixelFinderUpdate()
    {
      for (int i = 0; i < numPixels(); i++) {
        if (i == cmdArr[5])
          setPixelColor(i, Color(0, 255, 0));
        else
          setPixelColor(i, Color(0, 0, 0));
      }
      show();
      Increment();
    }





// 3. PATTERN TOOLS==========================================================================================================================================
    // Modifies colors for "tail" lights & when braking is detected-----------------------
    void checkBraking(){
      if (disableBrakingResponse){ // exit if debug
        return;
      }

      if (vescDir == 0){          // when at idle
        braking = false;
        brakingFaded = true;
      }
      if (braking) {
        if(brakingFaded && brakeBlink){       // first frame of braking
          if (rpmHist[0] < 0){
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(0, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(255, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(0, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
          } else {
            for(int i = breakPoint; i < numPixels(); i++){
              setPixelColor(i, Color(0, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
            for(int i = breakPoint; i < numPixels(); i++){
              setPixelColor(i, Color(255, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
            for(int i = breakPoint; i < numPixels(); i++){
              setPixelColor(i, Color(0, 0, 0));
            }
            show();
            delay(brakeBlink_ms);
          }
        }

        brakingFaded = false;
        if (rpmHist[0] < 0){
          for(int i = 0; i < breakPoint; i++){
            setPixelColor(i, Color(255, 0, 0));
          }
        } else {
          for(int i = breakPoint; i < numPixels(); i++){
            setPixelColor(i, Color(255, 0, 0));
          } 
        }
      } else if (!brakingFaded && !braking) {         // first "frame" after braking is no longer detected
        if (rpmHist[0] >= 0){
          for (int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(255 * (double(brakingFadeSteps - brakingFadeIndex) / double(brakingFadeSteps)) + Red(getPixelColor(i)), 255.0);
            uint8_t green = Green(getPixelColor(i)) * (double(brakingFadeIndex) / double(brakingFadeSteps));
            uint8_t blue = Blue(getPixelColor(i)) * (double(brakingFadeIndex) / double(brakingFadeSteps));
            setPixelColor(i, Color(red, green, blue));
          }
        } else if (rpmHist[0] < 0){
          for (int i = 0; i < breakPoint; i++){
            uint8_t red = min(255 * (double(brakingFadeSteps - brakingFadeIndex) / double(brakingFadeSteps)) + Red(getPixelColor(i)), 255.0);
            uint8_t green = Green(getPixelColor(i)) * (double(brakingFadeIndex) / double(brakingFadeSteps));
            uint8_t blue = Blue(getPixelColor(i)) * (double(brakingFadeIndex) / double(brakingFadeSteps));
            setPixelColor(i, Color(red, green, blue));
          }
        }
        brakingFadeIndex++;
        if (brakingFadeIndex == brakingFadeSteps){
          brakingFaded = true;
          brakingFadeIndex = 0;
        }
      }
    }
    
    //Called by some modes, rescales colors to add more red to "tail" lights
    void TailTint(){
      if (rpmHist[0] < 0){
        for(int i = 0; i < breakPoint; i++){
          int r = min(10 + 1.5 * double(Red(getPixelColor(i))), 255.0);
          int g = 0.3 * double(Green(getPixelColor(i)));
          int b = 0.3 * double(Blue(getPixelColor(i)));
          setPixelColor(i, Color(r, g, b));
        }
      } else {
        for(int i = breakPoint; i < numPixels(); i++){
          int r = min(10 + 1.5 * double(Red(getPixelColor(i))), 255.0);
          int g = 0.3 * double(Green(getPixelColor(i)));
          int b = 0.3 * double(Blue(getPixelColor(i)));
          setPixelColor(i, Color(r, g, b));
        } 
      }
    }
    // Any last special modifies to be applied to each pixel, current dims lights to idleBrightness when not moving 
    void setPixelColorIdleDim(uint16_t loc, uint32_t color) {
      if (vescDir == 0 && enableUART){   // dim lights if below 1mph && active only when UARTData is allowed
        setPixelColor(loc, DimColor(color, idleBrightness));
      }else{
        setPixelColor(loc, color);
      }
    }

    // Calculate percent dimmed version of a color (used by Comet [M10])
    uint32_t DimColor(uint32_t color, uint8_t percent)
    {
      uint32_t dimColor = Color(uint8_t(double(Red(color)) * pow((double(percent) / 100.0), 2.0)),
                                uint8_t(double(Green(color)) * pow((double(percent) / 100.0), 2.0)),
                                uint8_t(double(Blue(color)) * pow((double(percent) / 100.0), 2.0)));
      return dimColor;
    }



    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
      for (int i = 0; i < stripLength; i++)
      {
        setPixelColorIdleDim(i, DimColor(color, cmdArr[5]));
      }
    }



    // Set all pixels in a range to a color
    void ColorSetRange(uint32_t color, uint16_t strt, uint16_t fnsh)
    {
      if (strt >= fnsh){
        return;
      }
      if (fnsh > stripLength){
        fnsh = stripLength;
      }
      for (int i = strt; i < fnsh; i++)
      {
        setPixelColorIdleDim(i, DimColor(color, cmdArr[5]));
      }
    }


    // Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
      return (color >> 16) & 0xFF;
    }



    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
      return (color >> 8) & 0xFF;
    }



    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
      return color & 0xFF;
    }



    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
      WheelPos = 255 - WheelPos;
      if (WheelPos < 85)
      {
        return Color(255 - WheelPos * 3, 0, WheelPos * 3);
      }
      else if (WheelPos < 135)
      {
        WheelPos -= 85;
        return Color(0, int(double(WheelPos) * 5.1), int(255 - double(WheelPos) * 5.1));
      }
      else
      {
        WheelPos -= 135;
        return Color(int(double(WheelPos) * 2.125), int(255.0 - double(WheelPos) * 2.125), 0);
      }
    }
};

void StripComplete();

// Define some NeoPatterns for the two strip sections
NeoPatterns Strip = NeoPatterns(stripLength, datPinNum, NEO_GRB + NEO_KHZ800, &StripComplete);

// =================================================================== END OF DISPLAY FUNCTIONS ======================================================================





// 4. ==================================================================Strip Command Center=========================================================================================================================================================
void ControlPanel() {   // called when cmdArr is updated. Calls for transition to new requested strip state and starts the pattern.
  if (!cmdArr[0]) {     // if turned off, fade to black
    Strip.ActivePattern = NONE;
    for (int i = 0; i < 50; i++) {
      for (int j = 0; j < Strip.numPixels(); j++) {
        uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (50 - i)) / 50;

        Strip.setPixelColor(j, Strip.Color(red, green, blue));
      }
      Strip.show();
      delay(10);
    }
  } else {    
    randomSeed(millis());
    Strip.Color1 = Strip.Wheel(random(255));    // choose starting color
    Transition();                               // fade to state
    switch (cmdArr[1]) {
      case 0:
        {
          Strip.Standard();
          break;
      } case 1:
        {
          Strip.Rainbow(20);
          break;
      } case 2:
        {
          Strip.ColorWipe();
          break;
      } case 3:
        {
          Strip.Fade(Strip.Color1, Strip.Color2, 50, 60);
          break;
      } case 4:
        {
          Strip.Lava(20, 80);
          break;
      } case 5:
        {
          Strip.Canopy(20, 80);
          break;
      } case 6:
        {
          Strip.Ocean(20, 80);
          break;
      } case 7:
        {
          Strip.RollingWave(20, 80);
          break;
      } case 8:
        {
          Strip.ColorWave(20, 80);
          break;
      } case 9:
        {
          Strip.Fireflies(80);
          break;
      } case 10:
        {
          Strip.Confetti(80, 128);
          break;
      } case 11:
        {
          Strip.Comet(Strip.Color1, 50, 80);
          break;
      } case 12:
        {
          Strip.PacMan();
          break;
      } case 13:
        {
          Strip.TailColor();
          break;
      }case 14:
        {
          Strip.PixelFinder();
          break;
        }
      default:
        break;
    }
  }
}




// 5. ==========================================================TRANSITIONS=====================================================================================================
// Fades each pixel into new state 
void Transition() {
  int trSteps = 20;
  switch (int(cmdArr[1])) {
    case 0:
      {
        if (vescDir == -1){
          for (int  i = 0; i < trSteps; i++) {
            for (int j = breakPoint; j < Strip.numPixels(); j++) {
              uint32_t col = Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]);
              uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
              uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
              uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

              Strip.setPixelColorIdleDim(j, (Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5])));
            }
            for (int j = 0; j < breakPoint; j++) {
              uint32_t col = Strip.DimColor(Strip.Color(20, 0, 0), cmdArr[5]);
              uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
              uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
              uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

              Strip.setPixelColorIdleDim(j, (Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5])));
            }
            Strip.show();
            delay(20);
          }
        } else {
          for (int  i = 0; i < trSteps; i++) {
            for (int j = 0; j < breakPoint; j++) {
              uint32_t col = Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]);
              uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
              uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
              uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

              Strip.setPixelColorIdleDim(j, (Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5])));
            }
            for (int j = breakPoint; j < Strip.numPixels(); j++) {
              uint32_t col = Strip.DimColor(Strip.Color(20, 0, 0), cmdArr[5]);
              uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
              uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
              uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

              Strip.setPixelColorIdleDim(j, (Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5])));
            }
            Strip.show();
            delay(20);
          }
        }
        break;
    } case 1:
      {
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.DimColor(Strip.Wheel(int(double(j) * 256.0 / (stripLength / 2.0)) & 255), cmdArr[5]);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

            Strip.setPixelColorIdleDim(j, (Strip.Color(red, green, blue)));
          }
          Strip.checkBraking();
          Strip.show();
          delay(20);
        }
        break;
    } case 2:
      {
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.DimColor(Strip.Color1, cmdArr[5]);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

            Strip.setPixelColorIdleDim(j, (Strip.Color(red, green, blue)));
          }
          Strip.show();
          delay(20);
        }
        break;
    } case 3:
      {
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.DimColor(Strip.Color1, cmdArr[5]);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;

            Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
          }
          Strip.TailTint();
          Strip.show();
          delay(20);
        }
        break;
    } case 4:  //red wave, lava, set start state and fade to start state
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(5, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(180, 255)) * randBrightnessTo); // red
          fadeToArr[i][1] = int(double(random(20, 75)) * randBrightnessTo); // green
          fadeToArr[i][2] = 0; // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 5:  //green wave, canpoy, set start state and fade to start state
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(30, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 6:  //blue wave, ocean, set start state and fade to start state
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
          fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 7:  //rolling wave, set start state and fade to start state, red
      {
        //--------------------Room--------------------
        count++;
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(Strip.Red(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); //red
          fadeToArr[i][1] = int(double(Strip.Green(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(Strip.Blue(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // blue

        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 8:  //color wave, ocean, set start state and fade to start state, set color
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(20, 100)) / 100.0;

          fadeToArr[i][0] = min(int(double(cmdArr[2] + random(-75, 75) * (pow(2.0 * (double(cmdArr[2]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // red
          fadeToArr[i][1] = min(int(double(cmdArr[3] + random(-75, 75) * (pow(2.0 * (double(cmdArr[3]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // green
          fadeToArr[i][2] = min(int(double(cmdArr[4] + random(-75, 75) * (pow(2.0 * (double(cmdArr[4]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255);  // blue
          
          fadeToArr[i][0] = max(fadeToArr[i][0], 0);
          fadeToArr[i][1] = max(fadeToArr[i][1], 0);
          fadeToArr[i][2] = max(fadeToArr[i][2], 0);
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 9:  // fireflies fade to black
      {
        for (int i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;

            Strip.setPixelColor(j, Strip.Color(red, green, blue));
          }
          Strip.show();
          delay(20);
        }
        break;
    } case 10: // confetti fade to black
      {
        for (int i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;

            Strip.setPixelColor(j, Strip.Color(red, green, blue));
          }
          Strip.show();
          delay(20);
        }
        break;
    } case 11:
      {
        //built in already
        break;
    } case 12: // pacman fade to start config
      {
        for (int  i = 0; i < trSteps; i++) {
          if (rpmHist[0] < 0){
            for (int j = breakPoint; j < Strip.numPixels(); j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[2] * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[3] * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[4] * i) / trSteps;

              Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
            }
            for (int j = 1; j < breakPoint; j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;

              Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
            }
          } else {
            for (int j = 0; j < breakPoint; j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[2] * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[3] * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[4] * i) / trSteps;

              Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
            }
            for (int j = breakPoint + 1; j <Strip.numPixels(); j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;

              Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
            }
          }
          if (rpmHist[0] < 0){
            uint8_t redPM = (Strip.Red(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
            uint8_t greenPM = (Strip.Green(Strip.getPixelColor(0)) * (trSteps - i) + 168 * i) / trSteps;
            uint8_t bluePM = (Strip.Blue(Strip.getPixelColor(0)) * (trSteps - i)) / trSteps;
            Strip.setPixelColorIdleDim(0, (Strip.Color(redPM, greenPM, bluePM)));
          }else{
            uint8_t redPM = (Strip.Red(Strip.getPixelColor(breakPoint)) * (trSteps - i) + 255 * i) / trSteps;
            uint8_t greenPM = (Strip.Green(Strip.getPixelColor(breakPoint)) * (trSteps - i) + 168 * i) / trSteps;
            uint8_t bluePM = (Strip.Blue(Strip.getPixelColor(breakPoint)) * (trSteps - i)) / trSteps;
            Strip.setPixelColorIdleDim(breakPoint, (Strip.Color(redPM, greenPM, bluePM)));
          }
          Strip.show();
          delay(20);
        }
        break;
    } case 13:
      {
        for (int  i = 0; i < trSteps; i++) {
          if (rpmHist[0] < 0){
            for (int j = 0; j < breakPoint; j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              Strip.setPixelColorIdleDim(0, (Strip.Color(red, green, blue)));
            }
            for (int j = breakPoint; j < Strip.numPixels(); j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[2] * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[3] * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[4] * i) / trSteps;
              Strip.setPixelColorIdleDim(0, (Strip.Color(red, green, blue)));
            }
          }else{
            for (int j = 0; j < breakPoint; j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[2] * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[3] * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(0)) * (trSteps - i) + cmdArr[4] * i) / trSteps;
              Strip.setPixelColorIdleDim(j, (Strip.Color(red, green, blue)));
            }
            for (int j = breakPoint; j < Strip.numPixels(); j++) {
              uint8_t red = (Strip.Red(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              uint8_t green = (Strip.Green(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              uint8_t blue = (Strip.Blue(Strip.getPixelColor(0)) * (trSteps - i) + 255 * i) / trSteps;
              Strip.setPixelColorIdleDim(j, (Strip.Color(red, green, blue)));
            }
          }
          Strip.show();
          delay(10);
        }
    } case 14: //pixel finder fade to black
      {
      for (int i = 0; i < trSteps; i++){
        for (int j = 0; j < Strip.numPixels(); j++) {
          uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[2] * i) / trSteps;
          uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[3] * i) / trSteps;
          uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i) + cmdArr[4] * i) / trSteps;

          Strip.setPixelColorIdleDim(j, Strip.DimColor(Strip.Color(red, green, blue), cmdArr[5]));
        }
        Strip.show();
        delay(10);
      }
      break;
    } default:
      break;
  }
}




// 6. ====================================================================================== PATTERN COMPLETION ROUTINES ==============================================================================================
// Called when has completed full update cycle. Restarts animation, selects new colors
void StripComplete()
{
  switch (cmdArr[1]) {
    case 0:
      {
        dirFaded = true;
        dirSet = fadeDir;
        fadeDir = 0;
    } case 2:
      {
        uint32_t genColor;            // will store a new Color
        Strip.Color1 = Strip.Color2;  // Sets current Color as startpoint
        while (true) {                // makes sure new color isn't same as current and sets to new variable genColor.
          genColor = Strip.Wheel(random(255));
          if (fabs(int(Strip.Red(genColor) - Strip.Red(Strip.Color1))) > 75 || fabs(int(Strip.Green(genColor) - Strip.Green(Strip.Color1))) > 75
              || fabs(int(Strip.Blue(genColor) - Strip.Blue(Strip.Color1))) > 75) {
            if (int(Strip.Red(genColor) + Strip.Green(genColor) + Strip.Blue(genColor)) > 100) {
              Strip.Color2 = genColor;
              break;
            }
          }
        }

        break;
    } case 3:
      {
        uint32_t genColor;            // will store a new Color
        Strip.Color1 = Strip.Color2;  // Sets current Color as startpoint
        while (true) {                // makes sure new color isn't same as current and sets to new variable genColor.
          genColor = Strip.Wheel(random(255));
          if (fabs(int(Strip.Red(genColor) - Strip.Red(Strip.Color1))) > 75 || fabs(int(Strip.Green(genColor) - Strip.Green(Strip.Color1))) > 75
              || fabs(int(Strip.Blue(genColor) - Strip.Blue(Strip.Color1))) > 75) {
            if (int(Strip.Red(genColor) + Strip.Green(genColor) + Strip.Blue(genColor)) > 100) {
              Strip.Color2 = genColor;
              break;
            }
          }
        }

        break;
    } case 4:
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(5, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(180, 255)) * randBrightnessTo); // red
          fadeToArr[i][1] = int(double(random(20, 75)) * randBrightnessTo); // green
          fadeToArr[i][2] = 0; // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 5:
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(30, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first


        break;
    } case 6:
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
          fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 7:
      {
        //--------------------Room--------------------
        count++;
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(Strip.Red(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); //red
          fadeToArr[i][1] = int(double(Strip.Green(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(Strip.Blue(Strip.Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // blue

        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 8:
      {
        //--------------------Room--------------------
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(20, 100)) / 100.0;

          fadeToArr[i][0] = min(int(double(cmdArr[2] + random(-75, 75) * (pow(2.0 * (double(cmdArr[2]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // red
          fadeToArr[i][1] = min(int(double(cmdArr[3] + random(-75, 75) * (pow(2.0 * (double(cmdArr[3]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // green
          fadeToArr[i][2] = min(int(double(cmdArr[4] + random(-75, 75) * (pow(2.0 * (double(cmdArr[4]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255);  // blue
        
          fadeToArr[i][0] = max(fadeToArr[i][0], 0);
          fadeToArr[i][1] = max(fadeToArr[i][1], 0);
          fadeToArr[i][2] = max(fadeToArr[i][2], 0);
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first

        break;
    } case 9:
      {
        Strip.ColorSet(Strip.Color(0, 0, 0));
        Strip.checkBraking();
        Strip.show();
        for (int i = 0; i < numFiFl; i++) {
          stripArr[i][0] = random(0, stripLength);
        }
        for (int i = 0; i < numFiFl; i++) {  // at the end of the last step, start the next cycle
          stripArr[i][1] = random(0, 3);
          if (stripArr[i][1] == 0) {
            Strip.setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.DimColor(Strip.DimColor(Strip.Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            Strip.setPixelColorIdleDim(stripArr[i][0], Strip.DimColor(Strip.DimColor(Strip.getPixelColor((stripArr[i][0] + 1) % (int(stripLength) + 1)), 30), cmdArr[5]));
            Strip.setPixelColorIdleDim((stripArr[i][0] + 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelColorIdleDim((stripArr[i][0] - 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            stripArr[i][0] = (stripArr[i][0] + 1) % (int(stripLength) + 1);
          } else if (stripArr[i][1] == 1) {
            Strip.setPixelColorIdleDim((stripArr[i][0] - 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelColorIdleDim(stripArr[i][0], Strip.DimColor(Strip.DimColor(Strip.Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            Strip.setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
          } else if (stripArr[i][1] == 2) {
            Strip.setPixelColorIdleDim((stripArr[i][0] - 1) % (int(stripLength) + 1), Strip.DimColor(Strip.DimColor(Strip.Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            Strip.setPixelColorIdleDim(stripArr[i][0], Strip.DimColor(Strip.DimColor(Strip.getPixelColor((stripArr[i][0] - 1) % (int(stripLength) + 1)), 30), cmdArr[5]));
            Strip.setPixelColorIdleDim((stripArr[i][0] - 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelColorIdleDim((stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            stripArr[i][0] = (stripArr[i][0] - 1) % (int(stripLength) + 1);
          }
        }
        Strip.TotalSteps = random(10, 50);           // pick number of steps for the fireflies
        break;
    } default:
      break;
  }


  cometWheel = random(0, 255);     // prepare next comet
  cometDir = random(0, 2);
  cometStrt = random(0, stripLength);
}


// 7. ============================================================================ GETTING UART & WIFI DATA=======================================================================================================
// Get and interpret UART data 
void getVescData() {
  if (millis() - lastVescUpdate  > 50){
    lastVescUpdate = millis();

    double rpm;
    double volt;
    double amph;
    double tach;
    if (enableUART){
      if (vesc.getVescValues()) {
        rpm = vesc.data.rpm;
        volt = vesc.data.inpVoltage;
        amph = vesc.data.ampHours;
        tach = vesc.data.tachometerAbs;

        if(rpm > max(idleThresholdRpm, 1.0)) {                              // sets direction state
          vescDir = 1;
        } else if(rpm < -max(idleThresholdRpm, 1.0)) {
          vescDir = -1;
        } else {
          vescDir = 0;
        }
        
        //Serial.println("rpm: " + String(rpm));
        //Serial.println("voltage: "+ String(volt));
        //Serial.println("Ah: "+ String(amph));
        //Serial.println("tachometer: "+ String(tach));
        //Serial.println("Max idle rpm:" + String(idleThresholdRpm));
        //Serial.println("current direction state:" + String(vescDir));
      } else {
        //Serial.println("Failed to get data!");
        //Serial.println(vescDir);
        return;                                                            // dont change anything
      }
    } else if (simulateRpmData){                                           // for debug / demo
      rpm = 11460.0 * sin(double(millis())/10000.0) + 5000.0 * sin(double(millis())/1000.0);                       // Feeds wavy sinewave data to rpm for testing lights reacting to speed data functions

       if(rpm > idleThresholdRpm) {
          vescDir = 1;
        } else if(rpm < -idleThresholdRpm) {
          vescDir = -1;
        } else {
          vescDir = 0;
        }
    }

    rpmHist[5] = rpmHist[4];                                               // Running list of past 6 rpm values, ~50ms between, index 0 most recent
    rpmHist[4] = rpmHist[3];
    rpmHist[3] = rpmHist[2];
    rpmHist[2] = rpmHist[1];
    rpmHist[1] = rpmHist[0];
    rpmHist[0] = rpm;

    if (((fabs(rpmHist[0]) + fabs(rpmHist[1]) + fabs(rpmHist[2]))          // Compares a simple average for falling rpm for braking flag
        - (fabs(rpmHist[3]) + fabs(rpmHist[4]) + fabs(rpmHist[5]))) / 3.0  < - max(400.0 * (1.0 - brakingSens), 10.0) && vescDir != 0) {
      braking = true;
    } else{
      braking = false;
    }
  }
}


// Reads Web Command State and Forwards it to LED Code
void handleWiFiUpdates(){ 
   // compare the previous status to the current status
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();

    /*if (status == WL_AP_CONNECTED) {
      // a device has connected to the AP
      Serial.println("Device connected to AP");
    } else {
      // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP");
    }*/
  }


  // What action to perform
  // position in sequences array for selected sequence
  static int mode = cmdArr[1];
  // delay (speed) in ms (default 1 second)
  static int brightness = cmdArr[5];
  // Whether direction reversed
  static bool OnOff = cmdArr[0];
  // Number of colour options cannot exceed number of LEDs
  // One more included in array to allow terminating null
  // Must always have at last one color in position 0
  //static uint32_t colors[LED_COUNT+1] = {Strip.Color(255,255,255)};
  // should always be 1 color
  static int num_colors = 1;

  // Message to send from sequence request
  static String status_msg = "Ready";

  // What page to reply with (it's only after action handled that we return page)
  static int web_req = URL_DEFAULT;


  if (millis() - lastWebmillis > 500){
    lastWebmillis = millis();
    WiFiClient client = server.available();   // listen for incoming clients

    if (client) {                             // if you get a client,
      if (DEBUG > 1) Serial.println("new client");           // print a message out the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          if (DEBUG > 1) Serial.write(c);                    // print it out the serial monitor
          if (c == '\n') {                    // if the byte is a newline character

            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              if (web_req == URL_SEQUENCE) {
                showStatus(client, status_msg);
                web_req = URL_DEFAULT;
              }
              else if (web_req == URL_CSS) {
                showCSS(client);
                // reset web request
                web_req = URL_DEFAULT;
              }
              else if (web_req == URL_JS) {
                showJS(client);
                web_req = URL_DEFAULT;
              }
              else if (web_req == URL_JSON) {
                showJSON(client);
                web_req = URL_DEFAULT;
              }
              else if (web_req == URL_JQUERY) {
                showJQuery(client);
                web_req = URL_DEFAULT;
              }
              else if (web_req == URL_JQUERYUI) {
                showJQueryUI(client);
                web_req = URL_DEFAULT;
              }
              // default return web page
              else {
                showWebPage(client);
              }
              // break out of the while loop:
              break;
            } else {    // if you got a newline, then clear currentLine:
                currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }

          // temp variables for assessing arguments
          String url_arguments;
          String arg_name;
          String arg_value;

          // Only check for URL if it's a GET <url options> HTTP/ (ignore the http version number)
          if (currentLine.startsWith("GET") && currentLine.endsWith("HTTP/")) {
            String url_string = currentLine.substring(4, currentLine.indexOf(" HTTP/"));
            if (url_string.startsWith("/pixels.css")) {
              if (DEBUG == 1) Serial.println ("Request: /pixels.css");
              web_req = URL_CSS;
            }
            else if (url_string.startsWith("/pixels.js")) {
              if (DEBUG == 1) Serial.println ("Request: /pixels.js");
              web_req = URL_JS;
            }
            else if (url_string.startsWith("/modes.json")) {
              if (DEBUG == 1) Serial.println ("Request: /modes.json");
              web_req = URL_JSON;
            }
            else if (url_string.startsWith("/jquery.min.js")) {
              if (DEBUG == 1) Serial.println ("Request: /jquery.min.js");
              web_req = URL_JQUERY;
            }
            else if (url_string.startsWith("/jquery-ui.min.js")) {
              if (DEBUG == 1) Serial.println ("Request: /jquery-ui.min.js");
              web_req = URL_JQUERYUI;
            }
            // Sequence request
            else if (url_string.startsWith("/set")) {
              if (DEBUG == 1) {
                Serial.print ("Request: ");
                Serial.println (url_string);
              }
              url_arguments = url_string.substring(5);
              // take off each argument 
              while (url_arguments.length() > 0) {
                // get to = 
                // if no = then no more arguments
                if (url_arguments.indexOf("=") < 1) break;
                // strip to = (the name of the argument)
                arg_name = url_arguments.substring(0, url_arguments.indexOf("="));
                url_arguments = url_arguments.substring(url_arguments.indexOf("=")+1);
                // strip to & (if there is another)
                if (url_arguments.indexOf("&") < 1) {
                  arg_value = url_arguments;
                  //break;
                }
                else {
                  arg_value = url_arguments.substring(0, url_arguments.indexOf("&"));
                  url_arguments = url_arguments.substring(url_arguments.indexOf("&")+1);}
                  // Validate argument
                  if (arg_name == "mode") {
                    int mode_value = get_mode (arg_value);
                    // update action if it's a valid entry (not -1)
                    if (mode_value >=0) mode = mode_value;
                  }
                  else if (arg_name == "brightness") {
                      // toInt returns 0 if error so test for that separately
                      if (arg_value == "0") brightness = 0;
                      else {
                      int brightness_val = arg_value.toInt();
                      // check brightness_val is valid
                      if (brightness_val > 0) brightness = brightness_val;
                      } 
                  }
                  else if (arg_name == "onoff") {
                    if (arg_value == "0") OnOff = true;
                    else if (arg_value == "1") OnOff = false;
                  }
                  else if (arg_name == "pickedcolor") {
                        read_colors (arg_value);
                      // If no colors then set to default (1 white)
                      // shouldn't get this except on malformed request
                      if (num_colors == 0) {
                        read_colors ("ffffff");
                      }
                  }
                  // Otherwise invalid argument so ignore
                  else if (DEBUG > 0) {
                    Serial.print ("Request invalid:");
                    Serial.println (url_string);
                  }
              }
              web_req = URL_SEQUENCE;

            }
            else {
              web_req = URL_HTML;
            }
          }

        }
      }
      // close the connection:
      client.stop();
      if (DEBUG > 1) Serial.println("client disconnected");
    }

    updateCommands(OnOff, mode, usrColor[0], usrColor[1], usrColor[2], brightness);
  }
}

// Gets Web Command State and comapres it to current LED state. Updates LED state if they disagree.
void updateCommands(bool reqOnOff, int reqMode, int reqRed, int reqGreen, int reqBlue, int reqBrightness){
  if (cmdArr[0] != reqOnOff || cmdArr[1] != reqMode || cmdArr[2] != reqRed || cmdArr[3] != reqGreen || cmdArr[4] != reqBlue){
    cmdArr[0] = reqOnOff;
    cmdArr[1] = reqMode;
    cmdArr[2] = reqRed;
    cmdArr[3] = reqGreen;
    cmdArr[4] = reqBlue;
    Serial.println(cmdArr[1]);
    ControlPanel();
  }
    if (cmdArr[5] != reqBrightness){
    cmdArr[5] = reqBrightness;
  }
}

// get mode number (index) from mode list (based on mode_name). Return index in array, or -1 if not valid
int get_mode (String arg_value){
  for (int i=0; i < (sizeof(modes)/sizeof(mode)); i++){
    if (String(modes[i].mode_name) == arg_value) return i;
  }
  // if not found in array search return -1 for not found
  return -1;
}



// reads a color string and sets r, g, b into usrColor. later usrColor is compared to cmdArr and updated. 
void read_colors (String color_string){
  String this_color_string = color_string;
  // work through colors
  // not enough characters for a string so return
  if (this_color_string.length()<6) return;
  // Convert each pair to a rgb value

  int r = h2col (this_color_string.substring(0, 2));
  int g = h2col (this_color_string.substring(2, 4));
  int b = h2col (this_color_string.substring(4, 6));

  // if any are invalid then return
  if (r < 0 || g < 0 || b < 0) return;

  usrColor[0] = r;
  usrColor[1] = g;
  usrColor[2] = b;

  //Serial.println(r);
  //Serial.println(g);
  //Serial.println(b);

  // if not enough characters for another color then end
  if (this_color_string.length() < 13) {
    return;
  }
  // check if there is a comma in which case strip current color and comma and repeat
  if (this_color_string[6] == ',') {
    return;
  }
}

// gets 2 digit hex and coverts to decimal e.g:  FF -> 255
int h2col (String hexstring){
  int number = (int) strtol( &hexstring[0], NULL, 16);
  return number;
}



// verifies if connection succeeded
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}





// 8. ==================================================================== ARDUINO MAIN LOOP ========================================================================================

void setup() {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  //boot up command states
  cmdArr[0] = 1;            // 0 - off, 1 - on
  cmdArr[1] = bootMode;            // modes 0 - 13
  cmdArr[2] = bootRed;          // red
  cmdArr[3] = bootGreen;          // green
  cmdArr[4] = bootBlue;          // blue
  cmdArr[5] = bootBrightness;           // brightness slider value

  usrColor[0] = cmdArr[2];
  usrColor[1] = cmdArr[3];  // initalize user set color
  usrColor[2] = cmdArr[4];  

  Serial.begin(9600);       

 // check for the WiFi module:
  /*if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }*/

  // print the network name (SSID);
  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  // Create open network. Change this line if you want to create an WEP network:
  if (!WiFi.softAP(ssid, pass)) {
    log_e("Soft AP creation failed.");
    while(1);
  }
  IPAddress myIP = WiFi.softAPIP();
  //status = WiFi.begin(ssid, pass);
  /*if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }*/
  
  // wait 100 ms for connection:
  delay(100);

  server.begin();                           // start the web server on port 80
  if (DEBUG > 0) printWifiStatus();         // you're connected now, so print out the status

  Serial1.begin(115200);                    // VESC UART data
  vesc.setSerialPort(&Serial1);

  // initialize pixels
  //Strip.Color1 = Strip.Wheel(random(255));
  Strip.Color2 = Strip.Wheel(random(255));
  Strip.begin();
  Strip.setBrightness(master_brightness);   // Set hard brightness limiter
  ControlPanel();
  Strip.show();
}



void loop() {
  handleWiFiUpdates();
  getVescData();
  Strip.Update();
}

