#include "ioe.h"

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
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	ESP_ERROR_CHECK(i2c_master_start(cmd));
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, ((IOE_I2C_ADDRESS) << 1)|I2C_MASTER_WRITE, true)); // Write to I2C Address
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, data, true)); // Write Register data
	ESP_ERROR_CHECK(i2c_master_stop(cmd));
	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 0);
	i2c_cmd_link_delete(cmd);

	IOEOutputState = data;

	return ret;
}

// Please lock the IOE first to ensure correct value
uint8_t ioe_get_output_state() {
	return IOEOutputState;
}

esp_err_t ioe_read(uint8_t *data) {
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	ESP_ERROR_CHECK(i2c_master_start(cmd));
	ESP_ERROR_CHECK(i2c_master_write_byte(cmd, ((IOE_I2C_ADDRESS) << 1)|I2C_MASTER_READ, true)); // Write to I2C Address
	ESP_ERROR_CHECK(i2c_master_read_byte(cmd, data, true)); // Write Register data
	ESP_ERROR_CHECK(i2c_master_stop(cmd));
	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 0);
	i2c_cmd_link_delete(cmd);

	return ret;
}

esp_err_t ioe_init(void) {
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