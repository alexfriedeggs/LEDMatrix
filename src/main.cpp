#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "Logger.h"
#include "Panel.h"
#include "MatrixDriver.h"
#include "GameLifeMatrix.h"
#include "PlasmaMatrix.h"
#include "GY21Sensor.h"
#include "InputHandler.h"
#include "MODES.h"

#include "fonts/Roboto_Black_22.h"
#include "fonts/Led_Matrix_Font_5x3.h"

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
#define TEMPERATURE_COLOR_DEFAULT 0xFFFF // Bright white
#define HUMIDITY_COLOR_DEFAULT 0xFFFF    // Bright white
#define COLOURED_TEXT_SATURATION 100     // Saturation for coloured text

#define POLLING_INTERVAL_MS 50 // Input polling interval in milliseconds
#define SWITCH_DEBOUNCE_MS 150 // Switch debounce time in milliseconds

// onboard RGB LED object
Adafruit_NeoPixel pixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

GameLifeMatrix *gameLifeMatrix; // Game of Life matrix of cells
PlasmaMatrix *plasmaMatrix;     // Plamsa matrix of cells
Matrix *currentMatrix;          // Polymorphic pointer to current matrix

Panel *panel;               // LED matrix panel
MatrixDriver *matrixDriver; // Driver to update panel from matrix
GY21Sensor *gy21Sensor;     // Temperature and humidity sensor
InputHandler *inputHandler; // Input handler for brightness control

const int textOnlyFPS = 10; // desired frames per second
const int gameLifeFPS = 15; // desired frames per second
const int plasmaFPS = 40;   // desired frames per second
const int mainLoopFPS = 40; // desired main loop FPS

void setNewMode();
void testSweepOnboardLED();
void delayForFPS();

bool panelEnabled = true;
uint8_t brightness = 255;               // Default brightness level
uint16_t hue = 32768;                   // Default hue value
int displayMode = MODES::GAME_AND_TEXT; // Default mode
int textMode = MODES::TEXT_MODE_WHITE;  // Default text colour mode

void setup()
{
  Logger::begin(115200);
  delay(200);
  Logger::println("Starting LED Matrix Demo (v2) ");
  pixel.begin();
  pixel.setBrightness(0);

  gameLifeMatrix = new GameLifeMatrix(45, true); // 45% initial density, edge wrap enabled
  Logger::println("Game of Life Matrix initialized");

  plasmaMatrix = new PlasmaMatrix();
  Logger::println("Plasma Matrix initialized");

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
                                  0, 255,                                       // min/max brightness
                                  0, 65535,                                     // min/max hue
                                  10,                                           // glitch filter time microS
                                  SWITCH_DEBOUNCE_MS,                           // switch debounce time MS
                                  MODES::GAME_AND_TEXT, MODES::TEXT_MODE_WHITE, // staring modes
                                  255, 32768);                                  // starting brightness and text hue

  Logger::println("InputHandler initialized");

  // create MatrixDriver to update panel from matrix at FPS
  matrixDriver = new MatrixDriver(gameLifeFPS, panel, currentMatrix, gy21Sensor, &TEMPERATURE_FONT, &HUMIDITY_FONT,
                                  TEMPERATURE_COLOR_DEFAULT, HUMIDITY_COLOR_DEFAULT);
  // adjust text positions and offsets for better visual centering
  matrixDriver->setTemperatureTextXOffset(-1);
  matrixDriver->setTemperatureTextYOffset(1);
  matrixDriver->setHumidityTextXOffset(10);
  matrixDriver->setHumidityTextYOffset(13);
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
  // testSweepOnboardLED();

  uint8_t tempBrightness;
  uint16_t tempHue;
  int tempDisplayMode;
  int tempTextMode;
  bool tempLDREnable;

  static uint16_t rgbLedHue = 0;  // 0-65535
  static uint8_t rgbLedSat = 255; // 0-255
  static uint8_t rgbLedVal = 255; // 0-255

  bool valueChanged = false; // for logging only

  // read inputs
  inputHandler->getState(tempBrightness, tempHue, tempDisplayMode, tempTextMode, tempLDREnable);

  // apply brightness if changed
  if (tempBrightness != brightness) // brightness changed
  {
    brightness = tempBrightness;
    matrixDriver->setPanelBrightness(brightness);
    valueChanged = true;
  }

  // apply new display mode if changed
  if (tempDisplayMode != displayMode) // new mode selected
  {
    displayMode = tempDisplayMode;
    setNewMode();
    valueChanged = true;
  }

  // if panel enabled state changed, pause/resume matrix driver
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
    }
    valueChanged = true;
  }

  // TESTING ///////////////////////////////////////////////////////
  static const char *modeNames[MODES::TOTAL_MODES] = {
      "TEXT_ONLY",
      "GAME_AND_TEXT",
      "PLASMA_AND_TEXT",
      "GAME_ONLY",
      "PLASMA_ONLY"};
  if (valueChanged)
  {
    Logger::printf("Brightness Level: %d\n", brightness);
    Logger::printf("Panel Enabled: %s\n", panelEnabled ? "Yes" : "No");
    Logger::printf("Current Mode: %d: %s\n", displayMode, modeNames[displayMode]);
    Logger::printf("Current Hue: %d\n", hue);
    Logger::printf("Text Mode: %d\n", textMode);
  }
  // every second display ldr adc value for testing
  static unsigned long lastLDRLogTime = 0;
  if (millis() - lastLDRLogTime >= 1000)
  {
    lastLDRLogTime = millis();
    Logger::printf("Current LDR ADC Value: %d\n", inputHandler->getCurrentLDRValue());
  }

  /////////////////////////////////////////////////////////////////

  // change text colour if needed
  // Show RGB led uf panelEnabled. and set colour to current hue if mode enabled
  if (panelEnabled)
  {
    // update rgbLedHue value for onboard LED
    rgbLedHue = tempHue;
    if (tempTextMode == MODES::TEXT_MODE_WHITE)
    {
      // text white mode
      matrixDriver->setTemperatureFontColor(0xFFFF); // white
      matrixDriver->setHumidityFontColor(0xFFFF);    // white

      rgbLedSat = 0; // Onboard RGB LED white (no saturation but full brightness)
    }
    else
    {
      // text colour mode
      matrixDriver->setTemperatureFontColor(pixel.ColorHSV(tempHue, COLOURED_TEXT_SATURATION, 255));
      matrixDriver->setHumidityFontColor(pixel.ColorHSV(tempHue, COLOURED_TEXT_SATURATION, 255));

      rgbLedSat = 255; // Onboard RGB LED full colour
    }
    // Convert HSV to RGB and set LED color
    uint32_t color = pixel.gamma32(pixel.ColorHSV(rgbLedHue, rgbLedSat, rgbLedVal));
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

  delayForFPS();
}

// set new mode based on current input handler mode
void setNewMode()
{
  switch (displayMode)
  {
  case MODES::TEXT_ONLY:
    matrixDriver->setFPS(textOnlyFPS);
    matrixDriver->enableBackgroundDrawing(false);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::GAME_AND_TEXT:
    currentMatrix = gameLifeMatrix;
    currentMatrix->setBackgroundMode(true);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->setMatrix(currentMatrix);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::PLASMA_AND_TEXT:
    currentMatrix = plasmaMatrix;
    currentMatrix->setBackgroundMode(true);
    matrixDriver->setFPS(plasmaFPS);
    matrixDriver->setMatrix(currentMatrix);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(true);
    break;
  case MODES::GAME_ONLY:
    currentMatrix = gameLifeMatrix;
    currentMatrix->setBackgroundMode(false);
    matrixDriver->setFPS(gameLifeFPS);
    matrixDriver->setMatrix(currentMatrix);
    matrixDriver->enableBackgroundDrawing(true);
    matrixDriver->enableTextDrawing(false);
    break;
  case MODES::PLASMA_ONLY:
    currentMatrix = plasmaMatrix;
    currentMatrix->setBackgroundMode(false);

    matrixDriver->setFPS(plasmaFPS);
    matrixDriver->setMatrix(currentMatrix);
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

// simple test function to sweep colors on the onboard RGB LED. shows signs of life
void testSweepOnboardLED()
{
  // Simple HSV rainbow cycle on the onboard RGB LED to show signs of life
  static uint16_t hsvHue = 0;  // 0-65535
  static uint8_t hsvSat = 255; // 0-255
  static uint8_t hsvVal = 255; // 0-255

  // Only show LED if panel is enabled
  if (panelEnabled)
  {
    // Convert HSV to RGB and set LED color
    uint32_t color = pixel.gamma32(pixel.ColorHSV(hsvHue, hsvSat, hsvVal));
    pixel.setPixelColor(0, color);
    pixel.setBrightness(3); // Optional: adjust brightness dynamically
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
