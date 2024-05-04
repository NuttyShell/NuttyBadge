#include "NuttyPeripherals.h"

static esp_err_t initLEDC() {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_LCD_BL_PWM_DUTYRES, // resolution of PWM duty
        .freq_hz = LEDC_LCD_BL_PWM_FREQ,                      // frequency of PWM signal
        .speed_mode = LEDC_LCD_BL_PWM_SPEED,           // timer mode
        .timer_num = LEDC_LCD_BL_PWM_TIMER,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer);


    ledc_channel_config_t ledc_ch = {
         .channel   = LEDC_LCD_BL_PWM_CHANNEL,
        .duty       = 0,
        .gpio_num   = (GPIO_LCD_BL_PWM),
        .speed_mode = LEDC_LCD_BL_PWM_SPEED,
        .hpoint     = 0,
        .timer_sel  = LEDC_LCD_BL_PWM_TIMER,
        .flags.output_invert = LEDC_LCD_BL_PWM_INVERT
    };
    ledc_channel_config(&ledc_ch);
    ledc_set_duty(LEDC_LCD_BL_PWM_SPEED, LEDC_LCD_BL_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_LCD_BL_PWM_SPEED, LEDC_LCD_BL_PWM_CHANNEL);
    return ESP_OK;
}

static esp_err_t initI2C() {
    i2c_config_t i2c_conf;
	i2c_conf.mode = I2C_MODE_MASTER;
	i2c_conf.sda_io_num = GPIO_I2C_SDA;
	i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_conf.scl_io_num = GPIO_I2C_SCL;
	i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	i2c_conf.master.clk_speed = 100000; // 100KHz
    i2c_conf.clk_flags = 0;
	i2c_param_config(I2C_NUM_0, &i2c_conf);
	i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0);
    return ESP_OK;
}

static esp_err_t initGPIO() {
    gpio_config_t io_conf = {};

    // OUTPUT Pins
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1ULL << GPIO_I2C_SCL) | (1ULL << GPIO_LCD_BL_PWM) | (1ULL << GPIO_LCD_A0) | (1ULL << GPIO_LCD_CS) | (1ULL << GPIO_FSPI_MOSI) | (1ULL << GPIO_FSPI_SCK) | (1ULL << GPIO_RGB_DATA) | (1ULL << GPIO_CCMOD_CS) | (1ULL << GPIO_AUDIO_OUT));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);


    // OUTPUT Pins with Pu
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1ULL << GPIO_IR_TX_SDCD));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Audio Out and IRTx High Drive Strength
    gpio_set_drive_capability(GPIO_AUDIO_OUT, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(GPIO_IR_TX_SDCD, GPIO_DRIVE_CAP_3);

    // INPUT Pins
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = ( (1ULL << GPIO_IR_RX) | (1ULL << GPIO_BATT_ADC) | (1ULL << GPIO_FSPI_MISO));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // OD Pins
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io_conf.pin_bit_mask = ((1ULL << GPIO_I2C_SDA) | (1ULL << GPIO_SD_CLK) | (1ULL << GPIO_SD_CMD) | (1ULL << GPIO_SD_DAT0));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // INTERRUPT Pins
    // CC GDO0 is not configured
    // TODO: If use CC GDO0 configure it here
    io_conf.intr_type = GPIO_IOE_INT_EDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_IOE_INT);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    return ESP_OK;
}

esp_err_t initGPIOISR(void (*ISR_HANDLER)(void *), void *ISR_ARGS) {
    ESP_ERROR_CHECK(gpio_install_isr_service(GPIO_IOE_INT_ISR));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_IOE_INT, ISR_HANDLER, ISR_ARGS));
    return ESP_OK;
}

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t cali_handle;
static bool use_cali_value=false;
static esp_err_t initADC() {
    esp_err_t err;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .chan = ADC_CHANNEL,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if(err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) return ESP_FAIL;
    if(err == ESP_OK) use_cali_value=true;

    return ESP_OK;
}

static void readADC(int *adc_raw, int *voltage_mv) {
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, adc_raw));
    if (use_cali_value) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, *adc_raw, voltage_mv));
        *voltage_mv = *voltage_mv ADC_MV_ADJUST;
    }else{
        *voltage_mv = (int)(((float)*adc_raw) * (float)2.247319)ADC_MV_ADJUST;
    }
}

static esp_err_t initFSPI() {
    esp_err_t err;
    spi_bus_config_t buscfg = {
        .miso_io_num = GPIO_FSPI_MISO,
        .mosi_io_num = GPIO_FSPI_MOSI,
        .sclk_io_num = GPIO_FSPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    err = spi_bus_initialize(FSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(err);
    return ESP_OK;
}

static esp_err_t initFSPIDevice(spi_device_handle_t *spi, int SPI_SPEED, uint8_t SPI_MODE, int SPI_QUEUE_SIZE, uint32_t SPI_FLAGS, int CS_PIN, void (*TRANSFER_CB)(spi_transaction_t *)) {
    esp_err_t err;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz  = SPI_SPEED,
        .mode = SPI_MODE, 
        .spics_io_num = CS_PIN,
        .queue_size = SPI_QUEUE_SIZE,
        .flags = SPI_FLAGS, 
        .pre_cb = TRANSFER_CB
    };
    err = spi_bus_add_device(FSPI_HOST, &devcfg, spi);
    ESP_ERROR_CHECK(err);
    return ESP_OK;
}

static bool SDHostInited = false;
static sdmmc_host_t sdmmc_host = SDMMC_HOST_DEFAULT();

static esp_err_t initSDHost() {
    if(SDHostInited) return ESP_OK;

    esp_err_t err;
    
    sdmmc_slot_config_t sdmmc_slot = SDMMC_SLOT_CONFIG_DEFAULT();
    sdmmc_host.max_freq_khz = 40000;
    sdmmc_slot.width=1;
    sdmmc_slot.clk = GPIO_SD_CLK;
    sdmmc_slot.cmd = GPIO_SD_CMD;
    sdmmc_slot.d0 = GPIO_SD_DAT0;

   
    err = (sdmmc_host.init)();
    ESP_ERROR_CHECK(err); // If error, we abort as the Host Init shouldn't fail
    SDHostInited = true;
    //If this failed (indicated by card_handle != -1), slot deinit needs to called()
    //leave card_handle as is to indicate that (though slot deinit not implemented yet.
    //err = init_sdmmc_host(sdmmc_host.slot, &sdmmc_host, &card_handle);
    err = sdmmc_host_init_slot(sdmmc_host.slot, (const sdmmc_slot_config_t*) &sdmmc_slot);
    ESP_ERROR_CHECK(err); // We fail by abort(), no deinit needed

    return ESP_OK;
}

static esp_err_t initSDCard(sdmmc_card_t *card) {
    return sdmmc_card_init(&sdmmc_host, card);
}

static esp_err_t initRMTTxDevice(rmt_channel_handle_t *rmt_channel, uint8_t gpio, size_t mem_blk_syms, uint32_t res_hz, rmt_carrier_config_t *carrier_cfg) {
    rmt_tx_channel_config_t tx_ch_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = mem_blk_syms,
        .resolution_hz = res_hz,
        .trans_queue_depth = 4
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_ch_cfg, rmt_channel));
    if (carrier_cfg != NULL) rmt_apply_carrier(*rmt_channel, carrier_cfg);
    return rmt_enable(*rmt_channel);
}

static esp_err_t initRMTRxDevice(rmt_channel_handle_t *rmt_channel, uint8_t gpio, size_t mem_blk_syms, uint32_t res_hz, QueueHandle_t *rx_queue, rmt_rx_event_callbacks_t *rx_cb) {
    rmt_rx_channel_config_t rx_ch_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = mem_blk_syms,
        .resolution_hz = res_hz
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_ch_cfg, rmt_channel));
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(*rmt_channel, rx_cb, *rx_queue));
    return rmt_enable(*rmt_channel);
}

NuttyPeripherals nuttyPeripherals = {
    .initLEDC = initLEDC,
    .initI2C = initI2C,
    .initGPIO = initGPIO,
    .initGPIOISR = initGPIOISR,
    .initFSPI = initFSPI,
    .initRMTTxDevice = initRMTTxDevice,
    .initRMTRxDevice = initRMTRxDevice,
    .initFSPIDevice = initFSPIDevice,
    .initADC = initADC,
    .readADC = readADC,
    .initSDHost = initSDHost,
    .initSDCard = initSDCard
};

