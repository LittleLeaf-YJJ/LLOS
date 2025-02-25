#include <llos.h>
#include "hal_i2c.h"
#include "i2c.h"

static volatile bool busyFlag;

static ll_err_t HW_I2C2_Write(ll_device_list_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);
static ll_err_t HW_I2C2_Read(ll_device_list_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);
static ll_err_t HW_I2C2_DMAWrite(ll_device_list_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);

void LLOS_Device_Register_I2C(void)
{
	ll_device_list_t dev = {0};

	dev.name = "I2C2";
	dev.writeCB = HW_I2C2_Write;
	dev.readCB = HW_I2C2_Read;
	dev.DMA_writeCB = HW_I2C2_DMAWrite;
	if(LLOS_Register_Device(&dev) == LL_ERR_INVALID)
	{
		LOG_E("LLOS_Device_Register_I2C2 ", "%s register failed!\r\n", dev.name);
		while(1);
	}
}

static ll_err_t HW_I2C2_Write(ll_device_list_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	HAL_I2C_Master_Transmit(&hi2c2, address, (uint8_t *)buffer + offset, len, 100);
    return LL_ERR_SUCCESS;
}
static ll_err_t HW_I2C2_Read(ll_device_list_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	HAL_I2C_Master_Receive(&hi2c2, address, (uint8_t *)buffer + offset, len, 100);
    return LL_ERR_SUCCESS;
}
static ll_err_t HW_I2C2_DMAWrite(ll_device_list_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	busyFlag = true;
	HAL_I2C_Mem_Write_DMA(&hi2c2, address, 0x40, I2C_MEMADD_SIZE_8BIT , (uint8_t *)buffer, len);
	while(busyFlag);
	return LL_ERR_SUCCESS;
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if(hi2c->Instance == I2C2)
	{
		busyFlag = false;
	}
}