#include "board_def.h"
#include "lv_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "struct_def.h"
#include "src/lvgl/src/lvgl.h"
#include "lv_menu.h"
#include <Ticker.h>
#include "src/AXP202X_Library/src/axp20x.h"
#include "src/Button2/src/Button2.h"
#include <SPI.h>
#include "src/PCF8563_Library/src/pcf8563.h"
#include <soc/rtc.h>

//项目使用了 2472802 字节，占用了 (37%) 程序存储空间。最大为 6553600 字节。
//全局变量使用了121868字节，(2%)的动态内存，余留4400116字节局部变量。最大为4521984字节。

//项目使用了 2462330 字节，占用了 (37%) 程序存储空间。最大为 6553600 字节。
//全局变量使用了98828字节，(2%)的动态内存，余留4423156字节局部变量。最大为4521984字节。

// 项目使用了 1647050 字节，占用了 (25%) 程序存储空间。最大为 6553600 字节。
// 全局变量使用了121956字节，(2%)的动态内存，余留4400028字节局部变量。最大为4521984字节。

// 全局变量使用了74916字节，(1%)的动态内存，余留4447068字节局部变量。最大为4521984字节。

// 项目使用了 1535226 字节，占用了 (23%) 程序存储空间。最大为 6553600 字节。
// 全局变量使用了121444字节，(2%)的动态内存，余留4400540字节局部变量。最大为4521984字节。

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
static Ticker *wifiTicker = nullptr;
static Ticker btnTicker;
Button2 btn(USER_BUTTON);
Ticker pwmTicker;
power_data_t data;

static void time_task(void *param);


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

  display_init();

  Wire1.begin(SEN_SDA, SEN_SCL);

  axp.begin(Wire1);

  backlight_init();

  //BL Power
  axp.setPowerOutPut(AXP202_LDO2, AXP202_ON);

  Serial.printf("lv_create_ttgo\r\n");

  lv_create_ttgo();

  int level = backlight_getLevel();
  for (int level = 0; level < 255; level += 5) {
    backlight_adjust(level);
    delay(20);
  }

  axp.enableIRQ(AXP202_ALL_IRQ, AXP202_OFF);

  axp.adc1Enable(0xFF, AXP202_OFF);

  axp.adc2Enable(0xFF, AXP202_OFF);

  axp.adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);

  axp.enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);

  axp.clearIRQ();

  rtc.begin(Wire1);


  lv_main();

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
    charging_anim_start();
  }

  xTaskCreate(time_task, "time", 2048, NULL, 20, NULL);


  xEventGroupSetBits(g_sync_event_group, BIT0);
}


void power_handle(void *param)
{
  power_struct_t *p = (power_struct_t *)param;
  switch (p->event) {
    case LVGL_POWER_GET_MOINITOR:
      pwmTicker.attach_ms(1000, [] {
        data.vbus_vol = axp.getVbusVoltage();
        data.vbus_cur = axp.getVbusCurrent();
        data.batt_vol = axp.getBattVoltage();
        data.batt_cur = axp.isChargeing() ? axp.getBattChargeCurrent() : axp.getBattDischargeCurrent();
        lv_update_power_info(&data);
      });
      break;
    case LVGL_POWER_MOINITOR_STOP:
      pwmTicker.detach();
      break;
    case LVGL_POWER_IRQ:
      axp.readIRQ();
      if (axp.isVbusPlugInIRQ()) {
        charging_anim_start();
      }
      if (axp.isVbusRemoveIRQ()) {
        charging_anim_stop();
      }
      if (axp.isChargingDoneIRQ()) {
        charging_anim_stop();
      }
      if (axp.isPEKShortPressIRQ()) {
        if (isBacklightOn()) {
          // lv_task_enable(false);
          backlight_off();
          display_sleep();
          axp.setPowerOutPut(AXP202_LDO2, AXP202_OFF);
          xEventGroupClearBits(g_sync_event_group, BIT0);
          // rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
          rtc_clk_cpu_freq_set(RTC_CPU_FREQ_2M);
        } else {
          rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);
          // lv_task_enable(true);
          axp.setPowerOutPut(AXP202_LDO2, AXP202_ON);
          backlight_on();
          display_wakeup();
          touch_timer_create();
          xEventGroupSetBits(g_sync_event_group, BIT0);
        }
      }
      axp.clearIRQ();
      break;

    case LVGL_POWER_ENTER_SLEEP: {
        lv_create_ttgo();
        int level = backlight_getLevel();
        for (; level > 0; level -= 25) {
          backlight_adjust(level);
          delay(100);
        }
        display_off();
        axp.setPowerOutPut(AXP202_LDO2, AXP202_OFF);
        esp_sleep_enable_ext1_wakeup( ((uint64_t)(((uint64_t)1) << USER_BUTTON)), ESP_EXT1_WAKEUP_ALL_LOW);
        esp_deep_sleep_start();
      }
      break;
    default:
      break;
  }
}
#define DEFAULT_BATT_POLL   5

void time_handle(void *param)
{
  bool rslt;
  char time_buf[64];
  char date_buf[64];
  uint8_t batteryLoop = 0;
  time_struct_t *p = (time_struct_t *)param;
  switch (p->event) {
      {
      case LVGL_TIME_UPDATE:
        if (lv_main_in()) {
          strftime(time_buf, sizeof(time_buf), "%H:%M", &(p->time));
          strftime(date_buf, sizeof(date_buf), "%a %d %b ", &(p->time));
          lv_main_time_update(time_buf, date_buf);
          if (++batteryLoop >= DEFAULT_BATT_POLL) {
            batteryLoop = 0;
            lv_update_battery_percent(axp.getBattPercentage());
          }
        }
        break;
      case LVGL_TIME_ALARM:
        break;
      case LVGL_TIME_SYNC:
        do {
          rslt = syncRtcBySystemTime();
        } while (!rslt);
        break;
      default:
        break;
      }
  }
}


void loop()
{
  task_event_data_t event_data;
  for (;;) {
    if (xQueueReceive(g_event_queue_handle, &event_data, portMAX_DELAY) == pdPASS) {
      // Serial.printf("loop event: %d\r\n", event_data.type);
      switch (event_data.type) {
        case MESS_EVENT_VOLT:
          break;
        case MESS_EVENT_SPEK:
          break;
        case MESS_EVENT_TIME:
          time_handle(&event_data.time);
          break;
        case MESS_EVENT_POWER:
          power_handle(&event_data.power);
          break;
        default:
          Serial.println("Error event");
          break;
      }
    }
  }
}

static void time_task(void *param)
{
  struct tm time;
  task_event_data_t event_data;
  for (;;) {
    EventBits_t bits =  xEventGroupWaitBits(g_sync_event_group,
                                            BIT0,
                                            pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & BIT0) {
      if (getLocalTime(&time)) {
        event_data.type = MESS_EVENT_TIME;
        event_data.time.event = LVGL_TIME_UPDATE;
        event_data.time.time = time;
        xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
      }
    }
    delay(1000);
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
