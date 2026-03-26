/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 *
 * TWI Slave example for nRF54L15
 *
 * Listens on I2C address 0x54 using the Zephyr I2C target API (i2c22 / TWIS).
 *
 *  - On WRITE: stores the 4-byte payload (counter value from master) and
 *              prints it to the console.
 *  - On READ:  returns the last stored payload to the master.
 *
 * Hardware wiring (to master board, matches original test example):
 *   Slave P1.09 (SDA)  ----  Master P1.08 (SDA)
 *   Slave P1.13 (SCL)  ----  Master P1.12 (SCL)
 *   Common GND         ----  Common GND
 *
 * Build:
 *   west build -b nrf54l15dk/nrf54l15/cpuapp -- -DAPP_DIR=<path_to_twi_slave>
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* i2c-slave alias is defined in the board overlay and points to i2c22 */
#define NODE_TWIS   DT_ALIAS(i2c_slave)
#define SLAVE_ADDR  0x54
#define BUFFER_SIZE 4   /* one uint32_t counter */

/* Holds the latest counter written by the master; returned on read */
static uint8_t slave_buf[BUFFER_SIZE];

/* ------------------------------------------------------------------ */
/* Zephyr I2C target callbacks                                         */
/* ------------------------------------------------------------------ */

/*
 * Called by the TWIS shim when the master issues a read.
 * Provide a pointer to our buffer and its size; the shim copies
 * the data into its own DMA-safe buffer before arming the TX FIFO.
 */
static int target_buf_read_requested(struct i2c_target_config *config,
				     uint8_t **buf, uint32_t *len)
{
	*buf = slave_buf;
	*len = BUFFER_SIZE;
	printk("TWIS: read request\n");
	return 0;
}

/*
 * Called by the TWIS shim after the master finishes writing.
 * The shim passes a pointer to its internal receive buffer and
 * the number of bytes received.
 */
static void target_buf_write_received(struct i2c_target_config *config,
				      uint8_t *buf, uint32_t len)
{
	uint32_t to_copy = MIN(len, (uint32_t)BUFFER_SIZE);

	memcpy(slave_buf, buf, to_copy);

	uint32_t value = (uint32_t)slave_buf[0]
		       | ((uint32_t)slave_buf[1] <<  8)
		       | ((uint32_t)slave_buf[2] << 16)
		       | ((uint32_t)slave_buf[3] << 24);
	printk("TWIS: write done - counter = %u\n", value);
}

static const struct i2c_target_callbacks target_callbacks = {
	.buf_read_requested  = target_buf_read_requested,
	.buf_write_received  = target_buf_write_received,
};

static struct i2c_target_config target_cfg = {
	.address   = SLAVE_ADDR,
	.callbacks = &target_callbacks,
};

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
	const struct device *dev = DEVICE_DT_GET(NODE_TWIS);
	int ret;

	printk("=== TWI Slave starting (addr 0x%02X) ===\n", SLAVE_ADDR);

	if (!device_is_ready(dev)) {
		printk("ERROR: TWIS device not ready\n");
		return -ENODEV;
	}

	/*
	 * Register ourselves as an I2C target. The Zephyr TWIS shim driver
	 * (nordic,nrf-twis) configures the nrfx TWIS peripheral, applies
	 * pinctrl and enables the interrupt — no manual setup needed.
	 */
	ret = i2c_target_register(dev, &target_cfg);
	if (ret != 0) {
		printk("ERROR: i2c_target_register failed (%d)\n", ret);
		return ret;
	}

	printk("TWI Slave ready - waiting for master...\n");

	/* Everything is callback-driven; nothing to poll in the main loop */
	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
