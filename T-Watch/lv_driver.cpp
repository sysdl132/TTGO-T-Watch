#include "lv_driver.h"
#include "src/TFT_eSPI/TFT_eSPI.h"
#include <Ticker.h>
#include "board_def.h"
#include "esp_jpg_decode.h"
#include "freertos/FreeRTOS.h"
#include "src/FT5206_Library/src/FT5206.h"

#include "Adafruit_GFX.h"

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

static uint32_t _jpg_read(void * arg, size_t index, uint8_t *buf, size_t len) {
  if (buf) {
    memcpy(buf, test_map + index, len);
  }
  return len;
}

typedef struct {
  uint16_t width;
  uint16_t height;
  uint16_t data_offset;
  const uint8_t *input;
  uint8_t *output;
} rgb_jpg_decoder;

static bool _rgb_write(void * arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data) {
  // Serial.printf("_rgb_write: %d, %d, %d, %d, %d\r\n", x, y, w, h, data);
  rgb_jpg_decoder * jpeg = (rgb_jpg_decoder *)arg;
  if (!data) {
    if (x == 0 && y == 0) {
      //write start
      jpeg->width = w;
      jpeg->height = h;
      Serial.printf("w=%d, h=%d\r\n", w, h);
      // if output is null, this is BMP
      if (!jpeg->output) {
        Serial.println("malloc@@@");
        jpeg->output = (uint8_t *)malloc((w * h * 3) + jpeg->data_offset);
        if (!jpeg->output) {
          return false;
        }
      }
    } else {
      Serial.println("write end");
      //write end
    }
    return true;
  }

  size_t jw = jpeg->width * 3;
  size_t t = y * jw;
  size_t b = t + (h * jw);
  size_t l = x * 3;
  uint8_t *out = jpeg->output + jpeg->data_offset;
  uint8_t *o = out;
  size_t iy, ix;

  w = w * 3;

  for (iy = t; iy < b; iy += jw) {
    o = out + iy + l;
    for (ix = 0; ix < w; ix += 3) {
      o[ix] = data[ix + 2];
      o[ix + 1] = data[ix + 1];
      o[ix + 2] = data[ix];
    }
    data += w;
  }
  return true;
}

void test_canvas_buffer() {
  GFXcanvas16 canvas(240, 240);

  uint8_t * bitmap_rgb888 = (uint8_t*) ps_malloc(240 * 180 * 3);
  uint8_t * bitmap_rgb565 = (uint8_t*) ps_malloc(240 * 180 * 2);

  if (bitmap_rgb888 <= 0 || bitmap_rgb565 <= 0) {
    Serial.println("failed");
  }

  Serial.println(ESP.getFreeHeap());
  rgb_jpg_decoder jpeg;
  jpeg.width = 0;
  jpeg.height = 0;
  jpeg.input = test_map;
  jpeg.output = bitmap_rgb888;
  jpeg.data_offset = 0;

  if (esp_jpg_decode(sizeof(test_map), JPG_SCALE_NONE, _jpg_read, _rgb_write, (void*)&jpeg) != ESP_OK) {
    Serial.println("esp_jpg_decode failed.");
  } else {
    Serial.println("esp_jpg_decode ok!");
  }

  for (uint16_t row = 0; row < 180; row++) {
    for (uint16_t col = 0; col < 240; col++) {
      uint16_t index = row * 240 + col;
      uint8_t r = bitmap_rgb888[index * 3] >> 3;
      uint8_t g = bitmap_rgb888[index * 3 + 1] >> 2;
      uint8_t b = bitmap_rgb888[index * 3 + 2] >> 3;
      uint8_t byte1 = (b << 3) | (g >> 3);
      uint8_t byte2 = (g << 5) | r;
      bitmap_rgb565[index * 2] = byte1;
      bitmap_rgb565[index * 2 + 1] = byte2;
    }
  }
  Serial.println("----565 ok!");
  Serial.println(ESP.getFreeHeap());
  uint16_t* buffer = canvas.getBuffer();

  unsigned long start_time = millis();
  for (int i = 0; i < 50; i++) {
    if (i % 2) {
      canvas.fillScreen(0xF800); // red
    } else {
      canvas.fillScreen(0xFFE0); // yellow
    }
    tft.setAddrWindow(0, 0, 240, 240);
    tft.pushColors(buffer, 240 * 240);
    // tft.pushColors((uint16_t*)test0, 240 * 180);
  }
  tft.pushImage(0, 0, 240, 180, (uint16_t*)bitmap_rgb565);
  delay(50005);
  // 30ms per frame @ 40M SPI, 29.39 ms pure communication time
  // 21ms per frame @ 80M SPI, 17.87 ms pure communication time
  Serial.printf("100 frames cost: %d ms! \r\n", millis() - start_time);

  delay(4000);
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
