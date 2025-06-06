#ifndef _NUTTYINPUT_H
#define _NUTTYINPUT_H

#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

#include <lvgl.h>

#define NUTTYINPUT_BTN_USRDEF 0x0100
#define NUTTYINPUT_BTN_START 0x0080
#define NUTTYINPUT_BTN_SELECT 0x0040
#define NUTTYINPUT_BTN_B 0x0020
#define NUTTYINPUT_BTN_A 0x0010
#define NUTTYINPUT_BTN_RIGHT 0x0008
#define NUTTYINPUT_BTN_LEFT 0x0004
#define NUTTYINPUT_BTN_DOWN 0x0002
#define NUTTYINPUT_BTN_UP 0x0001
#define NUTTYINPUT_BTN_ALL 0x01ff

typedef struct _NuttyInputLVGLInputMapping {
    lv_key_t UP;
    lv_key_t DOWN;
    lv_key_t LEFT;
    lv_key_t RIGHT;
    lv_key_t A;
    lv_key_t B;
    lv_key_t SELECT;
    lv_key_t START;
    lv_key_t USRDEF;
} NuttyInputLVGLInputMapping;

void ioe_isr_handler(void* arg);
bool NuttyInput_isOneOfTheButtonsCurrentlyPressed(uint16_t);
void NuttyInput_waitSingleButtonHoldAndReleasedBlocking(uint16_t btn);
bool NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(uint16_t btn);
bool NuttyInput_waitSingleButtonHoldLongNonBlock(uint16_t btn);
void NuttyInput_clearButtonHoldState(uint16_t btn);
lv_indev_t* NuttyInput_UpdateLVGLInDev(NuttyInputLVGLInputMapping mapping);

esp_err_t NuttyInput_Init();

#endif /* _NUTTYINPUT_H */