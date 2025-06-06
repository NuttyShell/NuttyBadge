#include "ir.h"


static rmt_channel_handle_t ir_tx_channel = NULL;
static rmt_encoder_handle_t ir_encoder = NULL;
static rmt_transmit_config_t ir_tx_config = {.loop_count = 0};
static SemaphoreHandle_t ir_tx_gpio_status_semaphore = NULL;
static bool ir_tx_gpio_read_mode = false;

static rmt_channel_handle_t ir_rx_channel = NULL;
static rmt_receive_config_t ir_rx_config = {.signal_range_min_ns = 1250, .signal_range_max_ns = 12000000};
static QueueHandle_t ir_rx_queue = NULL;
static bool ir_rx_started = false;
static rmt_symbol_word_t raw_symbols[64];
static rmt_rx_done_event_data_t rx_data;
static uint16_t ir_rx_nec_code_address;
static uint16_t ir_rx_nec_code_command;

static const char *TAG = "IR";

typedef struct {
    uint16_t address;
    uint16_t command;
} ir_nec_scan_code_t;

typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} ir_nec_encoder_config_t;

typedef struct {
    rmt_encoder_t base;           // the base "class", declares the standard encoder interface
    rmt_encoder_t *copy_encoder;  // use the copy_encoder to encode the leading and ending pulse
    rmt_encoder_t *bytes_encoder; // use the bytes_encoder to encode the address and command data
    rmt_symbol_word_t nec_leading_symbol; // NEC leading code with RMT representation
    rmt_symbol_word_t nec_ending_symbol;  // NEC ending code with RMT representation
    int state;
} rmt_ir_nec_encoder_t;

static size_t rmt_encode_ir_nec(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    ir_nec_scan_code_t *scan_code = (ir_nec_scan_code_t *)primary_data;
    rmt_encoder_handle_t copy_encoder = nec_encoder->copy_encoder;
    rmt_encoder_handle_t bytes_encoder = nec_encoder->bytes_encoder;
    switch (nec_encoder->state) {
    case 0: // send leading code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &nec_encoder->nec_leading_symbol,
                                                sizeof(rmt_symbol_word_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec_encoder->state = 1; // we can only switch to next state when current encoder finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space to put other encoding artifacts
        }
    // fall-through
    case 1: // send address
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, &scan_code->address, sizeof(uint16_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec_encoder->state = 2; // we can only switch to next state when current encoder finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space to put other encoding artifacts
        }
    // fall-through
    case 2: // send command
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, &scan_code->command, sizeof(uint16_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec_encoder->state = 3; // we can only switch to next state when current encoder finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space to put other encoding artifacts
        }
    // fall-through
    case 3: // send ending code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &nec_encoder->nec_ending_symbol,
                                                sizeof(rmt_symbol_word_t), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            nec_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space to put other encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_ir_nec_encoder(rmt_encoder_t *encoder)
{
    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    rmt_del_encoder(nec_encoder->copy_encoder);
    rmt_del_encoder(nec_encoder->bytes_encoder);
    free(nec_encoder);
    return ESP_OK;
}

static esp_err_t rmt_ir_nec_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_ir_nec_encoder_t *nec_encoder = __containerof(encoder, rmt_ir_nec_encoder_t, base);
    rmt_encoder_reset(nec_encoder->copy_encoder);
    rmt_encoder_reset(nec_encoder->bytes_encoder);
    nec_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_ir_encoder(const ir_nec_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder) {
    esp_err_t ret = ESP_OK;
    rmt_ir_nec_encoder_t *nec_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    nec_encoder = rmt_alloc_encoder_mem(sizeof(rmt_ir_nec_encoder_t));
    ESP_GOTO_ON_FALSE(nec_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for ir nec encoder");
    nec_encoder->base.encode = rmt_encode_ir_nec;
    nec_encoder->base.del = rmt_del_ir_nec_encoder;
    nec_encoder->base.reset = rmt_ir_nec_encoder_reset;

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &nec_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    // construct the leading code and ending code with RMT symbol format
    nec_encoder->nec_leading_symbol = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = 9000ULL * config->resolution / 1000000,
        .level1 = 0,
        .duration1 = 4500ULL * config->resolution / 1000000,
    };
    nec_encoder->nec_ending_symbol = (rmt_symbol_word_t) {
        .level0 = 1,
        .duration0 = 560 * config->resolution / 1000000,
        .level1 = 0,
        .duration1 = 0x7FFF,
    };

    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 560 * config->resolution / 1000000, // T0H=560us
            .level1 = 0,
            .duration1 = 560 * config->resolution / 1000000, // T0L=560us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 560 * config->resolution / 1000000,  // T1H=560us
            .level1 = 0,
            .duration1 = 1690 * config->resolution / 1000000, // T1L=1690us
        },
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &nec_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");

    *ret_encoder = &nec_encoder->base;
    return ESP_OK;
err:
    if (nec_encoder) {
        if (nec_encoder->bytes_encoder) {
            rmt_del_encoder(nec_encoder->bytes_encoder);
        }
        if (nec_encoder->copy_encoder) {
            rmt_del_encoder(nec_encoder->copy_encoder);
        }
        free(nec_encoder);
    }
    return ret;
}

static esp_err_t ir_tx_init() {
    rmt_carrier_config_t carrier_cfg = {
        .duty_cycle = 0.33,
        .frequency_hz = 38000 // 38KHz
    };
    nuttyPeripherals.initRMTTxDevice(&ir_tx_channel, GPIO_IR_TX_SDCD, RMT_IRTX_MEM_BLK_SYMBOLS, RMT_IRTX_RESOLUTION_HZ, &carrier_cfg);
    
    ir_nec_encoder_config_t encoder_config = {
        .resolution = RMT_IRTX_RESOLUTION_HZ,
    };

    if(ir_tx_gpio_status_semaphore == NULL) ir_tx_gpio_status_semaphore = xSemaphoreCreateMutex();
    assert(ir_tx_gpio_status_semaphore != NULL);
    
    return rmt_ir_encoder(&encoder_config, &ir_encoder);
}

static bool ir_rx_recv_done_cb(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data) {
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

static esp_err_t ir_rx_init() {
    ir_rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(ir_rx_queue != NULL);
    rmt_rx_event_callbacks_t ir_rx_cb = {
        .on_recv_done = ir_rx_recv_done_cb,
    };
    return nuttyPeripherals.initRMTRxDevice(&ir_rx_channel, GPIO_IR_RX, RMT_IRRX_MEM_BLK_SYMBOLS, RMT_IRRX_RESOLUTION_HZ, &ir_rx_queue, &ir_rx_cb);
}

static esp_err_t ir_tx_necext(uint16_t address, uint16_t command) {
    bool needWait = true;
    while(needWait) {
        xSemaphoreTake(ir_tx_gpio_status_semaphore, portMAX_DELAY);
        needWait = ir_tx_gpio_read_mode;
        xSemaphoreGive(ir_tx_gpio_status_semaphore);
        vTaskDelay(pdTICKS_TO_MS(1));
    }
    ir_nec_scan_code_t code = {.address = address, .command = command};
    int rmt_cid;
    ESP_ERROR_CHECK(rmt_get_channel_id(ir_tx_channel, &rmt_cid));
    esp_rom_gpio_connect_out_signal(GPIO_IR_TX_SDCD, rmt_periph_signals.groups[0].channels[rmt_cid].tx_sig, false, false);
    ESP_ERROR_CHECK(gpio_set_drive_capability(GPIO_IR_TX_SDCD, GPIO_DRIVE_CAP_3));
    return rmt_transmit(ir_tx_channel, ir_encoder, &code, sizeof(code), &ir_tx_config);
}

static esp_err_t ir_tx_nec(uint8_t address, uint8_t command) {
    return ir_tx_necext(((((uint8_t)~address) << 8) | address), ((((uint8_t)~command) << 8) | command));
}

static void ir_tx_wait_finish_and_accquire_line_for_read() {
    xSemaphoreTake(ir_tx_gpio_status_semaphore, portMAX_DELAY);
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(ir_tx_channel, portMAX_DELAY));
    ir_tx_gpio_read_mode = true;
    xSemaphoreGive(ir_tx_gpio_status_semaphore);
}

static void ir_tx_release_line() {
    xSemaphoreTake(ir_tx_gpio_status_semaphore, portMAX_DELAY);
    ir_tx_gpio_read_mode = false;
    xSemaphoreGive(ir_tx_gpio_status_semaphore);
}

/**
 * @brief Check whether a duration is within expected range
 */
static inline bool nec_check_in_range(uint32_t signal_duration, uint32_t spec_duration) {
    return (signal_duration < (spec_duration + RMT_IRRX_NEC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - RMT_IRRX_NEC_DECODE_MARGIN));
}

/**
 * @brief Check whether a RMT symbol represents NEC logic zero
 */
static bool nec_parse_logic0(rmt_symbol_word_t *rmt_nec_symbols) {
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ZERO_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ZERO_DURATION_1);
}

/**
 * @brief Check whether a RMT symbol represents NEC logic one
 */
static bool nec_parse_logic1(rmt_symbol_word_t *rmt_nec_symbols) {
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_PAYLOAD_ONE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_PAYLOAD_ONE_DURATION_1);
}


/**
 * @brief Decode RMT symbols into NEC address and command
 */
static bool nec_parse_frame(rmt_symbol_word_t *rmt_nec_symbols) {
    rmt_symbol_word_t *cur = rmt_nec_symbols;
    uint16_t address = 0;
    uint16_t command = 0;
    bool valid_leading_code = nec_check_in_range(cur->duration0, NEC_LEADING_CODE_DURATION_0) &&
                              nec_check_in_range(cur->duration1, NEC_LEADING_CODE_DURATION_1);
    if (!valid_leading_code) {
        return false;
    }
    cur++;
    for (int i = 0; i < 16; i++) {
        if (nec_parse_logic1(cur)) {
            address |= 1 << i;
        } else if (nec_parse_logic0(cur)) {
            address &= ~(1 << i);
        } else {
            return false;
        }
        cur++;
    }
    for (int i = 0; i < 16; i++) {
        if (nec_parse_logic1(cur)) {
            command |= 1 << i;
        } else if (nec_parse_logic0(cur)) {
            command &= ~(1 << i);
        } else {
            return false;
        }
        cur++;
    }
    // save address and command
    ir_rx_nec_code_address = address;
    ir_rx_nec_code_command = command;
    return true;
}

/**
 * @brief Check whether the RMT symbols represent NEC repeat code
 */
static bool nec_parse_frame_repeat(rmt_symbol_word_t *rmt_nec_symbols)
{
    return nec_check_in_range(rmt_nec_symbols->duration0, NEC_REPEAT_CODE_DURATION_0) &&
           nec_check_in_range(rmt_nec_symbols->duration1, NEC_REPEAT_CODE_DURATION_1);
}

// status:
// 0 = no signal (timeout)
// 1 = unknown nec frame
// 2 = normal nec frame
// 3 = undecodable nec frame
// 4 = repeat nec frame
static void ir_rx_nec_wait(uint16_t *address, uint16_t *command, uint8_t *status, uint16_t ms_to_wait) {
    if(!ir_rx_started) {
        ESP_ERROR_CHECK(rmt_receive(ir_rx_channel, raw_symbols, sizeof(raw_symbols), &ir_rx_config));
        ir_rx_started = true;
    }
    //xQueueReset(ir_rx_queue);
    if(xQueueReceive(ir_rx_queue, &rx_data, pdMS_TO_TICKS(ms_to_wait)) == pdPASS) {
        //printf("---NEC FRAME BEGIN---\n");
        //for(size_t i=0; i<rx_data.num_symbols; i++) {
        //    printf("{%d:%d},{%d:%d}\r\n", rx_data.received_symbols[i].level0, rx_data.received_symbols[i].duration0, rx_data.received_symbols[i].level1, rx_data.received_symbols[i].duration1);
        //}
        //printf("---NEC FRAME STOPS---\n");
        switch(rx_data.num_symbols) {
        case 34:
            if(nec_parse_frame(rx_data.received_symbols)) {
                *address = ir_rx_nec_code_address;
                *command = ir_rx_nec_code_command;
                *status = 2;
            }else{
                *status = 3;
            }
            break;
        case 2:
            if(nec_parse_frame_repeat(rx_data.received_symbols)) {
                *address = ir_rx_nec_code_address;
                *command = ir_rx_nec_code_command;
                *status = 4;
            }else{
                *status = 3;
            }
            break;
        default:
            *status = 1;
            printf("NEC: Unknown\n");
            break;
        }
        ir_rx_started=false;
    }else{
        // Timeout receiving NEC Packet
        *status = 0;
    }
}

NuttyDriverIR nuttyDriverIR = {
    .initIRTx = ir_tx_init,
    .initIRRx = ir_rx_init,
    .txIRNECext = ir_tx_necext,
    .txIRNEC = ir_tx_nec,
    .txIRWaitFinishAndReserveLine = ir_tx_wait_finish_and_accquire_line_for_read,
    .txIRReleaseLine = ir_tx_release_line,
    .waitForIRNECRx = ir_rx_nec_wait
};