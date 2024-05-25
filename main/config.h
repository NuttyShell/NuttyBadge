#ifndef _CONFIG_h
#define _CONFIG_h

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/****
G0 Strapping Pin (BOOT Mode), [I2C DATA]
G1 [I2C SCL]
G2 [IO EXAPNDER INTERRUPT]
G3-5 [SDIO for SD Card]
(Remember to Pull CMD, DATA to 3V3)*
G6 [LCD BL PWM]
G7 [IR RX]
G8 [LCD A0]
G9 [ADC1_CH8 for Battery Voltage Reading]
G10, G11, G12, G13 [FSPI for LCD, CC1101]
G10, G11 Board LED
G14 [WS2812b DATA]
G15 [CC1101 Interrupt]
G16 [CC1101 SPI CS]
G17 [AUDIO OUT]
G18 [IR TX/SD Card Detect]
G19, G20 ESP USB D-/D+
G21 On-board Camera Port
G26-G37 SPI FLASH/PSRAM
(G33-G37 = SPIIO4-7, SPIDQS)
G33-G42 On-board Camera Port
G43, G44 UART0
G45, G46 Strapping Pin (VDD_SPI Voltage, BOOT Mode)
G45-G48 On-board Camera Port
*/

#define GPIO_I2C_SDA      0  // I2C Bus SDA
#define GPIO_I2C_SCL      1  // I2C Bus SCL
#define GPIO_IOE_INT      2  // IO Expander Intrrupt
#define GPIO_IOE_INT_EDGE GPIO_INTR_NEGEDGE
#define GPIO_IOE_INT_ISR  ESP_INTR_FLAG_EDGE
#define GPIO_SD_CLK       3  // SD Card SD Bus CLK
#define GPIO_SD_CMD       4  // SD Card SD Bus CMD
#define GPIO_SD_DAT0      5  // SD Card SD Bus DAT0
#define GPIO_LCD_BL_PWM   6  // LCD Backlight PWM Out
#define GPIO_IR_RX        7  // IR Receive
#define GPIO_LCD_A0       8  // A0 Pin for LCD
#define GPIO_BATT_ADC     9  // Battery Readout Pin (Should use Analog, see next line)
#define ADC_BATT_ADC      -1 // Battery ADC
#define GPIO_LCD_CS       10 // CS Pin for LCD
#define GPIO_FSPI_MOSI    11 // FSPI MOSI Pin
#define GPIO_FSPI_SCK     12 // FSPI SCK Pin
#define GPIO_FSPI_MISO    13 // FSPI MISO Pin
#define GPIO_RGB_DATA     14 // WS2812B RGB Data Pin
#define GPIO_CCMOD_GDO0   15 // GDO0 Pin for CC Module
#define GPIO_CCMOD_CS     16 // CS Pin for CC Module
#define GPIO_AUDIO_OUT    17 // Audio PWM Out
#define GPIO_IR_TX_SDCD   18 // On Circuit Multiplexed for 2 functions: IR LED Transmit and SD Card Detect


#define LEDC_LCD_BL_PWM_DUTYRES LEDC_TIMER_8_BIT
#define LEDC_LCD_BL_PWM_FREQ 200000
#define LEDC_LCD_BL_PWM_SPEED LEDC_LOW_SPEED_MODE
#define LEDC_LCD_BL_PWM_CHANNEL LEDC_CHANNEL_0
#define LEDC_LCD_BL_PWM_TIMER LEDC_TIMER_1
#define LEDC_LCD_BL_PWM_INVERT 0

#define ADC_ATTEN ADC_ATTEN_DB_6
#define ADC_CHANNEL ADC_CHANNEL_8
#define ADC_UNIT ADC_UNIT_1
#define ADC_BITWIDTH ADC_BITWIDTH_DEFAULT
#define ADC_MV_ADJUST +2 // Account for ADC Input impedance, Voltage Divider Error, and ADC Atten Error

#define FSPI_HOST SPI2_HOST
#define LCD_SPEED_HZ (20*1000*1000)
#define CC_SPEED_HZ (10*1000*1000)

#define RMT_RGB_MEM_BLK_SYMBOLS 64
#define RMT_RGB_RESOLUTION_HZ 10000000
#define RGB_SHIFT_BITS 4

#define RMT_IRTX_MEM_BLK_SYMBOLS 64
#define RMT_IRTX_RESOLUTION_HZ 1000000
#define RMT_IRRX_NEC_DECODE_MARGIN 200
#define RMT_IRRX_MEM_BLK_SYMBOLS 64
#define RMT_IRRX_RESOLUTION_HZ 1000000

#define IOE_I2C_ADDRESS 0x20

#define BTN_DEBOUNCE_PRESSED_THRESHOLD 8
#define BTN_DEBOUNCE_NOTPRESSED_THRESHOLD 4
#define BTN_HOLD_THRESHOLD 190

#define NUTTYSYSTEMMONITOR_LOW_BATTERY_THRESHOLD_MV 1900
#define NUTTYDISPLAY_INVERT_COLOR 1
#define NUTTYDISPLAY_USER_APP_AREA_WIDTH 128
#define NUTTYDISPLAY_USER_APP_AREA_HEIGHT 59
#define NUTTYDISPLAY_SYSTEM_TRAY_AREA_WIDTH 128
#define NUTTYDISPLAY_SYSTEM_TRAY_AREA_HEIGHT 5

#define SDCARD_MOUNT_POINT "/sdcard"

#endif /* _CONFIG_h */