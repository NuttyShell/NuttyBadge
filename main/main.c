#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "NuttyPeripherals.h"

#include "drivers/ioe.h"
#include "drivers/lcd.h"
#include "drivers/rgb.h"
#include "drivers/ir.h"
#include "drivers/sdcard.h"

#include "services/NuttyAudio/NuttyAudio.h"
#include "services/NuttyInput/NuttyInput.h"
#include "services/NuttySystemMonitor/NuttySystemMonitor.h"
#include "services/NuttyDisplay/NuttyDisplay.h"
#include "services/NuttyRGB/NuttyRGB.h"
#include "services/NuttyIR/NuttyIR.h"
#include "services/NuttyApps/NuttyApps.h"

#include "apps/NuttyMenu/NuttyMenu.h"
#include "apps/NuttyTest/NuttyTest.h"
#include "apps/NuttySnake/NuttySnake.h"
#include "apps/NuttyAudioPlayer/NuttyAudioPlayer.h"
#include "apps/NuttyRemote/NuttyRemote.h"
#include "apps/NuttyRF/NuttyRF.h"
#include "apps/NuttyRGBControl/NuttyRGBControl.h"
#include "apps/NuttyWifiScanner/NuttyWifiScanner.h"
#include "apps/NuttyBTRemote/NuttyBTRemote.h"
#include "apps/NuttyCounter/NuttyCounter.h"
#include "apps/NuttyBTScanner/NuttyBTScanner.h"
#include "apps/NuttyStopwatch/NuttyStopwatch.h"
#include "apps/NuttyFileManager/NuttyFileManager.h"
#include "apps/NuttySettings/NuttySettings.h"
#include "apps/NuttyAbout/NuttyAbout.h"

static const char* TAG = "NuttyOS";


extern uint8_t ussr_start[] asm("_binary_ussr_bin_start");
extern uint8_t ussr_end[]   asm("_binary_ussr_bin_end");
extern uint8_t ussr_8k_start[] asm("_binary_ussr_8k_bin_start");
extern uint8_t ussr_8k_end[]   asm("_binary_ussr_8k_bin_end");
extern uint8_t ussr_48k_start[] asm("_binary_ussr_48k_bin_start");
extern uint8_t ussr_48k_end[]   asm("_binary_ussr_48k_bin_end");
extern uint8_t ussr_96k_start[] asm("_binary_ussr_96k_bin_start");
extern uint8_t ussr_96k_end[]   asm("_binary_ussr_96k_bin_end");


extern const uint8_t boot_icon_start[] asm("_binary_boot_icon_png_start");
extern const uint8_t boot_icon_end[]   asm("_binary_boot_icon_png_end");

void app_main(void) {
    // Peripherals (GPIO, LEDC, I2C, GPIOISR)
    ESP_ERROR_CHECK(nuttyPeripherals.initGPIO());
    ESP_ERROR_CHECK(nuttyPeripherals.initLEDC());
    ESP_ERROR_CHECK(nuttyPeripherals.initI2C());
    ESP_ERROR_CHECK(nuttyPeripherals.initADC());
    ESP_ERROR_CHECK(nuttyPeripherals.initFSPI());
    ESP_ERROR_CHECK(nuttyPeripherals.initSDHost());
    // RMT (IR Tx/IR Rx/RGB) Does not need to init, will do for individual device

    // Nutty Drivers (IOE, LCD)
    ESP_ERROR_CHECK(nuttyDriverIOE.initIOE());
    ESP_ERROR_CHECK(nuttyDriverLCD.initLCD());

    // We should able to control the LCD now
    NuttyDisplay_Init();
    NuttyDisplay_setLCDBacklight(30);

    vTaskDelay(pdMS_TO_TICKS(200));

    // Show NuttyShell boot screen
    NuttyDisplay_showPNG((uint8_t *)boot_icon_start, boot_icon_end - boot_icon_start);
    vTaskDelay(pdMS_TO_TICKS(1500));
    NuttyDisplay_clearWholeScreen();

    // Show Initialization
    lv_obj_t *parent = lv_scr_act();
    lv_obj_t *lbl;
    NuttyDisplay_lockLVGL();
    lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "NuttyOS\nInitializing...");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    NuttyDisplay_unlockLVGL();

    // Doing the inits
    ESP_ERROR_CHECK(nuttyDriverRGB.initRGB(3));
    ESP_ERROR_CHECK(nuttyDriverIR.initIRTx());
    ESP_ERROR_CHECK(nuttyDriverIR.initIRRx());
    //ESP_LOGI(TAG, "SD Init...");
    //ESP_ERROR_CHECK(nuttyDriverSDCard.initSDCard());
    //if(!nuttyDriverSDCard.isSDCardMounted()) {
    //    ESP_LOGI(TAG, "SD Mount...");
    //    ESP_ERROR_CHECK(nuttyDriverSDCard.mountSDCard());
    //}
    //if(nuttyDriverSDCard.isSDCardMounted()) {
    //    ESP_LOGI(TAG, "SD Unmount...");
    //    ESP_ERROR_CHECK(nuttyDriverSDCard.unmountSDCard());
    //}
    //ESP_LOGI(TAG, "SD Info...");
    //nuttyDriverSDCard.printSDCardInfo();

    // Nutty Services
    NuttyAudio_Init();
    NuttyAudio_SetVolume(16); // 16 = orig, > 16 = + x/16; < 16 = - x/16
    //NuttyAudio_PlayBuffer(ussr_start, 773508);
    //NuttyAudio_PlayBuffer(ussr_48k_start, 1160260);
    NuttyInput_Init();
    NuttyStorage_Init();
    NuttySystemMonitor_Init();
    NuttySystemMonitor_hideSystemTray();
    //NuttyRGB_Init();
    NuttyApps_Init();
    ESP_ERROR_CHECK(nuttyPeripherals.initGPIOISR(ioe_isr_handler, NULL)); // Must AFTER Nutty Services Start as task handle must available for ISR

    vTaskDelay(pdMS_TO_TICKS(1000));
    NuttyDisplay_clearWholeScreen();
    NuttySystemMonitor_showSystemTray();
    
    uint8_t duty=0;
    uint8_t x=0;
    int8_t vol=16;
    NuttyInput_clearButtonHoldState(0x1ff);
    char *text=(char *)malloc(22);
    lv_obj_t *_lbl = NULL;

    // Register NuttyApps
    NuttyApps_registerApp(NuttyMenu);
    NuttyApps_registerApp(NuttyTest);
    NuttyApps_registerApp(NuttySnake);
    NuttyApps_registerApp(NuttyAudioPlayer);
    NuttyApps_registerApp(NuttyRemote);
    NuttyApps_registerApp(NuttyRF);
    NuttyApps_registerApp(NuttyRGBControl);
    NuttyApps_registerApp(NuttyWifiScanner);
    NuttyApps_registerApp(NuttyBTRemote);
    NuttyApps_registerApp(NuttyCounter);
    NuttyApps_registerApp(NuttyBTScanner);
    NuttyApps_registerApp(NuttyStopwatch);
    NuttyApps_registerParamedApp(NuttyFileManager);
    NuttyApps_registerApp(NuttySettings);
    NuttyApps_registerApp(NuttyAbout);
    NuttyApps_printApps();
    NuttyApps_launchAppByIndex(0);

    return;
    while(true) {

       


        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        duty+=1;


        
        
        snprintf(text, 20, "Duty=%d", duty);
        lv_obj_t *parent = NuttyDisplay_getUserAppArea();
        NuttyDisplay_lockLVGL();
        if(_lbl != NULL) lv_obj_del(_lbl);
        _lbl = lv_label_create(parent);
        lv_label_set_text(_lbl, text);
        lv_obj_align(_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        NuttyDisplay_unlockLVGL();

        if(duty == 255) { 
            duty=0; 
            ESP_LOGI(TAG, "LEDC Done Cycle.");
            NuttyDisplay_clearUserAppArea();
            _lbl=NULL;
        }

        bool _played=false;
        NuttyAudio_FinishedPlayed(&_played);
        if(_played) { 
            if(x%2 == 0) {
                ESP_LOGI(TAG, "BEEP");
                NuttyAudio_PlayTone(1047, 10);NuttyAudio_FinishedPlayed(&_played);
            }else{
                ESP_LOGI(TAG, "USSR");
                NuttyAudio_PlayBuffer(ussr_48k_start, 1160260);
            }
            x++;
        }

        uint16_t addr, cmd;
        uint8_t status;
        if(NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_A)) {
            ESP_LOGI(TAG, "IRTx: %04X:%04X", addr, cmd);
            nuttyDriverIR.txIRNEC(addr, cmd);
        }
        if(NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_B)) {
            nuttyDriverIR.waitForIRNECRx(&addr, &cmd, &status, 1000);
            if(status == 2 || status == 4) {
                ESP_LOGI(TAG, "[%d] IRRx: %04X:%04X", status, addr, cmd);
            }else{
                addr = 0;
                cmd = 0;
            }
        }
        if(NuttyInput_isOneOfTheButtonsCurrentlyPressed(NUTTYINPUT_BTN_USRDEF)) {
            int batt = NuttySystemMonitor_getBatteryVoltage();
            ESP_LOGI(TAG, "BATT=%d; LV_BAT=%d; VBUS=%d; SDM=%d, SDCD=%d;", batt, NuttySystemMonitor_isLowBattery(), NuttySystemMonitor_isVBUSConnected(), NuttyStorage_isSDCardMounted(), NuttyStorage_isSDCardInserted());

        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_UP)) {
            vol += 1;
            ESP_LOGI(TAG, "Setting Volume = %d", vol);
            NuttyAudio_SetVolume(vol);
        }
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_DOWN)) {
            vol -=1;
            ESP_LOGI(TAG, "Setting Volume = %d", vol);
            NuttyAudio_SetVolume(vol);
        }


        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_START)) {
            ESP_LOGI(TAG, "Listing files");
            esp_err_t e = nuttyDriverSDCard.lsDir(SDCARD_MOUNT_POINT"/");
            printf("err=%d\n", e);
        }

        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(NUTTYINPUT_BTN_SELECT)) {
            heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
        }
        
        
        // Heap stuff
        //heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}