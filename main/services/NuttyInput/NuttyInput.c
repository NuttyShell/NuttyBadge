#include "NuttyInput.h"
#include "services/NuttyDisplay/NuttyDisplay.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x0100 ? '1' : '0'), \
  ((byte) & 0x0080 ? '1' : '0'), \
  ((byte) & 0x0040 ? '1' : '0'), \
  ((byte) & 0x0020 ? '1' : '0'), \
  ((byte) & 0x0010 ? '1' : '0'), \
  ((byte) & 0x0008 ? '1' : '0'), \
  ((byte) & 0x0004 ? '1' : '0'), \
  ((byte) & 0x0002 ? '1' : '0'), \
  ((byte) & 0x0001 ? '1' : '0') 

static const char* TAG = "NuttyInput";

static volatile TaskHandle_t WorkerHandle;

void IRAM_ATTR ioe_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(WorkerHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static uint16_t btnPressedDebounced=0; // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
static uint16_t btnHeldDebounced=0; // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
static uint16_t btnHeldAndReleasedDebounced=0; // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
static void NuttyInput_Worker(void* arg) {
    uint8_t ioeReadout, i;
    uint16_t btnPressed; // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
    uint32_t debouncePressed[9]; // If overflow then just roll over...
    uint32_t debounceUnpressed[9]; // If overflow then just roll over...
    uint32_t holdCounter[9]; // If overflow then just roll over...
    for (;;) {
        //printf("Wait...");
        btnPressedDebounced = 0;
        for(i=0; i<9; i++) debouncePressed[i]=0;
        for(i=0; i<9; i++) debounceUnpressed[i]=0;
        for(i=0; i<9; i++) holdCounter[i]=0;
        xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY); // Block until interrupt
        // *ANY* Button is pressed
        btnPressed = 1;
        while(btnPressed) {
            btnPressed=0;
            nuttyDriverIOE.lockIOE();
            nuttyDriverIOE.writeIOE(0b11111011);
            nuttyDriverIOE.readIOE(&ioeReadout);
            btnPressed |= ((~ioeReadout & 0b00111000) << 3);
            //if(~ioeReadout & (1 << 5)) printf("UDEF\n");
            //if(~ioeReadout & (1 << 4)) printf("START\n");
            //if(~ioeReadout & (1 << 3)) printf("SELECT\n");
            nuttyDriverIOE.writeIOE(0b11111101);
            nuttyDriverIOE.readIOE(&ioeReadout);
            btnPressed |= ((~ioeReadout & 0b00111000));
            //if(~ioeReadout & (1 << 5)) printf("BBBB\n");
            //if(~ioeReadout & (1 << 4)) printf("AAAA\n");
            //if(~ioeReadout & (1 << 3)) printf("RIGHT\n");
            nuttyDriverIOE.writeIOE(0b11111110);
            nuttyDriverIOE.readIOE(&ioeReadout);
            btnPressed |= ((~ioeReadout & 0b00111000) >> 3);
            nuttyDriverIOE.releaseIOE();
            //if(~ioeReadout & (1 << 5)) printf("LEFT\n");
            //if(~ioeReadout & (1 << 4)) printf("DOWN\n");
            //if(~ioeReadout & (1 << 3)) printf("UP\n");
            //printf("P:" BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(btnPressed));

            // Handles Debouncing
            for (i=0; i<9; i++) {
                if((btnPressed & (1 << i))) {
                    debouncePressed[i]++; 
                }else{
                    debounceUnpressed[i]++;
                }
                if(debouncePressed[i] > BTN_DEBOUNCE_PRESSED_THRESHOLD) {
                    btnPressedDebounced |= (1 << i);
                    debouncePressed[i]=0;
                }
                if(debounceUnpressed[i] > BTN_DEBOUNCE_NOTPRESSED_THRESHOLD) {
                    btnPressedDebounced &= ~(1 << i);
                    debounceUnpressed[i]=0;
                }
            }

            // Handles Button Hold Detection
            for(i=0; i<9; i++) {
                if(!(btnPressedDebounced & (1 << i))) holdCounter[i] = 0;
                if((btnPressedDebounced & (1 << i))) {
                    holdCounter[i]++;
                }
                if(holdCounter[i] > BTN_DEBOUNCE_PRESSED_THRESHOLD) { // "Double" debounce
                    btnHeldDebounced |= (1 << i);
                }else if(holdCounter[i] <= BTN_DEBOUNCE_PRESSED_THRESHOLD && !(btnHeldDebounced & (1 << i))){
                    btnHeldDebounced &= ~(1 << i);
                }
            }
        }
        nuttyDriverIOE.lockIOE();
        nuttyDriverIOE.writeIOE(0b11111000); // Nothing pressed, put into "reall all buttons" mode and wait for interrupt
        nuttyDriverIOE.releaseIOE();
    }
}

// Check if one of the given button(s) is/are pressed at this exact moment
bool NuttyInput_isOneOfTheButtonsCurrentlyPressed(uint16_t btns) {
    return ((btnPressedDebounced & btns) != 0x00);
}

// Wait one of the given button(s) is/are pressed at this exact moment
void NuttyInput_waitOneOfTheButtonsPressed(uint16_t btns) {
    while(!(btnPressedDebounced & btns)) {
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

// Wait all the specified buttons are released
void NuttyInput_waitAllOfTheButtonsNotPressed(uint16_t btns) {
    while((btnPressedDebounced & btns)) {
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

// Wait the given button are being pressed and released (blocking)
void NuttyInput_waitSingleButtonHoldAndReleasedBlocking(uint16_t btn) {
    uint32_t holdCounter=0;
    while(1) {
        NuttyInput_waitOneOfTheButtonsPressed(btn);
        if(NuttyInput_isOneOfTheButtonsCurrentlyPressed(btn)) {
            holdCounter++;
            if(holdCounter == BTN_HOLD_THRESHOLD) {
                NuttyInput_waitAllOfTheButtonsNotPressed(btn);
                return;
            }
        }else{
            holdCounter=0;
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }

}

// Clear button held state, used with `NuttyInput_waitSingleButtonHoldAndReleasedNonBlock`
void NuttyInput_clearButtonHoldState(uint16_t btn) {
    btnHeldDebounced &= ~btn;
}

// Wait the given button is being pressed and released (non-blocking)
bool NuttyInput_waitSingleButtonHoldAndReleasedNonBlock(uint16_t btn) {
    if(btnHeldDebounced & btn) {
        if(!(btnPressedDebounced & btn)) {
            NuttyInput_clearButtonHoldState(btn);
            return true;
        }
    }
    return false;
}

static lv_indev_drv_t lvgl_indev_drv;
static lv_indev_t *lvgl_indev;
static NuttyInputLVGLInputMapping lvgl_mapping;
static SemaphoreHandle_t lvglInputSemaphore = NULL;

static void lvgl_keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    static uint32_t last_key = 0;

    /*Get whether the a key is pressed and save the pressed key*/
    uint16_t keyPress = 0x00;
    for(uint8_t i=0; i<9; i++) {
        if(NuttyInput_waitSingleButtonHoldAndReleasedNonBlock((1 << i)) == true) keyPress |= (1<<i);
    }


    if(keyPress != 0) {
        data->state = LV_INDEV_STATE_PRESSED;

        /*Translate the keys to LVGL control characters according to your key definitions*/
        while(xSemaphoreTake(lvglInputSemaphore, portMAX_DELAY) != pdTRUE);
        switch(keyPress) {
            case 1 << 0:
                keyPress = lvgl_mapping.UP;
                break;
            case 1 << 1:
                keyPress = lvgl_mapping.DOWN;
                break;
            case 1 << 2:
                keyPress = lvgl_mapping.LEFT;
                break;
            case 1 << 3:
                keyPress = lvgl_mapping.RIGHT;
                break;
            case 1 << 4:
                keyPress = lvgl_mapping.A;
                break;
            case 1 << 5:
                keyPress = lvgl_mapping.B;
                break;
            case 1 << 6:
                keyPress = lvgl_mapping.SELECT;
                break;
            case 1 << 7:
                keyPress = lvgl_mapping.START;
                break;
            case 1 << 8:
                keyPress = lvgl_mapping.USRDEF;
                break;
        }
        xSemaphoreGive(lvglInputSemaphore);
        NuttyInput_clearButtonHoldState(0x1ff);
        last_key = keyPress;
    }else{
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->key = last_key;
}

lv_indev_t* NuttyInput_UpdateLVGLInDev(NuttyInputLVGLInputMapping mapping) {
    while(xSemaphoreTake(lvglInputSemaphore, portMAX_DELAY) != pdTRUE);
    lvgl_mapping = mapping;
    xSemaphoreGive(lvglInputSemaphore);
    return lvgl_indev;
}

    

static void NuttyInput_UART(void *arg) {

        uint8_t buf[2];
        uint8_t i=0;
    while(1) {
        //printf("Waiting to hold A...\n");
        //waitSingleButtonHoldAndReleased(NUTTYINPUT_BTN_A);
        //waitOneOfTheButtonsPressed(0x01ff);
        //printf("%02x", btnPressedDebounced);
        //printf("OK!\n");

        uint8_t ch = fgetc(stdin);
        bool reset=true;
        if(ch != 0xff) {
            if(i==0 && ch == 'a') { 
                btnPressedDebounced |= (1 << 4); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                btnHeldDebounced |= (1 << 4); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnPressedDebounced &= ~(1 << 4); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnHeldDebounced &= ~(1 << 4); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
            }else if(i==0 && ch == 'b') {
                btnPressedDebounced |= (1 << 5); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                btnHeldDebounced |= (1 << 5); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnPressedDebounced &= ~(1 << 5); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnHeldDebounced &= ~(1 << 5); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
            }else if(i==0 && ch == 'x') {
                btnPressedDebounced |= (1 << 8); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                btnHeldDebounced |= (1 << 8); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnPressedDebounced &= ~(1 << 8); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                vTaskDelay(pdMS_TO_TICKS(10));
                btnHeldDebounced &= ~(1 << 8); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
            }else if(i==0 && ch == 'A') {
                print_lcd_frame_buffer();
            }else{
                if(i==0 && ch == 0x1b) { buf[i]=ch; i++; reset=false; }
                if(i==1 && ch == 0x5b) { buf[i]=ch; i++; reset=false; }
                if(i==2) {
                    if(ch == 0x41) { // Arrow key UP
                        btnPressedDebounced |= (1 << 0);  // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        btnHeldDebounced |= (1 << 0); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnPressedDebounced &= ~(1 << 0); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnHeldDebounced &= ~(1 << 0); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                    }else if(ch == 0x42) {  // Arrow key DOWN
                        btnPressedDebounced |= (1 << 1);  // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        btnHeldDebounced |= (1 << 1); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnPressedDebounced &= ~(1 << 1); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnHeldDebounced &= ~(1 << 1); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                    }else if(ch == 0x43) {  // Arrow key RIGHT
                        btnPressedDebounced |= (1 << 3);  // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        btnHeldDebounced |= (1 << 3); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnPressedDebounced &= ~(1 << 3); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnHeldDebounced &= ~(1 << 1); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                    }else if(ch == 0x44) {  // Arrow key LEFT
                        btnPressedDebounced |= (1 << 2); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        btnHeldDebounced |= (1 << 2); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnPressedDebounced &= ~(1 << 2); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                        vTaskDelay(pdMS_TO_TICKS(10));
                        btnHeldDebounced &= ~(1 << 2); // {0000000 USRDEF START SELECT B A RIGHT LEFT DOWN UP}
                    }
                }

            }
            if(reset) {
                i=0;
                memset(buf, 0x00, 2);
            }
        }

        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

esp_err_t NuttyInput_Init() {
    ESP_LOGI(TAG, "Init");
    xTaskCreate(NuttyInput_Worker, "input_worker", 2560, NULL, 10, &WorkerHandle);
    xTaskCreate(NuttyInput_UART, "input_uart", 4096, NULL, 10, NULL);
    assert(WorkerHandle != NULL);
    nuttyDriverIOE.lockIOE();
    ESP_ERROR_CHECK(nuttyDriverIOE.writeIOE(0b11111000));
    nuttyDriverIOE.releaseIOE();

    // LVGL Input stuff
    lvglInputSemaphore = xSemaphoreCreateMutex();
    assert(lvglInputSemaphore != NULL);
    while(xSemaphoreTake(lvglInputSemaphore, portMAX_DELAY) != pdTRUE);
    lv_indev_drv_init(&lvgl_indev_drv);
    lvgl_indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    lvgl_indev_drv.read_cb = lvgl_keypad_read;
    lvgl_indev = lv_indev_drv_register(&lvgl_indev_drv);
    assert(lvgl_indev != NULL);
    xSemaphoreGive(lvglInputSemaphore);

    return ESP_OK;
}