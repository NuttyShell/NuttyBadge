#ifndef _SDCARD_H
#define _SDCARD_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include "NuttyPeripherals.h"
#include "drivers/ioe.h"
#include "drivers/ir.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

typedef esp_err_t (*NuttyDriverSDCardInitFunc)(void);
typedef void (*NuttyDriverSDCardPrintSDCardInfoFunc)(void);
typedef esp_err_t (*NuttyDriverSDCardMountSDCardFunc)(void);
typedef esp_err_t (*NuttyDriverSDCardUnmountSDCardFunc)(void);
typedef esp_err_t (*NuttyDriverSDCardSizeMBFunc)(uint64_t *size_mb);
typedef esp_err_t (*NuttyDriverSDCardTypeFunc)(char **type);
typedef bool (*NuttyDriverSDCardIsSDCardMountedFunc)(void);
typedef bool (*NuttyDriverSDCardIsSDCardPersistFunc)(void);
typedef bool (*NuttyDriverSDCardGetSDInsertedFunc)(void);
typedef esp_err_t (*NuttyDriverSDCardLSDirFunc)(const char *);
typedef struct _NuttyDriverSDCard {
    NuttyDriverSDCardInitFunc initSDCard;
    NuttyDriverSDCardPrintSDCardInfoFunc printSDCardInfo;
    NuttyDriverSDCardMountSDCardFunc mountSDCard;
    NuttyDriverSDCardUnmountSDCardFunc unmountSDCard;
    NuttyDriverSDCardSizeMBFunc getSDCardSizeMB;
    NuttyDriverSDCardTypeFunc getSDCardType;
    NuttyDriverSDCardIsSDCardMountedFunc isSDCardMounted;
    NuttyDriverSDCardIsSDCardPersistFunc isSDCardPersist;
    NuttyDriverSDCardGetSDInsertedFunc getCardInserted;
    NuttyDriverSDCardLSDirFunc lsDir;
    
} NuttyDriverSDCard;
extern NuttyDriverSDCard nuttyDriverSDCard;

#endif /* _SDCARD_H */