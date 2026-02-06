#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "Logger.h"
#include "Panel.h"
#include "MatrixDriver.h"
#include "GameLifeMatrix.h"
#include "GameLifeMatrix2.h"
#include "PlasmaMatrix.h"
#include "GY21Sensor.h"
#include "InputHandler.h"
#include "MODES.h"
#include "OTAHandler.h"

#include "fonts/Roboto_Black_22.h"
#include "fonts/Led_Matrix_Font_5x3.h"

// LOGGING MASTER TOGGLE //
#define ENABLE_LOGGING true
///////////////////////////

#define WIFI_SSID "PLUSNET-PSQZ"
#define WIFI_PASSWORD "d67f7e27f4"
#define WIFI_IP_GATEWAY

#define GY21_SDA 8 //  Data I2C connection to GY-21 module
#define GY21_SCL 9 //  Clock I2C connection to GY-21 module
#define RGB_PIN 48 // Onboard RGB LED pin

#define LDR_PIN 2       // Pin for LDR input for ambient light sensing. ADC pin
#define BRIGHT_ENC_A 40 // Encoder pins for brightness control
#define BRIGHT_ENC_B 41
#define BRIGHT_ENC_SW 42
#define COLOR_ENC_A 38  // 39 // Encoder pins for color hue control
#define COLOR_ENC_B 39  // 38
#define COLOR_ENC_SW 19 // GPIO37 giving ramped pulse output signal - use GPIO19 instead

#define TEMPERATURE_FONT Roboto_Black_22
#define HUMIDITY_FONT Led_Matrix_Font_5x3
#define TEMPERATURE_COLOR_DEFAULT 0xFFFF // default text colour
#define HUMIDITY_COLOR_DEFAULT 0xFFFF    // default text colour
#define COLOURED_TEXT_SATURATION 180     // Saturation for coloured text

#define POLLING_INTERVAL_MS 50 // Input polling interval in milliseconds
#define SWITCH_DEBOUNCE_MS 150 // Switch debounce time in milliseconds

// onboard RGB LED object
Adafruit_NeoPixel pixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

GameLifeMatrix *gameLifeMatrix;   // Game of Life matrix of cells
GameLifeMatrix2 *gameLifeMatrix2; // Game of Life matrix of cells with different rules and palettes
PlasmaMatrix *plasmaMatrix;       // Plamsa matrix of cells
Matrix *currentMatrix;            // Polymorphic pointer to current matrix

Panel *panel;               // LED matrix panel
MatrixDriver *matrixDriver; // Driver to update panel from matrix
GY21Sensor *gy21Sensor;     // Temperature and humidity sensor
InputHandler *inputHandler; // Input handler for brightness, hue, modes
OTAHandler *otaHandler;     // OTA update handler

const int textOnlyFPS = 10; // desired frames per second
const int gameLifeFPS = 15; // desired frames per second
const int plasmaFPS = 40;   // desired frames per second
const int mainLoopFPS = 40; // desired main loop FPS

void setNewDisplayMode();
void delayForFPS();

// TESTING functions //////////////////////////////
void testSweepOnboardLED();
void testLogValues(bool valueChanged);
void rgbLedMirrorsColour();
///////////////////////////////////////////////////

bool panelEnabled = true;
uint8_t brightness = 255;               // brightness level
uint16_t backHue = 32768;               // hue value for background
int palletteIndex = 0;                  // current palette inddex for matrices that support multiple palettes
int displayMode = MODES::GAME_AND_TEXT; // mode
int mode2 = MODES::MODE2_A;             // secondary mode
int textWhiteOnly = true;               // whether text is white only or coloured based on hue
uint16_t textHue = backHue;             // hue value for text

void setup()
{
  Logger::begin(115200);
  delay(200);

  // master toggle for logging - set to false to disable all logging
  Logger::enableOutput(ENABLE_LOGGING);
  Logger::println("Starting LED Matrix Demo (v2) ");

  pixel.begin();
  pixel.setBrightness(0);

  otaHandler = new OTAHandler(WIFI_SSID, WIFI_PASSWORD,
                              "LEDMATRIXBOX", // hostname for mDNS
                              30000);         // try to reconnect every 30 seconds after disconnect

  gameLifeMatrix = new GameLifeMatrix(45, true); // 45% initial density, edge wrap enabled
  Logger::println("Game of Life Matrix initialized");

  plasmaMatrix = new PlasmaMatrix();
  Logger::println("Plasma Matrix initialized");

  gameLifeMatrix2 = new GameLifeMatrix2(45, true); // 45% initial density, edge wrap enabled
  Logger::println("Game of Life Matrix 2 initialized");

  // set initial matrix
  currentMatrix = gameLifeMatrix;

  panel = new Panel(brightness, true); // brightness 0-255, double buffering enabled
  Logger::println("Panel initialized");

  gy21Sensor = new GY21Sensor(GY21_SDA, GY21_SCL, 1000); // update every 1 second
  Logger::println("GY21Sensor initialized");

  inputHandler = new InputHandler(POLLING_INTERVAL_MS,
                                  BRIGHT_ENC_A, BRIGHT_ENC_B, BRIGHT_ENC_SW,
                                  COLOR_ENC_A, COLOR_ENC_B, COLOR_ENC_SW,
                                  LDR_PIN,
                                  0, 255,                               // min/max brightness
                                  0, 65535,                             // min/max hue
                                  10,                                   // glitch filter time microS
                                  SWITCH_DEBOUNCE_MS,                   // switch debounce time MS
                                  MODES::GAME_AND_TEXT, MODES::MODE2_A, // staring modes
                                  255, 32768);                          // starting brightness and hue

  Logger::println("InputHandler initialized");

  // create MatrixDriver to update panel from matrix at FPS
  matrixDriver = new MatrixDriver(gameLifeFPS, panel, currentMatrix, gy21Sensor,
                                  &TEMPERATURE_FONT, &HUMIDITY_FONT,
                                  TEMPERATURE_COLOR_DEFAULT, HUMIDITY_COLOR_DEFAULT);
  // adjust text positions and offsets for better visual centering
  matrixDriver->setTemperatureTextXOffset(-1);
  matrixDriver->setTemperatureTextYOffset(0);
  matrixDriver->setHumidityTextXOffset(10);
  matrixDriver->setHumidityTextYOffset(12);
  Logger::println("MatrixDriver initialized");

  delay(100);

  // start the sensor update task
  gy21Sensor->resume();
  Logger::println("GY21Sensor resumed");

  // Once everything ready, start the display update task
  matrixDriver->resume();
  Logger::println("MatrixDriver resumed");

  // now resume input handler polling
  inputHandler->resume();
  Logger::println("InputHandler resumed");
}

void loop()
{
  uint8_t tempBrightness;
  uint16_t tempHue;
  int tempDisplayMode;
  int tempMode2;
  bool tempLDREnable;

  bool valueChanged = false; // for logging only

  // check for OTA updates
  otaHandler->handle();

  // read inputs
  inputHandler->getState(tempBrightness, tempHue, tempDisplayMode, tempMode2, tempLDREnable);

  // if panel enabled state changed by ldr, pause/resume matrix driver
  if (panelEnabled != tempLDREnable)
  {
    panelEnabled = tempLDREnable;
    if (panelEnabled) // resuming panel
    {
      // restore panel brightness and resume the driver
      matrixDriver->resume();
    }
    else // pausing panel
    {
      // set panel brightness to 0 and pause the driver
      matrixDriver->pause();
      valueChanged = true;
      // skip rest of loop
      return;
    }
    valueChanged = true;
  }

  // apply brightness if changed
  if (tempBrightness != brightness)
  {
    brightness = tempBrightness;
    matrixDriver->setPanelBrightness(brightness);
    valueChanged = true;
  }

  // apply new hue if changed - MODE-DEPENDENT
  // the input hue is reset to textHue or hue
  // InputHandler hue changes always update text hue in TEXT_ONLY mode, but in other modes they update main hue
  // TEXT_MODE: text colours can only be set in TEXT_ONLY mode, they are preserved after leaving TEXT_ONLY mode.
  if (displayMode == MODES::TEXT_ONLY)
  {
    if (textWhiteOnly)
    {
      matrixDriver->setTemperatureFontColor(TEMPERATURE_COLOR_DEFAULT);
      matrixDriver->setHumidityFontColor(HUMIDITY_COLOR_DEFAULT);
    }
    else
    {
      if (tempHue != textHue)
      {
        textHue = tempHue;
        matrixDriver->setTemperatureFontColor(currentMatrix->hsvTo565(textHue, COLOURED_TEXT_SATURATION, 255));
        matrixDriver->setHumidityFontColor(currentMatrix->hsvTo565(textHue, COLOURED_TEXT_SATURATION, 255));
        valueChanged = true;
      }
    }
  }
  else
  {
    if (tempHue != backHue)
    {
      // for all other modes we set the new hue and/or cycle to next palette
      // Note that these do not have to be implemented by each matrix, they can do nothing if needed
      // these changes only take effect at the start of the next drawing frame in the driver task
      backHue = tempHue;
      matrixDriver->setHue(backHue);
      matrixDriver->nextPalette();
      valueChanged = true;
    }
  }

  // apply new display mode if changed
  // cycle through display modes
  if (tempDisplayMode != displayMode)
  {
    displayMode = tempDisplayMode;
    setNewDisplayMode();
    valueChanged = true;
  }

  // apply new secondary mode if changed. in this case it's a toggle
  // In TEXT_ONLY mode we toggle text colour/white only mode
  // In other modes we toggle cycling on/off for palette or hue changes
  if (tempMode2 != mode2)
  {
    mode2 = tempMode2;

    // in TEXT_ONLY mode we can toggle between white and coloured text
    if (displayMode == MODES::TEXT_ONLY)
    {
      textWhiteOnly = !textWhiteOnly; // toggle text colour/white only mode
      // if moving to coloured mode from white mode then reset input's stored hue to last text hue
      if (!textWhiteOnly)
      {
        inputHandler->setHue(textHue);
      }
    }
    else // in other modes we toggle cycling of palettes/hues
    {
      matrixDriver->toggleCycling();
    }

    valueChanged = true;
  }

  // delay to maintain desired main loop FPS - not critical timing
  delayForFPS();

  // TESTING FUNCTIONS:
  testLogValues(valueChanged);
  // testSweepOnboardLED();
  rgbLedMirrorsColour();
}

// set new display mode based on current input handler mode
void setNewDisplayMode()
{
  switch (displayMode)
  {
  case MODES::TEXT_ONLY:
    inputHandler->setHue(textHue);          // set input handler to monitor text hue
    currentMatrix = gameLifeMatrix;         // do this now for when we switch again so
    matrixDriver->setMatrix(currentMatrix); // we have valid gamelife matrix AS SOON AS
    currentMatrix->setBackgroundMode(true); // mode is switched next time
    matrixDriver->setFPS(textOnlyFPS);
    matrixDriver->enableBackgroundDrawing(false);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::GAME_AND_TEXT:
    inputHandler->setHue(backHue); // set input handler to monitor main hue for other modes
    currentMatrix = gameLifeMatrix;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(true);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::GAME2_AND_TEXT:
    currentMatrix = gameLifeMatrix2;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(true);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::PLASMA_AND_TEXT:
    currentMatrix = plasmaMatrix;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(true);
    matrixDriver->setFPS(plasmaFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::GAME_ONLY:
    currentMatrix = gameLifeMatrix;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(false);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(false);
    break;
  case MODES::GAME2_ONLY:
    currentMatrix = gameLifeMatrix2;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(false);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(false);
    break;
  case MODES::PLASMA_ONLY:
    currentMatrix = plasmaMatrix;
    matrixDriver->setMatrix(currentMatrix);
    currentMatrix->setBackgroundMode(false);
    matrixDriver->setFPS(plasmaFPS);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(false);
    break;
  default:
    Logger::println("Unknown mode selected!");
    break;
  }
}

// delay to maintain desired main loop FPS (approximate timing)
void delayForFPS()
{
  // main loop timing to maintain desired FPS
  static unsigned long lastLoopTime = 0;
  unsigned long currentLoopTime = millis();
  int delayTime = (1000 / mainLoopFPS) - (currentLoopTime - lastLoopTime);
  if (delayTime > 0)
    delay(delayTime);
  else
    delay(1); // yield to allow other tasks to run
  lastLoopTime = currentLoopTime;
}

// TESTING :: log salient values if changed to serial monitor
void testLogValues(bool valueChanged)
{
  // TESTING ///////////////////////////////////////////////////////
  static const char *modeNames[MODES::TOTAL_MODES] = {
      "TEXT_ONLY",
      "GAME_AND_TEXT",
      "GAME2_AND_TEXT",
      "PLASMA_AND_TEXT",
      "GAME_ONLY",
      "GAME2_ONLY",
      "PLASMA_ONLY"};
  if (valueChanged)
  {
    Logger::printf("Panel Enabled: %s\n", panelEnabled ? "Yes" : "No");
    Logger::printf("Brightness Level: %d\n", brightness);
    Logger::printf("Current Mode: %d: %s\n", displayMode, modeNames[displayMode]);
    Logger::printf("Current Hue: %d\n", backHue);
    Logger::printf("Secondary Mode: %d\n", mode2);
    Logger::printf("Cycling: %s\n", matrixDriver->isCycling() ? "Yes" : "No");
    Logger::printf("Text Colour Mode: %s\n", textWhiteOnly ? "White Only" : "Coloured");
    Logger::printf("Text Hue: %d\n", textHue);
  }
  // every second display ldr adc value for testing
  static unsigned long lastLDRLogTime = 0;
  if (millis() - lastLDRLogTime >= 10000)
  {
    lastLDRLogTime = millis();
    Logger::printf("Current LDR ADC Value: %d\n", inputHandler->getCurrentLDRValue());
  }
}

// set onboard RGB LED to mirror text mode colour
void rgbLedMirrorsColour()
{
  static uint16_t rgbLedHue = 0;  // 0-65535
  static uint8_t rgbLedSat = 255; // 0-255
  static uint8_t rgbLedVal = 255; // 0-255

  // Show RGB led if panelEnabled. and set colour to current pallet hue for testing. otherwise turn off
  if (panelEnabled)
  {
    // Convert HSV to RGB and set LED color
    uint32_t color = pixel.gamma32(pixel.ColorHSV(backHue, rgbLedSat, rgbLedVal));
    pixel.setPixelColor(0, color);
    pixel.setBrightness(3); // Optional: adjust brightness dynamically
    pixel.show();
  }
  else
  {
    // Turn off the LED if the panel is not enabled
    pixel.setPixelColor(0, 0);
    pixel.show();
  }
}

// simple test function to sweep colors on the onboard RGB LED. shows signs of life
void testSweepOnboardLED()
{
  // Simple HSV rainbow cycle on the onboard RGB LED to show signs of life
  static uint16_t hsvHue = 0;  // 0-65535
  static uint8_t hsvSat = 255; // 0-255
  static uint8_t hsvVal = 100; // 0-255

  // Only show LED if panel is enabled
  if (panelEnabled)
  {
    // Convert HSV to RGB and set LED color
    uint32_t color = pixel.gamma32(pixel.ColorHSV(hsvHue, hsvSat, hsvVal));
    pixel.setPixelColor(0, color);
    pixel.show();
    hsvHue += 16; // Smooth step (smaller = slower transition)
  }
  else
  {
    // Turn off LED when panel is disabled
    pixel.setPixelColor(0, 0); // Off
    pixel.show();
  }
}
