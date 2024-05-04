#include "NuttyInput.h"

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

// Check if one of the given button(s) is/are pressed
bool isOneOfTheButtonsPressed(uint16_t btns) {
    return ((btnPressedDebounced & btns) != 0x00);
}

// Wait one of the given button(s) is/are pressed
void waitOneOfTheButtonsPressed(uint16_t btns) {
    while(!(btnPressedDebounced & btns)) {
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void waitAllOfTheButtonsNotPressed(uint16_t btns) {
    while((btnPressedDebounced & btns)) {
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

// Wait the given button are being pressed and released
void waitSingleButtonHoldAndReleased(uint16_t btn) {
    uint32_t holdCounter=0;
    while(1) {
        waitOneOfTheButtonsPressed(btn);
        if(isOneOfTheButtonsPressed(btn)) {
            holdCounter++;
            if(holdCounter == BTN_HOLD_THRESHOLD) {
                waitAllOfTheButtonsNotPressed(btn);
                return;
            }
        }else{
            holdCounter=0;
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }

}

// Clear button held state
void clearButtonHoldState(uint16_t btn) {
    btnHeldDebounced &= ~btn;
}

// Wait the given button is being pressed and released (non-blocking)
bool waitSingleButtonHoldAndReleasedNonBlock(uint16_t btn) {
    if(btnHeldDebounced & btn) {
        if(!(btnPressedDebounced & btn)) {
            clearButtonHoldState(btn);
            return true;
        }
    }
    return false;
}

static void NuttyInput_Test(void *arg) {
    while(1) {
        //printf("Waiting to hold A...\n");
        //waitSingleButtonHoldAndReleased(NUTTYINPUT_BTN_A);
        //waitOneOfTheButtonsPressed(0x01ff);
        //printf("%02x", btnPressedDebounced);
        //printf("OK!\n");
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

esp_err_t NuttyInput_Init() {
    ESP_LOGI(TAG, "Init");
    xTaskCreate(NuttyInput_Worker, "input_worker", 2560, NULL, 10, &WorkerHandle);
    xTaskCreate(NuttyInput_Test, "input_tester", 2048, NULL, 10, NULL);
    assert(WorkerHandle != NULL);
    nuttyDriverIOE.lockIOE();
    ESP_ERROR_CHECK(nuttyDriverIOE.writeIOE(0b11111000));
    nuttyDriverIOE.releaseIOE();
    return ESP_OK;
}