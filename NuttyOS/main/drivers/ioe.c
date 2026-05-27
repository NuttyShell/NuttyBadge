#include "ioe.h"

#include "esp_log.h"
#include "driver/i2c_master.h"

static i2c_master_dev_handle_t ioe_i2c;

// As IOE can be accessed by mutliple tasks
// We should accquire the complete control for the IOE first
static SemaphoreHandle_t ioe_semaphore;
void ioe_try_accquire_or_wait() {
	while(xSemaphoreTake(ioe_semaphore, (TickType_t)10) != pdTRUE);
}

// Release IOE hardware for other tasks to use
void ioe_release() {
	xSemaphoreGive(ioe_semaphore);
}

// Must record the current IOE output state
// We cannot rely on reading the IOE to read its state, for ex. When we drive a pin HIGH, it could be pulled low by a button
// So we must preserve its state whenever we write to IOE
static uint8_t IOEOutputState;
esp_err_t ioe_write(uint8_t data) {
	esp_err_t ret = i2c_master_transmit(ioe_i2c, &data, 1, portMAX_DELAY);
	if(ret == ESP_OK) IOEOutputState = data;
	return ret;
}

// Please lock the IOE first to ensure correct value
uint8_t ioe_get_output_state() {
	return IOEOutputState;
}

esp_err_t ioe_read(uint8_t *data) {
	return i2c_master_receive(ioe_i2c, data, 1, portMAX_DELAY);
}

esp_err_t ioe_init(void) {
	ESP_ERROR_CHECK(nuttyPeripherals.initI2CDevice(&ioe_i2c, IOE_I2C_ADDRESS, IOE_I2C_SPEED_HZ));
    ESP_ERROR_CHECK(ioe_write(0xff));
	ioe_semaphore = xSemaphoreCreateMutex();
	assert(ioe_semaphore != NULL);
    return ESP_OK;
}

NuttyDriverIOE nuttyDriverIOE = {
    .initIOE = ioe_init,
    .writeIOE = ioe_write,
    .readIOE = ioe_read,
	.lockIOE = ioe_try_accquire_or_wait,
	.releaseIOE = ioe_release,
	.getIOEOutputState = ioe_get_output_state
};