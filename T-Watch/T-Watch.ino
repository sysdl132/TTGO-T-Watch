#include "board_def.h"
#include "struct_def.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "struct_def.h"
#include <WiFi.h>
#include <Ticker.h>
#include "src/AXP202X_Library/src/axp20x.h"
#include "src/Button2/src/Button2.h"
#include <SPI.h>
#include "src/PCF8563_Library/src/pcf8563.h"
#include <soc/rtc.h>
#include "lv_driver.h"

/*********************
        DEFINES
 *********************/
#define TASK_QUEUE_MESSAGE_LEN      10

/**********************

    STATIC VARIABLES
 **********************/
AXP20X_Class axp;
PCF8563_Class rtc;
EventGroupHandle_t g_sync_event_group = NULL;
QueueHandle_t g_event_queue_handle = NULL;
static Ticker btnTicker;
Button2 btn(USER_BUTTON);
Ticker pwmTicker;
power_data_t data;


bool syncRtcBySystemTime()
{
  struct tm info;
  time_t now;
  time(&now);
  localtime_r(&now, &info);
  if (info.tm_year > (2016 - 1900)) {
    Serial.println("syncRtcBySystemTime");
    //Month (starting from January, 0 for January) - Value range is [0,11]
    rtc.setDateTime(info.tm_year, info.tm_mon + 1, info.tm_mday, info.tm_hour, info.tm_min, info.tm_sec);
    return true;
  }
  return false;
}


void setup()
{
  Serial.begin(115200);
  Serial.println("booting ...");

  g_sync_event_group = xEventGroupCreate();

  g_event_queue_handle = xQueueCreate(TASK_QUEUE_MESSAGE_LEN, sizeof(task_event_data_t));

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);

  Wire1.begin(SEN_SDA, SEN_SCL);

  axp.begin(Wire1);

  Serial.println("1 ...");
  backlight_init();

  Serial.println("2 ...");
  display_init();

  Serial.println("3 ...");
  //BL Power
  axp.setPowerOutPut(AXP202_LDO2, AXP202_ON);

  for (int level = 0; level < 128; level += 25) {
    backlight_adjust(level);
    delay(100);
  }

  Serial.println("light ok!");

  Serial.println(ESP.getFreeHeap());

  graphic_test();

  test_canvas_buffer();

  Serial.println(ESP.getFreeHeap());

  while (1) {
    graphic_test();
  }

  axp.enableIRQ(AXP202_ALL_IRQ, AXP202_OFF);

  axp.adc1Enable(0xFF, AXP202_OFF);

  axp.adc2Enable(0xFF, AXP202_OFF);

  axp.adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);

  axp.enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);

  axp.clearIRQ();

  rtc.begin(Wire1);

  btn.setLongClickHandler([](Button2 & b) {
    task_event_data_t event_data;
    event_data.type = MESS_EVENT_POWER;
    event_data.power.event = LVGL_POWER_ENTER_SLEEP;
    xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
  });

  btnTicker.attach_ms(30, [] {
    btn.loop();
  });

  pinMode(AXP202_INT, INPUT_PULLUP);
  attachInterrupt(AXP202_INT, [] {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    task_event_data_t event_data;
    event_data.type = MESS_EVENT_POWER;
    event_data.power.event = LVGL_POWER_IRQ;
    xQueueSendFromISR(g_event_queue_handle, &event_data, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
      portYIELD_FROM_ISR ();
    }
  }, FALLING);


  if (axp.isChargeing()) {
  }

  xEventGroupSetBits(g_sync_event_group, BIT0);
}

void loop() {
  task_event_data_t event_data;
  for (;;) {
    if (xQueueReceive(g_event_queue_handle, &event_data, portMAX_DELAY) == pdPASS) {
      // Serial.printf("loop event: %d\r\n", event_data.type);
      switch (event_data.type) {
        case MESS_EVENT_FILE:
          break;
        case MESS_EVENT_VOLT:
          break;
        case MESS_EVENT_SPEK:
          break;
        case MESS_EVENT_WIFI:
          break;
        case MESS_EVENT_MOTI:
          break;
        case MESS_EVENT_TIME:
          break;
        case MESS_EVENT_POWER:
          break;
        case MESS_EVENT_BLE:
          break;
        default:
          Serial.println("Error event");
          break;
      }
    }
  }
}



extern "C" int get_batt_percentage()
{
  return axp.getBattPercentage();
}
extern "C" int get_ld1_status()
{
  return 1;
}
extern "C" int get_ld2_status()
{
  return axp.isLDO2Enable();
}
extern "C" int get_ld3_status()
{
  return axp.isLDO3Enable();
}
extern "C" int get_ld4_status()
{
  return axp.isLDO4Enable();
}
extern "C" int get_dc2_status()
{
  return axp.isDCDC2Enable();
}
extern "C" int get_dc3_status()
{
  return axp.isDCDC3Enable();
}

static uint32_t _cpu_freq_mhz = 240;

bool cpuFrequencySet(uint32_t cpu_freq_mhz) {
  if (_cpu_freq_mhz == cpu_freq_mhz) {
    return true;
  }
  rtc_cpu_freq_config_t conf;
  if (!rtc_clk_cpu_freq_mhz_to_config(cpu_freq_mhz, &conf)) {
    log_e("CPU clock could not be set to %u MHz", cpu_freq_mhz);
    return false;
  }
  rtc_clk_cpu_freq_set_config(&conf);
  _cpu_freq_mhz = conf.freq_mhz;
  return true;
}
