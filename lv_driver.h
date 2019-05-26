#pragma onec
#include <Arduino.h>


#ifdef __cplusplus
extern "C" {
#endif
void display_init();
int tftGetScreenHeight();
int tftGetScreenWidth();
void touch_timer_create();
void display_off();
void display_wakeup();
void display_sleep();
void backlight_init(void);
void backlight_setting(unsigned char level);
void backlight_off();
void backlight_on();
bool isBacklightOn();
void backlight_adjust(uint8_t level);
uint8_t backlight_getLevel();

void serial_print(char * str);

#ifdef __cplusplus
} /* extern "C" */
#endif
