#include "lcd.h"

static spi_device_handle_t lcd_spi;

static void lcd_reset(void) {
    uint8_t ioeState = 0;
    nuttyDriverIOE.lockIOE();
    ioeState = nuttyDriverIOE.getIOEOutputState();
    nuttyDriverIOE.writeIOE(ioeState | 0x80); // Write 1 to LCD reset PIN
    nuttyDriverIOE.releaseIOE();
    vTaskDelay(pdMS_TO_TICKS(20));
    nuttyDriverIOE.lockIOE();
    ioeState = nuttyDriverIOE.getIOEOutputState();
    nuttyDriverIOE.writeIOE(ioeState & ~0x80); // Write 0 to LCD reset PIN
    nuttyDriverIOE.releaseIOE();
    vTaskDelay(pdMS_TO_TICKS(50));
    nuttyDriverIOE.lockIOE();
    ioeState = nuttyDriverIOE.getIOEOutputState();
    nuttyDriverIOE.writeIOE(ioeState | 0x80); // Write 1 to LCD reset PIN
    nuttyDriverIOE.releaseIOE();
    vTaskDelay(pdMS_TO_TICKS(20));
}

void lcd_cmd(const uint8_t cmd, bool keep_cs_active) {
    spi_transaction_t t;
    memset(&t, 0x00, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd; // A0=0
    t.user = (void *)0;
    if(keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
    }
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

void lcd_data(uint8_t *data, size_t len) {
    spi_transaction_t t;
    if(len == 0) return; 
    memset(&t, 0x00, sizeof(t));
    t.length = len*8;
    t.tx_buffer = data;
    t.user = (void *)1;
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

void lcd_spi_pre_transfer_cb(spi_transaction_t *t) {
    gpio_set_level(GPIO_LCD_A0, (int)t->user);
}

void lcd_set_page(uint8_t page) {
    lcd_cmd(0xb0 + page, false);
}

void lcd_set_column(uint8_t column) {
    column += 1;
    lcd_cmd((0x10 | (column >> 4)), false);
    lcd_cmd((0x00 | (column & 0x0f)), false);
}

void lcd_clear() {
    uint8_t page=0, column = 0;
    for(page = 0; page<8; page++) {
        lcd_set_page(page);
        lcd_set_column(0);
        for(column = 0; column<128; column ++) {
            uint8_t d=0x00;
            lcd_data(&d, 1);
        }
    }
}

void lcd_full() {
    uint8_t page=0, column = 0;
    for(page = 0; page<8; page++) {
        lcd_set_page(page);
        lcd_set_column(0);
        for(column = 0; column<128; column ++) {
            uint8_t d=0x10;
            lcd_data(&d, 1);
        }
    }
}


static void lcd_update_page(uint8_t page, uint8_t *data, size_t datasz) {
    lcd_set_page(page);
    lcd_set_column(0);
    lcd_data(data, datasz);
}

// LCD arranged (vertically): Page 4,5,6,7,0,1,2,3...
static void lcd_update_page_seq_order(uint8_t page, uint8_t *data, size_t datasz) {
    uint8_t reordered_page;
    if(page >= 4) {
        reordered_page = page-4;
    }else{
        reordered_page = page+4;
    }
    lcd_set_page(reordered_page);
    lcd_set_column(0);
    lcd_data(data, datasz);
}

void lcd_pic(uint8_t *pic, size_t sz) {
    uint8_t *p = pic;
    uint8_t page=0, column = 0;
    for(page = 0; page<8; page++) {
        lcd_set_page((page + 4) % 8);
        lcd_set_column(0);
        for(column = 0; column<128; column ++) {
            lcd_data(p, 128);
            p += 128;
        }
    }
}

void lcd_set_backlight_duty(uint32_t duty) {
    ledc_set_duty(LEDC_LCD_BL_PWM_SPEED, LEDC_LCD_BL_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LCD_BL_PWM_SPEED, LEDC_LCD_BL_PWM_CHANNEL);
}

 static uint8_t lcd_init_sequence [14] =
{
        0xa2,//bias set:1/9
     0xa0,//ADC select:nomal,0->128
     0xa6,//normal/reverse  display
     0xc8,//commom output mode select: reverse
     0xf8,//booster ratio set
     0x00,//booster ratio:4x
     0x2f,//power control set
     0x26,//set (Rb/Ra)
     0x81,//vo output voltage set
     0x09,//voltage volume =9.0v
    0xA0, //列扫描顺序：从左到右
    0xc8, //行扫描顺序：反序
     0x60,//set start line
     0xaf,//display on  
};

esp_err_t lcd_init(void) {
    ESP_ERROR_CHECK(nuttyPeripherals.initFSPIDevice(&lcd_spi, LCD_SPEED_HZ, 0, 7, SPI_DEVICE_NO_DUMMY, GPIO_LCD_CS, lcd_spi_pre_transfer_cb));
    lcd_reset();


    lcd_cmd(0xe2, false); // system reset
	lcd_cmd(0x40, false); // set LCD start line to 0
	lcd_cmd(0xa0, false); // set SEG direction (A1 to flip horizontal)
	lcd_cmd(0xc8, false); // set COM direction (C0 to flip vert)
	lcd_cmd(0xa2, false); // set LCD bias mode 1/9
	lcd_cmd(0x2c, false); // set boost on
	lcd_cmd(0x2e, false); // set voltage regulator on
	lcd_cmd(0x2f, false); // Voltage follower on
	lcd_cmd(0xf8, false); // set booster ratio to
	lcd_cmd(0x00, false); // 4x
	lcd_cmd(0x23, false); // set resistor ratio = 4
	lcd_cmd(0x81, false);
	lcd_cmd(0x28, false); // set contrast = 40
	lcd_cmd(0xac, false); // set static indicator off
	lcd_cmd(0x00, false);
	lcd_cmd(0xa6, false); // disable inverse
	lcd_cmd(0xaf, false); // enable display
    
    uint8_t i=0;
    //for(i=0; i<14; i++) lcd_cmd(lcd_init_sequence[i], false);
    vTaskDelay(pdMS_TO_TICKS(100));
	lcd_cmd(0xa5, false); // display all points
    vTaskDelay(pdMS_TO_TICKS(200));
	lcd_cmd(0xa4, false); // normal display

    lcd_clear();
    //lcd_pic(picture, 128*64);

    uint8_t *_tmp = malloc(128);
    memset(_tmp, 0xff, 128);
    //lcd_update_page(4, _tmp, 128);

    //lcd_update_page_seq_order(7, _tmp, 20);
    return ESP_OK;
}



NuttyDriverLCD nuttyDriverLCD = {
    .initLCD = lcd_init,
    .updatePage = lcd_update_page,
    .updatePageSeqOrder = lcd_update_page_seq_order,
    .setBacklightDutyCycle = lcd_set_backlight_duty,
};