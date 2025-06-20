#include <llos.h>
#include "hal_spi.h"
#include "spi.h"

static ll_err_t HW_SPI1_Write(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);
static ll_err_t HW_SPI1_Read(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len);
static ll_err_t HW_SPI1_WriteRead(ll_device_t *dev, uint32_t address, uint32_t offset, const void *writeData, void *readData, uint32_t len);
static ll_err_t HW_SPI1_Ctrl(ll_device_t *dev, uint32_t cmd, void *arg);
static ll_err_t HW_SPI1_DMAWrite(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len);

void LLOS_Device_Register_SPI(void)
{
	ll_device_t dev = {0};

	dev.name = "SPI1";
	dev.writeCB = HW_SPI1_Write;
	dev.readCB = HW_SPI1_Read;
	dev.write_readCB = HW_SPI1_WriteRead;
	dev.DMA_writeCB = HW_SPI1_DMAWrite;
	dev.ctrlCB = HW_SPI1_Ctrl;
	if(LLOS_Register_Device(&dev) == LL_ERR_INVALID)
	{
		LL_LOG_E("LLOS_Device_Register_SPI1 ", "%s register failed!\r\n", dev.name);
		while(1);
	}
}

static ll_err_t HW_SPI1_Write(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	HAL_SPI_Transmit(&hspi1, (uint8_t *)buffer + offset, len, 1000);
    return LL_ERR_SUCCESS;
}
static ll_err_t HW_SPI1_Read(ll_device_t *dev, uint32_t address, uint32_t offset, void *buffer, uint32_t len)
{
	HAL_SPI_Receive(&hspi1, (uint8_t *)buffer + offset, len, 1000);
    return LL_ERR_SUCCESS;
}
static ll_err_t HW_SPI1_WriteRead(ll_device_t *dev, uint32_t address, uint32_t offset, const void *writeData, void *readData, uint32_t len)
{
	HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)writeData + offset, (uint8_t *)readData + offset, len, 1000);
	return LL_ERR_SUCCESS;
}
static ll_err_t HW_SPI1_DMAWrite(ll_device_t *dev, uint32_t address, uint32_t offset, const void *buffer, uint32_t len)
{
	HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)buffer + offset, len);
	while(__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_BSY) == SET);
	return LL_ERR_SUCCESS;
}
static ll_err_t HW_SPI1_Ctrl(ll_device_t *dev, uint32_t cmd, void *arg)
{
	ll_bit_t bit;
	bit = *(uint8_t *)arg;
	
	if(bit)
	{
		//while(__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_BSY) == SET);
		LLOS_Device_WritePin(dev, cmd, ll_set);
	}
	else 
	{
		LLOS_Device_WritePin(dev, cmd, ll_reset);
	}

	return LL_ERR_SUCCESS;
}
