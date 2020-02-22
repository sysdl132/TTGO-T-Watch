#include "board_def.h"
#include "lv_driver.h"
#include "lv_filesys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "struct_def.h"
#include <WiFi.h>
#include "src/lvgl/src/lvgl.h"
#include "lv_menu.h"
#include "motion_task.h"
#include <Ticker.h>
#include "src/AXP202X_Library/src/axp20x.h"
#include "src/Button2/src/Button2.h"
#include <SPI.h>
#include "src/PCF8563_Library/src/pcf8563.h"
#include <soc/rtc.h>
#include "lv_ble.h"

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

/**********************
  defines
**********************/

void wifi_event_setup()
{
  WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.disconnected.reason);
  }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

  eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Completed scan for access points");
    int16_t len =  WiFi.scanComplete();
    for (int i = 0; i < len; ++i) {
      lv_wifi_list_add(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
    WiFi.scanDelete();
  }, WiFiEvent_t::SYSTEM_EVENT_SCAN_DONE);

  eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("Connected to access point");

  }, WiFiEvent_t::SYSTEM_EVENT_STA_CONNECTED);

  eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("Obtained IP address: ");
    Serial.println(WiFi.localIP());
    task_event_data_t event_data;
    event_data.type = MESS_EVENT_WIFI;
    event_data.wifi.event = LVGL_WIFI_CONFIG_CONNECT_SUCCESS;
    xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
  }, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
}

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


void syncSystemTimeByRtc()
{
  struct tm t_tm;
  struct timeval val;
  RTC_Date dt = rtc.getDateTime();
  t_tm.tm_hour = dt.hour;
  t_tm.tm_min = dt.minute;
  t_tm.tm_sec = dt.second;
  t_tm.tm_year = dt.year - 1900;    //Year, whose value starts from 1900
  t_tm.tm_mon = dt.month - 1;       //Month (starting from January, 0 for January) - Value range is [0,11]
  t_tm.tm_mday = dt.day;
  val.tv_sec = mktime(&t_tm);
  val.tv_usec = 0;
  settimeofday(&val, NULL);
  Serial.print("Get RTC DateTime:");
  Serial.println(rtc.formatDateTime(PCF_TIMEFORMAT_YYYY_MM_DD_H_M_S));
  Serial.println("End of localtime");
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
  for (int level = 0; level < 255; level += 25) {
    backlight_adjust(level);
    delay(100);
  }

  lv_filesystem_init();

  axp.enableIRQ(AXP202_ALL_IRQ, AXP202_OFF);

  axp.adc1Enable(0xFF, AXP202_OFF);

  axp.adc2Enable(0xFF, AXP202_OFF);

  axp.adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);

  axp.enableIRQ(AXP202_VBUS_REMOVED_IRQ | AXP202_VBUS_CONNECT_IRQ | AXP202_CHARGING_FINISHED_IRQ, AXP202_ON);

  axp.clearIRQ();

  rtc.begin(Wire1);

  syncSystemTimeByRtc();

  wifi_event_setup();

  motion_task_init();

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

#ifdef ENABLE_BLE
  ble_init();
#endif

  xEventGroupSetBits(g_sync_event_group, BIT0);
}


void wifi_handle(void *data)
{
  int16_t len;
  wifi_struct_t *p = (wifi_struct_t *)data;
  switch (p->event) {
    case LVGL_WIFI_CONFIG_SCAN:
      Serial.println("[WIFI] WIFI Scan Start ...");
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      len = WiFi.scanNetworks();
      Serial.printf("[WIFI] WIFI Scan Done ...Scan %d len\n", len);
      break;
    case LVGL_WIFI_CONFIG_SCAN_DONE:
      break;
    case LVGL_WIFI_CONFIG_CONNECT_SUCCESS:
      Serial.println("Connect Success");
      wifiTicker->detach();
      delete wifiTicker;
      lv_wifi_connect_pass();
      configTzTime("CST-8", "pool.ntp.org");

      task_event_data_t event_data;
      event_data.type = MESS_EVENT_TIME;
      event_data.time.event = LVGL_TIME_SYNC;
      xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);

      break;
    case LVGL_WIFI_CONFIG_CONNECT_FAIL:
      Serial.println("Connect Fail,Power OFF WiFi");
      WiFi.disconnect(true);
      wifiTicker->detach();
      delete wifiTicker;
      lv_wifi_connect_fail();

      break;
    case LVGL_WIFI_CONFIG_TRY_CONNECT: {
        wifi_auth_t *auth = (wifi_auth_t *)p->ctx;
        wl_status_t ret = WiFi.begin(auth->ssid, auth->password);
        wifiTicker = new Ticker();
        wifiTicker->once_ms(10000, [] {
          task_event_data_t event_data;
          event_data.type = MESS_EVENT_WIFI;
          event_data.wifi.event = LVGL_WIFI_CONFIG_CONNECT_FAIL;
          xQueueSend(g_event_queue_handle, &event_data, portMAX_DELAY);
        });
      }
      break;
    default:
      break;
  }
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
        data.batt_cur = axp.isChargeing() ? data.batt_cur = axp.getBattChargeCurrent() : axp.getBattDischargeCurrent();
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
          bma423_disable_interrupt();
          xEventGroupClearBits(g_sync_event_group, BIT0);
          // rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
          rtc_clk_cpu_freq_set(RTC_CPU_FREQ_2M);
        } else {
          rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);
          // lv_task_enable(true);
          bma423_enable_interrupt();
          axp.setPowerOutPut(AXP202_LDO2, AXP202_ON);
          backlight_on();
          display_wakeup();
          touch_timer_create();
          syncSystemTimeByRtc();
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
        case MESS_EVENT_FILE:
          file_handle(&event_data.file);
          break;
        case MESS_EVENT_VOLT:
          break;
        case MESS_EVENT_SPEK:
          break;
        case MESS_EVENT_WIFI:
          wifi_handle(&event_data.wifi);
          break;
        case MESS_EVENT_MOTI:
          motion_handle(&event_data.motion);
          break;
        case MESS_EVENT_TIME:
          time_handle(&event_data.time);
          break;
        case MESS_EVENT_POWER:
          power_handle(&event_data.power);
          break;
        case MESS_EVENT_BLE:
#ifdef ENABLE_BLE
          ble_handle(&event_data.ble);
#endif
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
extern "C" const char *get_wifi_ssid()
{
  return WiFi.SSID() == "" ? "None" : WiFi.SSID().c_str();
}
extern "C" const char *get_wifi_rssi()
{
  return String(WiFi.RSSI()).c_str();
}
extern "C" const char *get_wifi_channel()
{
  return String(WiFi.channel()).c_str();
}
extern "C" const char *get_wifi_address()
{
  return WiFi.localIP().toString().c_str();
}
extern "C" const char *get_wifi_mac()
{
  return WiFi.macAddress().c_str();
}

void gps_power_on()
{
  axp.setLDO3Mode(1);
  axp.setPowerOutPut(AXP202_LDO3, AXP202_ON);
}

void gps_power_off()
{
  axp.setPowerOutPut(AXP202_LDO3, AXP202_OFF);
}

void s7xg_power_on()
{
  axp.setLDO4Voltage(AXP202_LDO4_1800MV);
  axp.setPowerOutPut(AXP202_LDO4, AXP202_ON);
}

void s7xg_power_off()
{
  axp.setPowerOutPut(AXP202_LDO4, AXP202_OFF);
}
