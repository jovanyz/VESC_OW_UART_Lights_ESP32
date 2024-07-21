// General Inputs
const char* ssid = "Jovan'sVESCR";                              // network SSID (name)
const char* pass = "vescisbetter";                          // network password (use for WPA, or use as key for WEP)

const char* OTAssid = "Oasis";
const char* OTApassword = "Mar1sistheb3st!";

const bool enableOTA = true;
const bool enableUART = true;                         // set "true" if VESC is connected to UART. Makes strip laggy if not configured.

const int datPinNum = 21;                             // GPIO pin connected to LED data line D10 on ESP Nano
const int buttonPin = 38;
const int master_brightness = 35;                     // Hard brightness limiter, (0-255). Independent of slider. Choose wisely! Make sure you have enough amps!

const double stripLength = 40;                         // number of LEDs 
const int breakPoint = 20;                             // index of first tail-light pixel

const double sCells = 18;
const bool enableBMeter = true;

// Mode Inputs
const double numRainbow = 1.5;                         // number of full rainbows distributed in stripLength
const int seed = 0;                                    // starting pixel of Colorwipe Pattern
const double divBy = 8.0;                             // choose a number ideally between 8 and 15
const double modif = 40;                               // From number of LEDs, find the closest larger number with the divisor: divBy.
const int pts = 5;                                     // pts = modif / divBy. Creates guidepoints for wave pattern
const int numFiFl = 15;                                // number of "fireflies" added 8-20 are good values
const int numConf = 6;                                 // number of colored speckles added for "Confetti" 4-10 are good values

// VESC Behavior Inputs
const int idleBrightness = 15;                         // brightness percent when at idle (0 to 100). Dims lights when not riding
const double idleThresholdMph = 2.5;                   // mph threshold to escape idle mode. 
const double brakingSens = 0.25;                       // 0 to 1, brakelight sensitivity, 1 is sensitive, 0 is sluggish
const bool brakeBlink = true;                          // quick blink animation when brakelights come on
const int brakeBlink_ms = 50;                          // milliseconds. How fast blink occurs

// Startup Inputs
const int bootMode = 8;                                // 0 to 12, wihich mode to start initlaize after OW power on
const int bootRed = 255;                               // 0-255 Rgb
const int bootGreen = 255;                             // 0-255 rGb
const int bootBlue = 255;                              // 0-255 rgB
const int bootBrightness = 100;                         // 0-100 inital brightness percent

// Debugand Demo Settings
const bool simulateRpmData = false;                     // feed sine wave data into rpmHist
const bool disableBrakingResponse = false;              // leave false. for debug/  testing 
