#ifndef _NUTTYINPUT_H
#define _NUTTYINPUT_H

#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

#define NUTTYINPUT_BTN_USRDEF 0x0100
#define NUTTYINPUT_BTN_START 0x0080
#define NUTTYINPUT_BTN_SELECT 0x0040
#define NUTTYINPUT_BTN_B 0x0020
#define NUTTYINPUT_BTN_A 0x0010
#define NUTTYINPUT_BTN_RIGHT 0x0008
#define NUTTYINPUT_BTN_LEFT 0x0004
#define NUTTYINPUT_BTN_DOWN 0x0002
#define NUTTYINPUT_BTN_UP 0x0001

void ioe_isr_handler(void* arg);
bool isOneOfTheButtonsPressed(uint16_t);
void waitSingleButtonHoldAndReleased(uint16_t btn);
bool waitSingleButtonHoldAndReleasedNonBlock(uint16_t btn);
void clearButtonHoldState(uint16_t btn);

esp_err_t NuttyInput_Init();

#endif /* _NUTTYINPUT_H */