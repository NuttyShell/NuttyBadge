#ifndef _USB_H
#define _USB_H

#include "config.h"
#include <string.h>
#include <esp_err.h>
#include <sys/stat.h>
#include "esp_partition.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"

#include "sdcard.h"

typedef esp_err_t (*NuttyDriverUSBInitFunc)(void);
typedef struct _NuttyDriverUSB {
    NuttyDriverUSBInitFunc initUSB;
} NuttyDriverUSB;

extern NuttyDriverUSB nuttyDriverUSB;

#endif /* _USB_H */
