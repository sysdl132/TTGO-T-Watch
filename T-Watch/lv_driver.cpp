#include "lv_driver.h"
#include "src/TFT_eSPI/TFT_eSPI.h"
#include <Ticker.h>
#include "board_def.h"
#include "freertos/FreeRTOS.h"
#include "src/FT5206_Library/src/FT5206.h"

#define BACKLIGHT_CHANNEL   ((uint8_t)1)
// #define DEBUG_DEMO

static TFT_eSPI *tft = nullptr;
static FT5206_Class *tp = nullptr;
static Ticker lvTicker1;
static Ticker lvTicker2;
static bool tpInit = false;


void serial_print(char * str) {
  Serial.println(str);
}

int tftGetScreenWidth()
{
  return tft->width();
}

int tftGetScreenHeight()
{
  return tft->height();
}

void display_off()
{
  tft->writecommand(TFT_DISPOFF);
  tft->writecommand(TFT_SLPIN);
  tp->enterSleepMode();
}

void display_sleep()
{
  tft->writecommand(TFT_DISPOFF);
  tft->writecommand(TFT_SLPIN);
  tp->enterMonitorMode();
}

void display_wakeup()
{
  tft->writecommand(TFT_SLPOUT);
  tft->writecommand(TFT_DISPON);
}

void display_init()
{
  tft = new TFT_eSPI(240, 240);
  tft->init();
  tft->setRotation(0);

  pinMode(TP_INT, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);
  tp = new FT5206_Class();
  if (! tp->begin(Wire)) {
    Serial.println("Couldn't start FT5206 touchscreen controller");
  } else {
    tpInit = true;
    Serial.println("Capacitive touchscreen started");
  }
  tft->fillScreen(TFT_GREEN);

  tft->drawPixel(tft->width() / 2, tft->height() / 2, TFT_RED);
  tft->setCursor(0, 0);
  tft->setTextColor(TFT_YELLOW);
  tft->setTextSize(1);
  tft->setTextWrap(true);
  tft->setTextFont(7);
  tft->println("12345");
  // tft->invertDisplay(false);
}

void backlight_init(void) {
  ledcAttachPin(TFT_BL, 1);
  ledcSetup(BACKLIGHT_CHANNEL, 12000, 8);
}


uint8_t backlight_getLevel()
{
  return ledcRead(BACKLIGHT_CHANNEL);
}

void backlight_adjust(uint8_t level)
{
  ledcWrite(BACKLIGHT_CHANNEL, level);
}

bool isBacklightOn()
{
  return (bool)ledcRead(BACKLIGHT_CHANNEL);
}

void backlight_off()
{
  ledcWrite(BACKLIGHT_CHANNEL, 0);
}

void backlight_on()
{
  ledcWrite(BACKLIGHT_CHANNEL, 200);
}
