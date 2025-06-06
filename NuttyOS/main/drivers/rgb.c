#include "rgb.h"


static rmt_channel_handle_t rgb_channel = NULL;
static uint8_t *rgb_pixels = NULL;
static rmt_encoder_handle_t rgb_encoder = NULL;
static rmt_transmit_config_t rgb_tx_config = {.loop_count = 0};
static uint8_t rgb_pixels_count;
static const char *TAG = "RGB";

// Converts HSV Values to RGB Values
static void rgb_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b) {
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    switch (led_encoder->state) {
    case 0: // send RGB data
        encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = 1; // switch to next state when current encoding session finished
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    // fall-through
    case 1: // send reset code
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                sizeof(led_encoder->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
            state |= RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            state |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_rgb_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_encoder = rmt_alloc_encoder_mem(sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for rgb encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del = rmt_del_led_strip_encoder;
    led_encoder->base.reset = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1 = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1 = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
    led_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

static void rgb_set_pixel_rgb_without_display(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    rgb_pixels[pixel * 3 + 0] = g >> RGB_SHIFT_BITS;
    rgb_pixels[pixel * 3 + 1] = r >> RGB_SHIFT_BITS;
    rgb_pixels[pixel * 3 + 2] = b >> RGB_SHIFT_BITS;
}

static void rgb_set_pixel_hsv_without_display(uint8_t pixel, uint8_t h, uint8_t s, uint8_t v) {
    uint32_t r, g, b;
    rgb_hsv2rgb(h, s, v, &r, &g, &b);
    rgb_set_pixel_rgb_without_display(pixel, r, g, b);
}

static esp_err_t rgb_display_now() {
    // Flush RGB values to LEDs
    ESP_ERROR_CHECK(rmt_transmit(rgb_channel, rgb_encoder, rgb_pixels, sizeof(uint8_t) * rgb_pixels_count * 3, &rgb_tx_config));
    return rmt_tx_wait_all_done(rgb_channel, portMAX_DELAY);
}

static esp_err_t rgb_set_pixel_rgb_and_display_now(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    rgb_set_pixel_rgb_without_display(pixel, r, g, b);
    return rgb_display_now();
}

static esp_err_t rgb_set_pixel_hsv_and_display_now(uint8_t pixel, uint8_t h, uint8_t s, uint8_t v) {
    rgb_set_pixel_hsv_without_display(pixel, h, s, v);
    return rgb_display_now();
}


static esp_err_t rgb_init(uint8_t pixels) {
    nuttyPeripherals.initRMTTxDevice(&rgb_channel, GPIO_RGB_DATA, RMT_RGB_MEM_BLK_SYMBOLS, RMT_RGB_RESOLUTION_HZ, NULL);
    if(rgb_pixels != NULL) { free(rgb_pixels); rgb_pixels = NULL; }
    rgb_pixels = (uint8_t*) malloc(pixels * 3 * sizeof(uint8_t));
    assert(rgb_pixels != NULL);
    rgb_pixels_count = pixels;
    
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_RGB_RESOLUTION_HZ,
    };
    
    return rmt_rgb_encoder(&encoder_config, &rgb_encoder);
}

NuttyDriverRGB nuttyDriverRGB = {
    .initRGB = rgb_init,
    .setRGBWithoutDisplay = rgb_set_pixel_rgb_without_display,
    .setHSVWithoutDisplay = rgb_set_pixel_hsv_without_display,
    .setRGBAndDisplay = rgb_set_pixel_rgb_and_display_now,
    .setHSVAndDisplay = rgb_set_pixel_hsv_and_display_now,
    .displayNow = rgb_display_now
};