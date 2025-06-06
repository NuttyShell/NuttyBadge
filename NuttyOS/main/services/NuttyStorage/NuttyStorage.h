#ifndef _NUTTYSTORAGE_H
#define _NUTTYSTORAGE_H

#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "drivers/sdcard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <esp_log.h>

esp_err_t NuttyStorage_Init();
bool NuttyStorage_isSDCardMounted();
bool NuttyStorage_isSDCardInserted();
bool NuttyStorage_isSDCardPersist();
void NuttyStorage_getSDCardSizeMB(uint64_t *mb);
void NuttyStorage_getSDCardType(char **type);

#endif /* _NUTTYSTORAGE_H */