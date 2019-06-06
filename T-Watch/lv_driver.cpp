#include "lv_driver.h"
#include "src/TFT_eSPI/TFT_eSPI.h"
#include <Ticker.h>
#include "board_def.h"
#include "freertos/FreeRTOS.h"
#include "src/FT5206_Library/src/FT5206.h"

#define BACKLIGHT_CHANNEL   ((uint8_t)1)
// #define DEBUG_DEMO

TFT_eSPI tft = TFT_eSPI(240, 240);

static FT5206_Class *tp = nullptr;
static Ticker lvTicker1;
static Ticker lvTicker2;
static bool tpInit = false;


void serial_print(char * str) {
  Serial.println(str);
}

int tftGetScreenWidth()
{
  return tft.width();
}

int tftGetScreenHeight()
{
  return tft.height();
}

void display_off()
{
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
  tp->enterSleepMode();
}

void display_sleep()
{
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
  tp->enterMonitorMode();
}

void display_wakeup()
{
  tft.writecommand(TFT_SLPOUT);
  tft.writecommand(TFT_DISPON);
}


void display_init() {
  tft.init();
  tft.setRotation(0);

  pinMode(TP_INT, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);
  tp = new FT5206_Class();
  if (! tp->begin(Wire)) {
    Serial.println("Couldn't start FT5206 touchscreen controller");
  } else {
    tpInit = true;
    Serial.println("Capacitive touchscreen started");
  }
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

void test_lines(uint16_t color) {
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawLine(0, 0, x, tft.height() - 1, color);
    delay(0);
  }
  for (int16_t y = 0; y < tft.height(); y += 6) {
    tft.drawLine(0, 0, tft.width() - 1, y, color);
    delay(0);
  }

  tft.fillScreen(TFT_BLACK);
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawLine(tft.width() - 1, 0, x, tft.height() - 1, color);
    delay(0);
  }
  for (int16_t y = 0; y < tft.height(); y += 6) {
    tft.drawLine(tft.width() - 1, 0, 0, y, color);
    delay(0);
  }

  tft.fillScreen(TFT_BLACK);
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawLine(0, tft.height() - 1, x, 0, color);
    delay(0);
  }
  for (int16_t y = 0; y < tft.height(); y += 6) {
    tft.drawLine(0, tft.height() - 1, tft.width() - 1, y, color);
    delay(0);
  }

  tft.fillScreen(TFT_BLACK);
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawLine(tft.width() - 1, tft.height() - 1, x, 0, color);
    delay(0);
  }
  for (int16_t y = 0; y < tft.height(); y += 6) {
    tft.drawLine(tft.width() - 1, tft.height() - 1, 0, y, color);
    delay(0);
  }
}

void testdrawrects(uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawRect(tft.width() / 2 - x / 2, tft.height() / 2 - x / 2 , x, x, color);
  }
}


void graphic_test() {
  tft.fillScreen(TFT_GREEN);

  tft.drawPixel(tft.width() / 2, tft.height() / 2, TFT_RED);

  tft.setCursor(0, 0);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  tft.setTextFont(7);
  tft.println("12345");
  test_lines(TFT_RED);
  delay(500);
  testdrawrects(TFT_GREEN);
  delay(500);

  // tft->invertDisplay(false);
}
