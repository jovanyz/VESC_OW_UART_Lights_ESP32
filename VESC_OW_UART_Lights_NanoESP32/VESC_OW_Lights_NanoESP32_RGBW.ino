// Libraries. Huge thanks to this guy https://www.penguintutor.com/projects/arduino-rp2040-pixelstrip as a lot this code is originally theirs.
// OTA Libraries
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <WiFiClient.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>

#include "./src/VescUart/src/VescUart.h"
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

// Designed for static IP address so you can connect to Arduino
// These are lists and need to be commas instead of dots
// eg. for IP address 192.168.0.1 use 192,168,0,1 instead
#define IP_ADDR 192,168,0,44
#define IP_DNS 8,8,8,8          // Google DNS
#define IP_GW 192,168,0,1       // default gateway (router)
#define IP_SUBNET 255,255,255,0 

// what level of debug from serial console
// higher number the more debug messages
// recommend level 1 (includes occassional messages)
#define DEBUG 2
#define LEDR 14
#define LEDG 15
#define LEDB 16
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

struct commands{
  int cmdArr[8];                             // array of commands [0/1 OnOff, 0-13 Mode, 0-255 Red, 0-255 Geen, 0-255 Blue, 0-100 Brightness, unused...] 
  int usrColor [4];                          // stores website requested color, Updates cmdArr if Colors dont match.
};

// For Asymmetric Fade Patterns Modes 4-8
struct patterns{
  int fadeFromArr[pts + 1][4];               // for RGBW Values
  int fadeArr[pts + 1][4];                   // fadeFrom is starting configuration ...
  int fadeToArr[pts + 1][4];                 // ... fadeTo, is ending configuration
  int waveCount;                             // used to track overall colorshift in ColorWave[M7]

  // For Fireflies Pattern [M9]
  int stripArr[numFiFl][2];                  // [2] because first datapoint is location, second is direction (right -1, left, 1, stay 0)

  // For Comet Pattern [M11]
  int cometDir;                              // direction: forwards of backwards
  int cometStrt;                             // starting pixel
  int cometWheel;                            // color
};

// VESC UART Data
struct vData{
  double rpmHist[6];    // take past 6 rpm value, 300 ms window
  int vescDir;                           // current OW direction: 1 forward, -1 backward, 0 idle
  bool braking;                      // true when rpm decel detected
  bool brakingFaded;                  // flag for transitions between brakelights and tail lights
  int brakingFadeSteps;                 // 20 steps to fade to normal after braking
  int brakingFadeIndex;                  // fade step index

  double motorCurrent;
  double volt;
  double duty;
  double pitch;
  double adc1;
  double adc2;

  long int idleTime;
};

struct brake{
  bool dirFaded;                      // tracks changing direction red/white fading progress
  int fadeDir;                        // requested fade direction
  int dirSet;                         // which direction was just set as front -1 rev, 1 forw, 0 idle

  const double idleThresholdRpm;      // converts mph to erpm (15 for 30/2 motor poles)
};

commands CMD = {{0}, {0}};
patterns PTRN = {{{0}}, {{0}}, {{0}}, 1, {{0}}, 0, 0, 0};
vData  vDATA = {{0}, 0, false, true, 20, 0, 0, 0, 0, 0, 0, 0, 0};
brake BRK = {true, 0, 1, idleThresholdMph / (11.0 * PI * 60.0) * 63360.0 * 15.0};

int OTAState = 0;

bool BLEPause;
int UARTBle = 0;

//Bluetooth Bridge
int MTU_SIZE = 128;
int PACKET_SIZE = MTU_SIZE - 3;
NimBLEServer *pServer = nullptr;
NimBLEService *pServiceVesc = nullptr;
NimBLEService *pServiceRescue = nullptr;
NimBLECharacteristic *pCharacteristicVescTx = nullptr;
NimBLECharacteristic *pCharacteristicVescRx = nullptr;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define VESC_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define VESC_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define VESC_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

std::string vescBuffer;
std::string updateBuffer;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
  {
    ESP_LOGI(LOG_TAG_BLESERVER, "Client connected: %s", NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    ESP_LOGI(LOG_TAG_BLESERVER, "Multi-connect support: start advertising");
    deviceConnected = true;
    NimBLEDevice::startAdvertising();
  }

  void onDisconnect(NimBLEServer *pServer)
  {
    ESP_LOGI(LOG_TAG_BLESERVER, "Client disconnected - start advertising");
    deviceConnected = false;
    NimBLEDevice::startAdvertising();
  }

  void onMTUChange(uint16_t MTU, ble_gap_conn_desc *desc)
  {
    ESP_LOGI(LOG_TAG_BLESERVER, "MTU changed - new size %d, peer %s", MTU, NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    MTU_SIZE = MTU;
    PACKET_SIZE = MTU_SIZE - 3;
  }
};

char tmpbuf[1024]; // CAUTION: always use a global buffer, local buffer will flood the stack

void dumpBuffer(std::string header, std::string buffer)
{
  if (esp_log_level_get("BleServer") < ESP_LOG_DEBUG)
  {
    return;
  }
  int length = snprintf(tmpbuf, 50, "%s : len = %d / ", header.c_str(), buffer.length());
  for (char i : buffer)
  {
    length += snprintf(tmpbuf + length, 1024 - length, "%02x ", i);
  }
  ESP_LOGD(LOG_TAG_BLESERVER, "%s", tmpbuf);
}

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    ESP_LOGD(LOG_TAG_BLESERVER, "onWrite to characteristics: %s", pCharacteristic->getUUID().toString().c_str());
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0)
    {
      if (pCharacteristic->getUUID().equals(pCharacteristicVescRx->getUUID()))
      {
        dumpBuffer("BLE/UART => VESC: ", rxValue);
        for (int i = 0; i < rxValue.length(); i++)
        {
          Serial0.write(rxValue[i]);
        }
      }
    }
  }
};


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
        if (CMD.cmdArr[1] == 0){
          analogWrite(LEDR,  255 - CMD.cmdArr[2] * 0.75);
          analogWrite(LEDG,  255 - CMD.cmdArr[3] * 0.75);
          analogWrite(LEDB,  255 - CMD.cmdArr[4] * 0.75); 
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
      Interval = 25;
      TotalSteps = 40;
      Index = 0;
    }

    void StandardUpdate()
    { 
      if(BRK.dirFaded && vDATA.rpmHist[0] * vDATA.rpmHist[1] < 0.0){        // detects direction flip, checks two steps back into history in case "0" is captured
        BRK.dirFaded = false;                                     // flag for fading
        if (vDATA.rpmHist[0] < 0.0){
          BRK.fadeDir = -1;                                       // set reversing
        }
        else if (vDATA.rpmHist[0] > 0.0){
          BRK.fadeDir = 1;                                        // set forward
        } 
      } else if(BRK.dirFaded && BRK.dirSet == 1 && vDATA.rpmHist[0] < 0.0){   // if fade completed but current direction is wrong 
        BRK.dirFaded = false;                                     // flag for fade again
        BRK.fadeDir = -1;                                         // correct the direction
        BRK.dirSet = 0;                                           // set direction check idle 
      } else if(BRK.dirFaded && BRK.dirSet == -1 && vDATA.rpmHist[0] > 0.0){  
        BRK.dirFaded = false;
        BRK.fadeDir = 1;
        BRK.dirSet = 0;
      } else if(BRK.dirFaded && vDATA.rpmHist[0] > 0.0){                  // if forward & fade is correct 
        for(int i = 0; i < breakPoint; i++){                   // set nose color
          setPixelFiltered(i, dimColor(Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]), CMD.cmdArr[6]));
        } 
        for(int i = breakPoint; i < numPixels(); i++){          // set tail red, brightness scaled to rpm
          setPixelFiltered(i, Color(min(15.0 + 125.0 * pow(fabs(vDATA.rpmHist[0]) / 6000.0, 2.0) , 255.0), 0, 0));
        } 
      } else if(BRK.dirFaded && vDATA.rpmHist[0] < 0.0){                    // if backward & fade is correct
        for(int i = breakPoint; i < numPixels(); i++){          // set tail to color
          setPixelFiltered(i, dimColor(Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]), CMD.cmdArr[6]));
        } 
        for(int i = 0; i < breakPoint; i++){                    // set nose red, brightness scaled to rpm
          setPixelFiltered(i, Color(min(15.0 + 125.0 * pow(fabs(vDATA.rpmHist[0]) / 6000.0, 2.0) , 255.0), 0, 0));
        }
      } else {
        for(int i = 0; i < numPixels(); i++){                    // set nose red, brightness scaled to rpm
          batteryMeter(i, Color(15, 0, 0));
        }
      }
      
      if (!BRK.dirFaded) {                                          // if requested direction change
        Increment();                                            // start fade index count
        if(BRK.fadeDir == 1){                                       // if requested fade nose forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(60.0 * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + CMD.cmdArr[2] * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = CMD.cmdArr[3] * (double(Index + 1)/double(TotalSteps));
            uint8_t blue = CMD.cmdArr[4] * (double(Index + 1)/double(TotalSteps));
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
          for(int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(CMD.cmdArr[2] * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + 60 * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = CMD.cmdArr[3] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            uint8_t blue = CMD.cmdArr[4] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
        } else if (BRK.fadeDir == -1){                               // if requested fade tail forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(CMD.cmdArr[2] * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + 60 * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = CMD.cmdArr[3] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            uint8_t blue = CMD.cmdArr[4] * (double(TotalSteps - Index - 1)/double(TotalSteps));
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
          for(int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(60 * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + CMD.cmdArr[2] * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = CMD.cmdArr[3] * (double(Index + 1)/double(TotalSteps));
            uint8_t blue = CMD.cmdArr[4] * (double(Index + 1)/double(TotalSteps));
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
        }
      }
      
      checkBraking();
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
        setPixelFiltered(i, dimColor(Wheel(int((double(i) * 256.0 / (stripLength / numRainbow)) + double(Index)) & 255), CMD.cmdArr[6]));
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
      Interval = max(70.0 - (fabs(vDATA.rpmHist[0]) / 164.0) + 5.0, 1.0);
      TotalSteps = stripLength - 1;
      Index = 0;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
      colorSetRange(dimColor(Color2, CMD.cmdArr[6]), 0, Index + 1);
      colorSetRange(dimColor(Color1, CMD.cmdArr[6]), Index + 1, stripLength);
      checkBraking();
      show();
      Increment();
      Interval = max(70.0 - (fabs(vDATA.rpmHist[0]) / 164.0) + 5.0, 1.0);
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
      ColorSet(dimColor(Color(red, green, blue), CMD.cmdArr[6]));
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
          uint8_t red = ((PTRN.fadeArr[i][0] * (pixR - j)) + (PTRN.fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((PTRN.fadeArr[i][1] * (pixR - j)) + (PTRN.fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((PTRN.fadeArr[i][2] * (pixR - j)) + (PTRN.fadeArr[i + 1][2] * j)) / pixR;
          setPixelFiltered(i * pixR + j, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
        }
        uint8_t red1 = ((PTRN.fadeArr[pts - 1][0] * (pixR - j)) + (PTRN.fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((PTRN.fadeArr[pts - 1][1] * (pixR - j)) + (PTRN.fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((PTRN.fadeArr[pts - 1][2] * (pixR - j)) + (PTRN.fadeArr[0][2] * j)) / pixR;
        setPixelFiltered((modif / divBy - 1) * pixR + j, dimColor(Color(red1, green1, blue1), CMD.cmdArr[6]));
      }
      
      checkBraking();
      show();
      Increment();

      for (int i = 0; i < pts + 1; i++)
      {
        PTRN.fadeArr[i][0] = ((PTRN.fadeFromArr[i][0] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][0] * Index)) / TotalSteps;
        PTRN.fadeArr[i][1] = ((PTRN.fadeFromArr[i][1] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][1] * Index)) / TotalSteps;
        PTRN.fadeArr[i][2] = ((PTRN.fadeFromArr[i][2] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][2] * Index)) / TotalSteps;
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
          uint8_t red = ((PTRN.fadeArr[i][0] * (pixR - j)) + (PTRN.fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((PTRN.fadeArr[i][1] * (pixR - j)) + (PTRN.fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((PTRN.fadeArr[i][2] * (pixR - j)) + (PTRN.fadeArr[i + 1][2] * j)) / pixR;
          setPixelFiltered(i * pixR + j, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
        }
        uint8_t red1 = ((PTRN.fadeArr[pts - 1][0] * (pixR - j)) + (PTRN.fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((PTRN.fadeArr[pts - 1][1] * (pixR - j)) + (PTRN.fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((PTRN.fadeArr[pts - 1][2] * (pixR - j)) + (PTRN.fadeArr[0][2] * j)) / pixR;
        setPixelFiltered((modif / divBy - 1) * pixR + j, dimColor(Color(red1, green1, blue1), CMD.cmdArr[6]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        PTRN.fadeArr[i][0] = ((PTRN.fadeFromArr[i][0] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][0] * Index)) / TotalSteps;
        PTRN.fadeArr[i][1] = ((PTRN.fadeFromArr[i][1] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][1] * Index)) / TotalSteps;
        PTRN.fadeArr[i][2] = ((PTRN.fadeFromArr[i][2] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][2] * Index)) / TotalSteps;
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
          uint8_t red = ((PTRN.fadeArr[i][0] * (pixR - j)) + (PTRN.fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((PTRN.fadeArr[i][1] * (pixR - j)) + (PTRN.fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((PTRN.fadeArr[i][2] * (pixR - j)) + (PTRN.fadeArr[i + 1][2] * j)) / pixR;
          setPixelFiltered(i * pixR + j, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
        }
        uint8_t red1 = ((PTRN.fadeArr[pts - 1][0] * (pixR - j)) + (PTRN.fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((PTRN.fadeArr[pts - 1][1] * (pixR - j)) + (PTRN.fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((PTRN.fadeArr[pts - 1][2] * (pixR - j)) + (PTRN.fadeArr[0][2] * j)) / pixR;
        setPixelFiltered((modif / divBy - 1) * pixR + j, dimColor(Color(red1, green1, blue1), CMD.cmdArr[6]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        PTRN.fadeArr[i][0] = ((PTRN.fadeFromArr[i][0] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][0] * Index)) / TotalSteps;
        PTRN.fadeArr[i][1] = ((PTRN.fadeFromArr[i][1] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][1] * Index)) / TotalSteps;
        PTRN.fadeArr[i][2] = ((PTRN.fadeFromArr[i][2] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][2] * Index)) / TotalSteps;
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
          uint8_t red = ((PTRN.fadeArr[i][0] * (pixR - j)) + (PTRN.fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((PTRN.fadeArr[i][1] * (pixR - j)) + (PTRN.fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((PTRN.fadeArr[i][2] * (pixR - j)) + (PTRN.fadeArr[i + 1][2] * j)) / pixR;
          setPixelFiltered(i * pixR + j, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
        }
        uint8_t red1 = ((PTRN.fadeArr[pts - 1][0] * (pixR - j)) + (PTRN.fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((PTRN.fadeArr[pts - 1][1] * (pixR - j)) + (PTRN.fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((PTRN.fadeArr[pts - 1][2] * (pixR - j)) + (PTRN.fadeArr[0][2] * j)) / pixR;
        setPixelFiltered((modif / divBy - 1)  * pixR + j, dimColor(Color(red1, green1, blue1), CMD.cmdArr[6]));
      }

      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        PTRN.fadeArr[i][0] = ((PTRN.fadeFromArr[i][0] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][0] * Index)) / TotalSteps;
        PTRN.fadeArr[i][1] = ((PTRN.fadeFromArr[i][1] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][1] * Index)) / TotalSteps;
        PTRN.fadeArr[i][2] = ((PTRN.fadeFromArr[i][2] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][2] * Index)) / TotalSteps;
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
          uint8_t red = ((PTRN.fadeArr[i][0] * (pixR - j)) + (PTRN.fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((PTRN.fadeArr[i][1] * (pixR - j)) + (PTRN.fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((PTRN.fadeArr[i][2] * (pixR - j)) + (PTRN.fadeArr[i + 1][2] * j)) / pixR;
          setPixelFiltered(i * pixR + j, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
        }
        uint8_t red1 = ((PTRN.fadeArr[pts - 1][0] * (pixR - j)) + (PTRN.fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((PTRN.fadeArr[pts - 1][1] * (pixR - j)) + (PTRN.fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((PTRN.fadeArr[pts - 1][2] * (pixR - j)) + (PTRN.fadeArr[0][2] * j)) / pixR;
        setPixelFiltered((modif / divBy - 1) * pixR + j, dimColor(Color(red1, green1, blue1), CMD.cmdArr[6]));
      }

      TailTint();
      checkBraking();
      show();
      Increment();
      
      for (int i = 0; i < pts + 1; i++)
      {
        PTRN.fadeArr[i][0] = ((PTRN.fadeFromArr[i][0] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][0] * Index)) / TotalSteps;
        PTRN.fadeArr[i][1] = ((PTRN.fadeFromArr[i][1] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][1] * Index)) / TotalSteps;
        PTRN.fadeArr[i][2] = ((PTRN.fadeFromArr[i][2] * (TotalSteps - Index)) + (PTRN.fadeToArr[i][2] * Index)) / TotalSteps;
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
        PTRN.stripArr[i][0] = random(0, stripLength); // pick the location of fireflies
      }
    }

    // Update the Fireflies Pattern
    void FirefliesUpdate()
    {
      for (int i = 0; i < numFiFl; i++) {
        PTRN.stripArr[i][1] = random(0, 3);     // pick left, right, or stay
        if (PTRN.stripArr[i][1] == 0) {         // left
          setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), dimColor(dimColor(dimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), CMD.cmdArr[6]));
          setPixelFiltered(PTRN.stripArr[i][0], dimColor(dimColor(getPixelColor((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1)), 30), CMD.cmdArr[6]));
          setPixelFiltered((PTRN.stripArr[i][0] + 2) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelFiltered((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1), Color(0, 0, 0));
          PTRN.stripArr[i][0] = (PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1);
        } else if (PTRN.stripArr[i][1] == 1) {  // stay
          setPixelFiltered((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelFiltered(PTRN.stripArr[i][0], dimColor(dimColor(dimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), CMD.cmdArr[6]));
          setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), Color(0, 0, 0));
        } else if (PTRN.stripArr[i][1] == 2) {  // right
          setPixelFiltered((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1), dimColor(dimColor(dimColor(Color(66, 74, 19), random(0, 70)),
                           int((1.0 - fabs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), CMD.cmdArr[6]));
          setPixelFiltered(PTRN.stripArr[i][0], dimColor(dimColor(getPixelColor((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1)), 30), CMD.cmdArr[6]));
          setPixelFiltered((PTRN.stripArr[i][0] - 2) % (int(stripLength) + 1), Color(0, 0, 0));
          setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), Color(0, 0, 0));
          PTRN.stripArr[i][0] = (PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1);
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
        setPixelFiltered(random(0, stripLength), dimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index))))));
        setPixelFiltered(random(0, stripLength), dimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index + 24))))));
        setPixelFiltered(random(0, stripLength), dimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20, 20)),
                         int(50.0 * (1.0 + sin(PI / 36.0 * double(Index + 48))))));
      }
      for (int  i = 0; i < stripLength; i++) {
        setPixelColor(i, dimColor(getPixelColor(i), 80));
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
      PTRN.cometDir = random(0, 2);
      PTRN.cometStrt = random(0, stripLength);
      PTRN.cometWheel = random(0, 255);
    }
    // Update the Comet Pattern
    void CometUpdate()
    {
      int loc = 0;
      for (int i = 0; i < numPixels(); i++) {
        if (i == Index) {                     // if chosen pixel
          if (PTRN.cometDir == 0) {
            loc = i + PTRN.cometStrt;
            if (loc > int(stripLength)) {
              loc -= int(stripLength);
            }
          } else if (PTRN.cometDir == 1) {
            loc = PTRN.cometStrt - i;
            if (loc < 0) {
              loc += int(stripLength);
            }
          }
          setPixelFiltered(loc, dimColor(Wheel(PTRN.cometWheel + random(0, 30) - 15), CMD.cmdArr[6]));
        }
        else // Fading tail
        {
          setPixelFiltered(i, dimColor(dimColor(getPixelColor(i), 80), CMD.cmdArr[6]));
        }
      }
      
      checkBraking();
      show();
      Increment();
    }



// 2.12 PacMan--------------------------------------------------------------
    void PacMan(){
      ActivePattern = PACMAN;
      Interval = max(20.0 , 200.0 - (fabs(vDATA.rpmHist[0]) / 57.0) + 20.0);   // equals to 0 + 5.0 ms at 25mph
      TotalSteps = 5;
      Index = 0;
    }

    void PacManUpdate(){
      if (vDATA.rpmHist[0] < 0){
        colorSetRange(dimColor(Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]), CMD.cmdArr[6]), breakPoint, numPixels());
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
        colorSetRange(dimColor(Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]), CMD.cmdArr[6]), 0, breakPoint);
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
      Interval = max(20.0 , 200.0 - (fabs(vDATA.rpmHist[0]) / 57.0) + 20.0);
    }

// 2.13 TailColor-----------------------------------------------------------------
    void TailColor(){
      ActivePattern = TAIL_COLOR;
      Interval = 25;
      TotalSteps = 40;
      Index = 0;
    }

    void TailColorUpdate(){
      if(BRK.dirFaded && vDATA.rpmHist[0] * vDATA.rpmHist[1] < 0.0){        // detects direction flip, checks two steps back into history in case "0" is captured
        BRK.dirFaded = false;                                     // flag for fading
        if (vDATA.rpmHist[0] < 0.0){
          BRK.fadeDir = -1;                                       // set reversing
        }
        else if (vDATA.rpmHist[0] > 0.0){
          BRK.fadeDir = 1;                                        // set forward
        } 
      } else if(BRK.dirFaded && BRK.dirSet == 1 && vDATA.rpmHist[0] < 0.0){   // if fade completed but current direction is wrong 
        BRK.dirFaded = false;                                     // flag for fade again
        BRK.fadeDir = -1;                                         // correct the direction
        BRK.dirSet = 0;                                           // set direction check idle 
      } else if(BRK.dirFaded && BRK.dirSet == -1 && vDATA.rpmHist[0] > 0.0){  
        BRK.dirFaded = false;
        BRK.fadeDir = 1;
        BRK.dirSet = 0;
      } else if(BRK.dirFaded && vDATA.rpmHist[0] > 0.0){                  // if forward & fade is correct 
        for(int i = 0; i < breakPoint; i++){                   // set nose color
          setPixelFiltered(i, dimColor(Color1, CMD.cmdArr[6]));
        } 
        for(int i = breakPoint; i < numPixels(); i++){          // set tail red, brightness scaled to rpm
          setPixelFiltered(i, dimColor(Color2, CMD.cmdArr[6]));
        } 
      } else if(BRK.dirFaded && vDATA.rpmHist[0] < 0.0){                    // if backward & fade is correct
        for(int i = breakPoint; i < numPixels(); i++){          // set tail to color
          setPixelFiltered(i, dimColor(Color1, CMD.cmdArr[6]));
        } 
        for(int i = 0; i < breakPoint; i++){                    // set nose red, brightness scaled to rpm
          setPixelFiltered(i, dimColor(Color2, CMD.cmdArr[6]));
        }
      } else {
        for(int i = 0; i < numPixels(); i++){                    // set nose red, brightness scaled to rpm
          batteryMeter(i, Color2);
        }
      }
      
      if (!BRK.dirFaded) {                                          // if requested direction change
        Increment();                                            // start fade index count
        if(BRK.fadeDir == 1){                                       // if requested fade nose forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(Red(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Red(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = min(Green(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Green(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t blue = min(Blue(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Blue(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
          for(int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(Red(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Red(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = min(Green(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Green(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t blue = min(Blue(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Blue(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
        } else if (BRK.fadeDir == -1){                               // if requested fade tail forward
          for(int i = 0; i < breakPoint; i++){
            uint8_t red = min(Red(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Red(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = min(Green(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Green(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t blue = min(Blue(Color1) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Blue(Color2) * (double(Index + 1)/double(TotalSteps)), 255.0);
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
          for(int i = breakPoint; i < numPixels(); i++){
            uint8_t red = min(Red(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Red(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t green = min(Green(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Green(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            uint8_t blue = min(Blue(Color2) * (double(TotalSteps - Index - 1)/double(TotalSteps)) 
                              + Blue(Color1) * (double(Index + 1)/double(TotalSteps)), 255.0);
            setPixelFiltered(i, dimColor(Color(red, green, blue), CMD.cmdArr[6]));
          }
        }
      }
      
      checkBraking();
      show();
    }

// 2.14 PixelFinder--------------------------------------------------------------
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
        if (i == CMD.cmdArr[6])
          setPixelColor(i, Color(0, 255, 0));
        else
          setPixelColor(i, Color(0, 0, 0));
      }
      show();
      Increment();
    }





// 3. PATTERN TOOLS==========================================================================================================================================
        // Any last special modifies to be applied to each pixel, current dims lights to idleBrightness when not moving 
    void setPixelFiltered(uint16_t loc, uint32_t color) {
      //color = gamma32(color);
      uint8_t red = Red(color);
      uint8_t green = Green(color);
      uint8_t blue = Blue(color);
      uint8_t white = calcWhite(red, green, blue);
      uint32_t wcolor;

      if(vDATA.rpmHist[0] < 0 && loc > breakPoint){
        wcolor = Color(red, green, blue, white);
      } 
      else if(vDATA.rpmHist[0] > 0 && loc < breakPoint){
        wcolor = Color(red, green, blue, white);
      } else {
        wcolor = Color(red, green, blue, 0);
      }

      if (vDATA.vescDir == 0 && enableUART && BLEPause){   // dim lights smoothly if below 5mph 
        uint8_t rampWhite;
        if(vDATA.rpmHist[0] < 0 && loc > breakPoint){
          rampWhite = map(-vDATA.rpmHist[0], 0.0, BRK.idleThresholdRpm * 2, 0.0, white);
        } 
        else if(vDATA.rpmHist[0] > 0 && loc < breakPoint){
          rampWhite = map(vDATA.rpmHist[0], 0.0, BRK.idleThresholdRpm * 2, 0.0, white);
        } else {
          rampWhite = 0;
        }
        
        uint8_t rampBrightness = map(fabs(vDATA.rpmHist[0]), 0.0, BRK.idleThresholdRpm * 2, idleBrightness, CMD.cmdArr[6]);
        wcolor = Color(red, green, blue, rampWhite);
        if(!batteryMeter(loc, wcolor)){
          setPixelColor(loc, dimColor(wcolor, rampBrightness));
        }
      }else{
        setPixelColor(loc, wcolor);
      }
    }
    
    // Modifies colors for "tail" lights & when braking is detected-----------------------
    void checkBraking(){
      if (disableBrakingResponse){ // exit if disabled
        return;
      }

      if (fabs(vDATA.rpmHist[0]) < BRK.idleThresholdRpm){       // when at idle do nothing
        vDATA.braking = false;
        vDATA.brakingFaded = true;
        return;
      }
      if (vDATA.braking) {
        if(vDATA.brakingFaded && brakeBlink){                     // first frame of braking (brakingFaded is still true), checks brakeBlink enabled
          if (vDATA.rpmHist[0] < 0){                              // if forward
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(0, 0, 0));             // turn LEDs off
            }
            show();
            delay(brakeBlink_ms);                           // delay short while
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(255, 0, 0));           // set LEDs red
            }
            show();                                         
            delay(brakeBlink_ms);                           // delay short while
            for(int i = 0; i < breakPoint; i++){
              setPixelColor(i, Color(0, 0, 0));             // turn LEDs off
            }
            show();
            delay(brakeBlink_ms);                           // delay short while...
          } else {                                          // if going backwards, do same but for nose
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

        vDATA.brakingFaded = false;                         // flags for fade back to pattern

        if (vDATA.rpmHist[0] < 0){                          // if backwards
          for(int i = 0; i < breakPoint; i++){
            setPixelColor(i, Color(255, 0, 0));       // sets nose to red to complete blink pattern
          }
        } else {
          for(int i = breakPoint; i < numPixels(); i++){
            setPixelColor(i, Color(255, 0, 0));       // sets tail red
          } 
        }
      } else if (!vDATA.brakingFaded && !vDATA.braking) {         // first "frame" after braking is no longer detected fade back to pattern
        if (vDATA.rpmHist[0] >= 0){
          for (int i = breakPoint; i < numPixels(); i++){
            uint8_t red = (255 * (vDATA.brakingFadeSteps - vDATA.brakingFadeIndex) + (Red(getPixelColor(i)) * vDATA.brakingFadeIndex)) / vDATA.brakingFadeSteps;
            uint8_t green = (Green(getPixelColor(i)) * vDATA.brakingFadeIndex) / vDATA.brakingFadeSteps;
            uint8_t blue = (Blue(getPixelColor(i)) * vDATA.brakingFadeIndex) / vDATA.brakingFadeSteps;
            uint8_t white = calcWhite(red, green, blue);

            setPixelFiltered(i, Color(red, green, blue));
          }
        } else if (vDATA.rpmHist[0] < 0){
          for (int i = 0; i < breakPoint; i++){
            uint8_t red = (255 * (vDATA.brakingFadeSteps - vDATA.brakingFadeIndex) + (Red(getPixelColor(i)) * vDATA.brakingFadeIndex)) / vDATA.brakingFadeSteps;
            uint8_t green = (Green(getPixelColor(i)) * vDATA.brakingFadeIndex) / vDATA.brakingFadeSteps;
            uint8_t blue = (Blue(getPixelColor(i)) * vDATA.brakingFadeIndex) / vDATA.brakingFadeSteps;
            setPixelFiltered(i, Color(red, green, blue));
          }
        }
        vDATA.brakingFadeIndex++;
        if (vDATA.brakingFadeIndex >= vDATA.brakingFadeSteps){   // fade completed, setup for next time
          vDATA.brakingFaded = true;
          vDATA.brakingFadeIndex = 0;
        }
      }
    }
    
    //Called by some modes, rescales colors to add more red to "tail" lights
    void TailTint(){
      if (vDATA.rpmHist[0] < 0){
        for(int i = 0; i < breakPoint; i++){
          uint8_t r = min(20 + 1.5 * double(Red(getPixelColor(i))), 255.0);
          uint8_t g = 0.3 * double(Green(getPixelColor(i)));
          uint8_t b = 0.3 * double(Blue(getPixelColor(i)));
          uint8_t w = 0;

          setPixelColor(i, Color(r, g, b, w));

        }
      } else {
        for(int i = breakPoint; i < numPixels(); i++){
          uint8_t r = min(10 + 1.5 * double(Red(getPixelColor(i))), 255.0);
          uint8_t g = 0.3 * double(Green(getPixelColor(i)));
          uint8_t b = 0.3 * double(Blue(getPixelColor(i)));
          uint8_t w = 0;

          setPixelColor(i, Color(r, g, b, w));
        } 
      }
    }

    // Gets a scaled white value (0-255) from r, g, b values.
    uint8_t calcWhite(uint8_t r, uint8_t g, uint8_t b){
      uint8_t whiteVal = pow(double((r + g + b) / 3) / 255.0 , 2.0) * 255.0;
      return whiteVal;
    }

    bool batteryMeter(uint16_t loc, uint32_t color){
      if (vDATA.rpmHist[0] != 0){
        vDATA.idleTime = millis();
      }

      if((vDATA.adc1 > 1.0 || vDATA.adc2 > 1.0) && millis() - vDATA.idleTime > 500){
        int bPercent = (vDATA.volt - 2.8 * sCells) / (4.16 * sCells - 2.8 * sCells) * 20.0;

        if (loc < bPercent * 0.7){
          setPixelColor(loc, gamma32(Wheel(double(loc) / 20.0 * 115.0)));
        } 
        else if (loc < bPercent){
          setPixelColor(loc, gamma32(dimColor(Wheel(double(loc) / 20.0 * 115.0), (1 + 0.7 * sin(double(loc) / 3.0 + double(millis()) / 350.0)) * 58.0)));
        }
        else if (loc > breakPoint){
          setPixelColor(loc, color);
        }
        else{
          setPixelColor(loc, Color(0, 0, 0));
        }
        return true;
      }else {
        return false;
      }
    }

    // Calculate percent dimmed version of a color (used by Comet [M10])
    uint32_t dimColor(uint32_t color, uint8_t percent)
    {
      uint8_t red = double(Red(color)) * (double(percent) / 100.0);
      uint8_t green = double(Green(color)) * (double(percent) / 100.0);
      uint8_t blue = double(Blue(color)) * (double(percent) / 100.0);
      uint8_t white = double(White(color)) * (double(percent) / 100.0);

      uint32_t dimColor = Color(red, green, blue, white);
      return dimColor;
    }

    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
      for (int i = 0; i < stripLength; i++)
      {
        setPixelFiltered(i, dimColor(color, CMD.cmdArr[6]));
      }
    }

    // Set all pixels in a range to a color
    void colorSetRange(uint32_t color, uint16_t strt, uint16_t fnsh)
    {
      if (strt >= fnsh){
        return;
      }
      if (fnsh > stripLength){
        fnsh = stripLength;
      }
      for (int i = strt; i < fnsh; i++)
      {
        setPixelFiltered(i, dimColor(color, CMD.cmdArr[6]));
      }
    }

    // Returns the White component of a 32-bit color
    uint8_t White(uint32_t color)
    {
      return (color >> 24) & 0xFF;
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
NeoPatterns Strip = NeoPatterns(stripLength, datPinNum, NEO_GRBW + NEO_KHZ800, &StripComplete);
Adafruit_NeoPixel Button(1, buttonPin, NEO_GRB + NEO_KHZ800);

// =================================================================== END OF DISPLAY FUNCTIONS ======================================================================





// 4. ==================================================================Strip Command Center=========================================================================================================================================================
void ControlPanel() {   // called when cmdArr is updated. Calls for transition to new requested strip state and starts the pattern.
  if (!CMD.cmdArr[0]) {     // if turned off, fade to black
    Strip.ActivePattern = NONE;
    for (int i = 0; i < 50; i++) {
      for (int j = 0; j < Strip.numPixels(); j++) {
        uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint8_t white = (Strip.White(Strip.getPixelColor(j)) * (50 - i)) / 50;
        uint32_t color = Strip.Color(red, green, blue, white);

        Strip.setPixelColor(j, color);
      }
      Strip.show();
      delay(10);
    }
  } else {    
    Transition();                               // fade to state
    switch (CMD.cmdArr[1]) {
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
          Strip.Fade(Strip.Color1, Strip.Color2, 50, 40);
          break;
      } case 4:
        {
          Strip.Lava(40, 40);
          break;
      } case 5:
        {
          Strip.Canopy(30, 40);
          break;
      } case 6:
        {
          Strip.Ocean(20, 40);
          break;
      } case 7:
        {
          Strip.RollingWave(30, 40);
          break;
      } case 8:
        {
          Strip.ColorWave(30, 40);
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
  int trSteps = 30;
  switch (int(CMD.cmdArr[1])) {
    case 0:
      {
        BRK.dirFaded = true;
        BRK.fadeDir = 0;
        BRK.dirSet = 1;
        for (int  i = 0; i < trSteps; i++) {
          for (int j = breakPoint; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.Color(20, 0, 0);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white = 0;
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);
          }
          for (int j = 0; j < breakPoint; j++) {
            uint32_t col = Strip.Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white = Strip.calcWhite(red, green, blue);
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);       
          }   
          Strip.show();
          delay(50);
        }
        break;
    } case 1:
      {
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.dimColor(Strip.Wheel(int(double(j) * 256.0 / (stripLength / 2.0)) & 255), idleBrightness);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white;
            if(j < breakPoint){
              white = Strip.calcWhite(red, green, blue);
            } else {
              white = 0;
            }
            uint32_t color = Strip.Color(red, green, blue, white);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(50);
        }
        break;
    } case 2:
      {
        randomSeed(millis());
        Strip.Color1 = Strip.Wheel(random(255));    // choose starting color
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.dimColor(Strip.Color1, idleBrightness);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white;
            if(j < breakPoint){
              white =  Strip.calcWhite(red, green, blue);
            } else {
              white = 0;
            }

            uint32_t color = Strip.Color(red, green, blue, white);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(50);
        }
        break;
    } case 3:
      {
        randomSeed(millis());
        Strip.Color1 = Strip.Wheel(random(255));    // choose starting color
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.dimColor(Strip.Color1, idleBrightness);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white;

            if(j < breakPoint){
              white =  Strip.calcWhite(red, green, blue);
            } else {
              white = 0;
            }

            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);          
          }
          Strip.TailTint();
          Strip.show();
          delay(50);
        }
        break;
    } case 4:  //red wave, lava, set start state and fade to start state
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(180, 255)) * randBrightnessTo); // red
          PTRN.fadeToArr[i][1] = int(double(random(20, 75)) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = 0; // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 5:  //green wave, canpoy, set start state and fade to start state
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
          PTRN.fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 6:  //blue wave, ocean, set start state and fade to start state
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
          PTRN.fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
          PTRN.fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 7:  //rolling wave, set start state and fade to start state, red
      {
        PTRN.waveCount++;
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(Strip.Red(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); //red
          PTRN.fadeToArr[i][1] = int(double(Strip.Green(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = int(double(Strip.Blue(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // blue

        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 8:  //color wave, ocean, set start state and fade to start state, set color
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = min(int(double(CMD.cmdArr[2] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[2]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // red
          PTRN.fadeToArr[i][1] = min(int(double(CMD.cmdArr[3] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[3]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // green
          PTRN.fadeToArr[i][2] = min(int(double(CMD.cmdArr[4] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[4]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255);  // blue
          
          PTRN.fadeToArr[i][0] = max(PTRN.fadeToArr[i][0], 0);
          PTRN.fadeToArr[i][1] = max(PTRN.fadeToArr[i][1], 0);
          PTRN.fadeToArr[i][2] = max(PTRN.fadeToArr[i][2], 0);
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 9:  // fireflies fade to black
      {
        for (int i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t white = (Strip.White(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint32_t color = Strip.Color(red, green, blue, white);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(40);
        }
        break;
    } case 10: // confetti fade to black
      {
        for (int i = 0; i < trSteps; i++) {
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t white = (Strip.White(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint32_t color = Strip.Color(red, green, blue, white);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(40);
        }
        break;
    } case 11:
      {
        randomSeed(millis());
        Strip.Color1 = Strip.Wheel(random(255));    // choose starting color
        break;
    } case 12: // pacman fade to color front, black back
      {
        for (int  i = 0; i < trSteps; i++) {
          for (int j = 0; j < breakPoint; j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i) + CMD.cmdArr[2] * i) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i) + CMD.cmdArr[3] * i) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i) + CMD.cmdArr[4] * i) / trSteps;
            uint8_t white = (Strip.White(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);
            Strip.setPixelColor(j, color);          
          }
          for (int j = breakPoint + 1; j <Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t white = 0;

            Strip.setPixelColor(j, Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]));
          }
          uint8_t redPM = (Strip.Red(Strip.getPixelColor(breakPoint)) * (trSteps - i) + 255 * i) / trSteps;
          uint8_t greenPM = (Strip.Green(Strip.getPixelColor(breakPoint)) * (trSteps - i) + 168 * i) / trSteps;
          uint8_t bluePM = (Strip.Blue(Strip.getPixelColor(breakPoint)) * (trSteps - i)) / trSteps;
          uint8_t whitePM = (Strip.White(Strip.getPixelColor(breakPoint)) * (trSteps - i)) / trSteps;
          uint32_t colorPM = Strip.Color(redPM, greenPM, bluePM, whitePM);

          Strip.setPixelColor(breakPoint, colorPM);
          
          Strip.show();
          delay(50);
        }
        break;
    } case 13:  //tailcolor fade to chosen color
        {
        BRK.dirFaded = true;
        BRK.fadeDir = 0;
        BRK.dirSet = 1;
        Strip.Color2 = Strip.Color1;
        Strip.Color1 = Strip.Color(CMD.cmdArr[2], CMD.cmdArr[3], CMD.cmdArr[4]);

        for (int  i = 0; i < trSteps; i++) {
          for (int j = breakPoint; j < Strip.numPixels(); j++) {
            uint32_t col = Strip.dimColor(Strip.Color2, idleBrightness);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white = 0;
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);          
          }
          for (int j = 0; j < breakPoint; j++) {
            uint32_t col = Strip.dimColor(Strip.Color1, idleBrightness);
            uint8_t red = ((Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Red(col) * i)) / trSteps;
            uint8_t green = ((Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Green(col) * i)) / trSteps;
            uint8_t blue = ((Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) + (Strip.Blue(col) * i)) / trSteps;
            uint8_t white = Strip.calcWhite(red, green, blue);
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(50);
        }
        break;
    } case 14: //pixel finder fade to black
        {
        for (int i = 0; i < trSteps; i++){
          for (int j = 0; j < Strip.numPixels(); j++) {
            uint8_t red = (Strip.Red(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t green = (Strip.Green(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t blue = (Strip.Blue(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint8_t white = (Strip.White(Strip.getPixelColor(j)) * (trSteps - i)) / trSteps;
            uint32_t color = Strip.dimColor(Strip.Color(red, green, blue, white), CMD.cmdArr[6]);

            Strip.setPixelColor(j, color);          
          }
          Strip.show();
          delay(40);
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
  switch (CMD.cmdArr[1]) {
    case 0:
      {
        BRK.dirFaded = true;
        BRK.dirSet = BRK.fadeDir;
        BRK.fadeDir = 0;
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
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(180, 255)) * randBrightnessTo); // red
          PTRN.fadeToArr[i][1] = int(double(random(20, 75)) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = 0; // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 5:
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
          PTRN.fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first


        break;
    } case 6:
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
          PTRN.fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
          PTRN.fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 7:
      {
        PTRN.waveCount++;
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = int(double(Strip.Red(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); //red
          PTRN.fadeToArr[i][1] = int(double(Strip.Green(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
          PTRN.fadeToArr[i][2] = int(double(Strip.Blue(Strip.Wheel((PTRN.waveCount * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // blue

        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 8:
      {
        for (int i = 0; i < pts + 1; i++)
        {
          PTRN.fadeFromArr[i][0] = PTRN.fadeToArr[i][0];
          PTRN.fadeFromArr[i][1] = PTRN.fadeToArr[i][1];
          PTRN.fadeFromArr[i][2] = PTRN.fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(50, 100)) / 100.0;

          PTRN.fadeToArr[i][0] = min(int(double(CMD.cmdArr[2] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[2]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // red
          PTRN.fadeToArr[i][1] = min(int(double(CMD.cmdArr[3] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[3]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255); // green
          PTRN.fadeToArr[i][2] = min(int(double(CMD.cmdArr[4] + random(-75, 75) * (pow(2.0 * (double(CMD.cmdArr[4]) / 180.0 - 0.4), 2)) + 0.1) * pow(randBrightnessTo, 1.5)), 255);  // blue
        
          PTRN.fadeToArr[i][0] = max(PTRN.fadeToArr[i][0], 0);
          PTRN.fadeToArr[i][1] = max(PTRN.fadeToArr[i][1], 0);
          PTRN.fadeToArr[i][2] = max(PTRN.fadeToArr[i][2], 0);
        }
        PTRN.fadeToArr[pts][0] = PTRN.fadeToArr[0][0]; // red is same as first
        PTRN.fadeToArr[pts][1] = PTRN.fadeToArr[0][1]; // green is same as first
        PTRN.fadeToArr[pts][2] = PTRN.fadeToArr[0][2]; // blue is same as first

        break;
    } case 9:
      {
        Strip.ColorSet(Strip.Color(0, 0, 0));
        Strip.checkBraking();
        Strip.show();
        for (int i = 0; i < numFiFl; i++) {
          PTRN.stripArr[i][0] = random(0, stripLength);
        }
        for (int i = 0; i < numFiFl; i++) {  // at the end of the last step, start the next cycle
          PTRN.stripArr[i][1] = random(0, 3);
          if (PTRN.stripArr[i][1] == 0) {
            Strip.setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.dimColor(Strip.dimColor(Strip.Color(66, 74, 19), random(0, 10)), CMD.cmdArr[6]));
            Strip.setPixelFiltered(PTRN.stripArr[i][0], Strip.dimColor(Strip.dimColor(Strip.getPixelColor((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1)), 30), CMD.cmdArr[6]));
            Strip.setPixelFiltered((PTRN.stripArr[i][0] + 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelFiltered((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            PTRN.stripArr[i][0] = (PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1);
          } else if (PTRN.stripArr[i][1] == 1) {
            Strip.setPixelFiltered((PTRN.stripArr[i][0] - 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelFiltered(PTRN.stripArr[i][0], Strip.dimColor(Strip.dimColor(Strip.Color(66, 74, 19), random(0, 10)), CMD.cmdArr[6]));
            Strip.setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
          } else if (PTRN.stripArr[i][1] == 2) {
            Strip.setPixelFiltered((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1), Strip.dimColor(Strip.dimColor(Strip.Color(66, 74, 19), random(0, 10)), CMD.cmdArr[6]));
            Strip.setPixelFiltered(PTRN.stripArr[i][0], Strip.dimColor(Strip.dimColor(Strip.getPixelColor((PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1)), 30), CMD.cmdArr[6]));
            Strip.setPixelFiltered((PTRN.stripArr[i][0] - 2) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            Strip.setPixelFiltered((PTRN.stripArr[i][0] + 1) % (int(stripLength) + 1), Strip.Color(0, 0, 0));
            PTRN.stripArr[i][0] = (PTRN.stripArr[i][0] - 1) % (int(stripLength) + 1);
          }
        }
        Strip.TotalSteps = random(10, 50);           // pick number of steps for the fireflies
        break;
    } case 13:
      {
        BRK.dirFaded = true;
        BRK.dirSet = BRK.fadeDir;
        BRK.fadeDir = 0;
        break;
      }
      default:
      break;
  }


  PTRN.cometWheel = random(0, 255);     // prepare next comet
  PTRN.cometDir = random(0, 2);
  PTRN.cometStrt = random(0, stripLength);
}


// 7. ============================================================================ GETTING UART & WIFI DATA=======================================================================================================
// Get and interpret UART data 
void getVescData() {
  if (millis() - lastVescUpdate  > 50){
    lastVescUpdate = millis();

    double rpm;
    double amph;
    double tach;

    if (BLEPause){
      if (vesc.getVescValues() && vesc.getFloatValues()) {
        vDATA.motorCurrent = vesc.data.avgMotorCurrent;
        rpm = vesc.data.rpm;
        vDATA.volt = vesc.data.inpVoltage;
        amph = vesc.data.ampHours;
        tach = vesc.data.tachometerAbs;
        vDATA.duty = vesc.data.dutyCycleNow;

        vDATA.pitch = vesc.floatData.truePitch;
        vDATA.adc1 = vesc.floatData.adc1;
        vDATA.adc2 = vesc.floatData.adc2;

        if(rpm > max(BRK.idleThresholdRpm, 1.0)) {                              // sets direction state
          vDATA.vescDir = 1;
        } else if(rpm < -max(BRK.idleThresholdRpm, 1.0)) {
          vDATA.vescDir = -1;
        } else {
          vDATA.vescDir = 0;
        }
        
      } else {
        return;                                                            // dont change anything
      }
    } else if (simulateRpmData){                                           // for debug / demo
      rpm = 11460.0 * sin(double(millis())/10000.0) + 5000.0 * sin(double(millis())/1000.0);                       // Feeds wavy sinewave data to rpm for testing lights reacting to speed data functions

       if(rpm > BRK.idleThresholdRpm) {
          vDATA.vescDir = 1;
        } else if(rpm < -BRK.idleThresholdRpm) {
          vDATA.vescDir = -1;
        } else {
          vDATA.vescDir = 0;
        }
    }

    vDATA.rpmHist[5] = vDATA.rpmHist[4];                                               // Running list of past 6 rpm values, ~50ms between, index 0 most recent
    vDATA.rpmHist[4] = vDATA.rpmHist[3];
    vDATA.rpmHist[3] = vDATA.rpmHist[2];
    vDATA.rpmHist[2] = vDATA.rpmHist[1];
    vDATA.rpmHist[1] = vDATA.rpmHist[0];
    vDATA.rpmHist[0] = rpm;

    if(vDATA.motorCurrent < -10.0 - 10.0 * (1.0 - brakingSens)){                // Negative motor current means braking
      vDATA.braking = true;
    } else{
      vDATA.braking = false;
    }
  }
}


// Reads Web Command State and Forwards it to LED Code
void handleWiFiUpdates(){ 
   // compare the previous status to the current status
  if (status != WiFi.status()) {
    // it has changed update the variable
    status = WiFi.status();
  }


  // What action to perform
  // position in sequences array for selected sequence
  static int mode = CMD.cmdArr[1];
  // delay (speed) in ms (default 1 second)
  static int brightness = CMD.cmdArr[6];
  // Whether direction reversed
  static bool OnOff = CMD.cmdArr[0];

  static bool ota = false;
  static bool ble = false;
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


  if (millis() - lastWebmillis > 250){
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
                  else if (arg_name == "ota"){
                    if (arg_value == "0") ota = 0;
                    else if (arg_value == "1") ota = 1;
                  }
                  else if (arg_name == "ble"){
                    if (arg_value == "0") ble = 0;
                    else if (arg_value == "1") ble = 1;
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

    updateCommands(OnOff, mode, CMD.usrColor[0], CMD.usrColor[1], CMD.usrColor[2], brightness, ota, ble);
  }
}

// Gets Web Command State and comapres it to current LED state. Updates LED state if they disagree.
void updateCommands(bool reqOnOff, int reqMode, int reqRed, int reqGreen, int reqBlue, int reqBrightness, int OTA, int BLE){
  if (CMD.cmdArr[0] != reqOnOff || CMD.cmdArr[1] != reqMode || CMD.cmdArr[2] != reqRed || CMD.cmdArr[3] != reqGreen || CMD.cmdArr[4] != reqBlue){
    CMD.cmdArr[0] = reqOnOff;
    CMD.cmdArr[1] = reqMode;
    CMD.cmdArr[2] = reqRed;
    CMD.cmdArr[3] = reqGreen;
    CMD.cmdArr[4] = reqBlue;
    Serial.println(CMD.cmdArr[1]);
    ControlPanel();
  }
  if (CMD.cmdArr[6] != reqBrightness){
    CMD.cmdArr[6] = reqBrightness;
  }
  if (OTAState != OTA){
    OTAState = OTA;
    setup();
  }
  if (UARTBle != BLE){
    UARTBle = BLE;
    setup();
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

  CMD.usrColor[0] = r;
  CMD.usrColor[1] = g;
  CMD.usrColor[2] = b;

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

void BLEBridge(){
  if (Serial0.available())
  {
    int oneByte;
    while (Serial0.available())
    {
      oneByte = Serial0.read();
      vescBuffer.push_back(oneByte);
    }

    if (deviceConnected)
    {
      while (vescBuffer.length() > 0)
      {
        if (vescBuffer.length() > PACKET_SIZE)
        {
          dumpBuffer("VESC => BLE/UART", vescBuffer.substr(0, PACKET_SIZE));
          pCharacteristicVescTx->setValue(vescBuffer.substr(0, PACKET_SIZE));
          vescBuffer = vescBuffer.substr(PACKET_SIZE);
        }
        else
        {
          dumpBuffer("VESC => BLE/UART", vescBuffer);
          pCharacteristicVescTx->setValue(vescBuffer);
          vescBuffer.clear();
        }
        pCharacteristicVescTx->notify();
        delay(5); // bluetooth stack will go into congestion, if too many packets are sent
      }
    }
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    ESP_LOGI(LOG_TAG_BLESERVER, "start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}

// ==================================================================

void UpdateButton(){
  if(fabs(vDATA.rpmHist[0]) > 0){
    Button.setPixelColor(0, Strip.getPixelColor(0));
  } else {
    Button.setPixelColor(0, Button.Color(0, 255 * (sin(millis() / 800.0) * 0.4 + 0.6), 255 * (sin(millis() / 800.0) * 0.4 + 0.6)));
  }
  Button.show();
}

// 8. ==================================================================== ARDUINO MAIN LOOP ========================================================================================

void setup() {
  if(OTAState && enableOTA){
    Serial.flush();
    Serial.begin(115200);
    Serial.println("Booting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(OTAssid, OTApassword);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }

    // Port defaults to 3232
    // ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    // ArduinoOTA.setHostname("myesp32");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

    ArduinoOTA.begin();

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  else {
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);

    //boot up command states
    CMD.cmdArr[0] = 1;            // 0 - off, 1 - on
    CMD.cmdArr[1] = bootMode;            // modes 0 - 13
    CMD.cmdArr[2] = bootRed;          // red
    CMD.cmdArr[3] = bootGreen;          // green
    CMD.cmdArr[4] = bootBlue;          // blue
    CMD.cmdArr[6] = bootBrightness;           // brightness slider value

    CMD.usrColor[0] = CMD.cmdArr[2];
    CMD.usrColor[1] = CMD.cmdArr[3];  // initalize user set color
    CMD.usrColor[2] = CMD.cmdArr[4];  

    Serial.begin(9600);       

    // print the network name (SSID);
    Serial.print("Creating access point named: ");
    Serial.println(ssid);

    // Create open network. Change this line if you want to create an WEP network:
    if (!WiFi.softAP(ssid, pass)) {
      log_e("Soft AP creation failed.");
      while(1);
    }
    IPAddress myIP = WiFi.softAPIP();
    
    // wait 100 ms for connection:
    delay(100);

    server.begin();                           // start the web server on port 80
    if (DEBUG > 0) printWifiStatus();         // you're connected now, so print out the status

    if (UARTBle == 0){
      BLEPause = enableUART;
      Serial0.begin(115200);                    // VESC UART data
      vesc.setSerialPort(&Serial0);
    } else {
      BLEPause = false;
      Serial0.flush();
      Serial0.begin(115200); // RX=20, TX=21

      // Create the BLE Device
      NimBLEDevice::init("Jovan'sBLEBridge");
      NimBLEDevice::setPower(ESP_PWR_LVL_P9);

      // Create the BLE Server
      pServer = NimBLEDevice::createServer();
      pServer->setCallbacks(new MyServerCallbacks());
      auto pSecurity = new NimBLESecurity();
      pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

      // Create the BLE Service
      BLEService *pService = pServer->createService(VESC_SERVICE_UUID);

      // Create a BLE TX Characteristic
      pCharacteristicVescTx = pService->createCharacteristic(
          VESC_CHARACTERISTIC_UUID_TX,
          NIMBLE_PROPERTY::NOTIFY |
              NIMBLE_PROPERTY::READ);

      // Create a BLE RX Characteristic
      pCharacteristicVescRx = pService->createCharacteristic(
          VESC_CHARACTERISTIC_UUID_RX,
          NIMBLE_PROPERTY::WRITE |
              NIMBLE_PROPERTY::WRITE_NR);

      pCharacteristicVescRx->setCallbacks(new MyCallbacks());

      // Start the VESC service
      pService->start();

      // Start advertising
      NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
      pAdvertising->addServiceUUID(VESC_SERVICE_UUID);
      //    pAdvertising->setAppearance(0x00);
      //    pAdvertising->setScanResponse(true);
      //    pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter

      pAdvertising->start();
      ESP_LOGI(LOG_TAG_BLESERVER, "waiting a client connection to notify...");
    }

    // initialize pixels
    Strip.Color1 = Strip.Color(bootRed, bootGreen, bootBlue);
    Strip.Color2 = Strip.Wheel(random(255));
    Strip.begin();
    Strip.setBrightness(master_brightness);   // Set hard brightness limiter
    ControlPanel();
    Strip.show();

    Button.begin();
    Button.setBrightness(255);
    Button.show();
  }
}



void loop() {
  if(enableOTA && OTAState == 1){
    ArduinoOTA.handle();
  }

  else { 
    handleWiFiUpdates();
    if(UARTBle){
      BLEBridge();
    } else{
      getVescData();
    }
    Strip.Update();
    
    UpdateButton();
  }
}

