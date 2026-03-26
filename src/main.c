/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 *
 * TWI Slave example for nRF54L15
 *
 * Listens on I2C address 0x54 using the nrfx TWIS driver (i2c22).
 *
 *  - On WRITE: stores the 4-byte payload (counter value from master) and
 *              prints it to the console.
 *  - On READ:  returns the last stored payload to the master.
 *
 * Hardware wiring (to master board):
 *   Slave P1.08 (SDA)  ----  Master SDA
 *   Slave P1.12 (SCL)  ----  Master SCL
 *   Common GND          ----  Common GND
 *
 * Build:
 *   west build -b nrf54l15dk/nrf54l15/cpuapp -- -DAPP_DIR=<path_to_twi_slave>
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/linker/devicetree_regions.h>
#include <zephyr/sys/printk.h>
#include <nrfx_twis.h>

/* i2c-slave alias is defined in the board overlay and points to i2c22 */
#define NODE_TWIS  DT_ALIAS(i2c_slave)

/* Place the DMA-accessible buffers in the correct memory region (if required) */
#define TWIS_MEMORY_SECTION \
	COND_CODE_1(DT_NODE_HAS_PROP(NODE_TWIS, memory_regions),                          \
		(__attribute__((__section__(                                               \
			LINKER_DT_NODE_REGION_NAME(DT_PHANDLE(NODE_TWIS, memory_regions)))))), \
		())

#define SLAVE_ADDR   0x54
#define BUFFER_SIZE  4       /* one uint32_t counter */

/* nrfx TWIS instance – register base taken directly from devicetree */
static nrfx_twis_t twis = {
	.p_reg = (NRF_TWIS_Type *)DT_REG_ADDR(NODE_TWIS)
};

/* Shared buffer: written by master, read back on request */
static uint8_t slave_buf[BUFFER_SIZE] TWIS_MEMORY_SECTION;

/* ------------------------------------------------------------------ */
/* TWIS event handler (called from ISR context)                        */
/* ------------------------------------------------------------------ */
static void twis_event_handler(nrfx_twis_event_t const *p_event)
{
	switch (p_event->type) {

	case NRFX_TWIS_EVT_READ_REQ:
		/* Master wants to read – prepare the TX buffer */
		nrfx_twis_tx_prepare(&twis, slave_buf, BUFFER_SIZE);
		printk("TWIS: read request\n");
		break;

	case NRFX_TWIS_EVT_READ_DONE:
		printk("TWIS: read done (%u bytes sent)\n", p_event->data.rx_amount);
		break;

	case NRFX_TWIS_EVT_WRITE_REQ:
		/* Master wants to write – prepare the RX buffer */
		nrfx_twis_rx_prepare(&twis, slave_buf, BUFFER_SIZE);
		printk("TWIS: write request\n");
		break;

	case NRFX_TWIS_EVT_WRITE_DONE: {
		/* Decode the received counter and print it */
		uint32_t value = (uint32_t)slave_buf[0]
			       | ((uint32_t)slave_buf[1] <<  8)
			       | ((uint32_t)slave_buf[2] << 16)
			       | ((uint32_t)slave_buf[3] << 24);
		printk("TWIS: write done – counter = %u\n", value);
		break;
	}

	default:
		printk("TWIS: unhandled event %d\n", (int)p_event->type);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
	nrfx_err_t nrfx_ret;
	int ret;

	printk("=== TWI Slave starting (addr 0x%02X) ===\n", SLAVE_ADDR);

	/* Initialise the TWIS peripheral */
	const nrfx_twis_config_t config = {
		.addr       = {SLAVE_ADDR, 0},
		.skip_gpio_cfg = true,   /* pin mux is done via pinctrl below */
		.skip_psel_cfg = true,
	};

	nrfx_ret = nrfx_twis_init(&twis, &config, twis_event_handler);
	if (nrfx_ret != NRFX_SUCCESS) {
		printk("ERROR: nrfx_twis_init failed (0x%08X)\n", nrfx_ret);
		return -EIO;
	}

	/* Apply pin configuration from the board overlay */
	PINCTRL_DT_DEFINE(NODE_TWIS);
	ret = pinctrl_apply_state(PINCTRL_DT_DEV_CONFIG_GET(NODE_TWIS),
				  PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		printk("ERROR: pinctrl_apply_state failed (%d)\n", ret);
		return ret;
	}

	/* Connect and enable the TWIS interrupt */
	IRQ_CONNECT(DT_IRQN(NODE_TWIS), DT_IRQ(NODE_TWIS, priority),
		    nrfx_twis_irq_handler, &twis, 0);
	irq_enable(DT_IRQN(NODE_TWIS));

	nrfx_twis_enable(&twis);

	printk("TWI Slave ready – waiting for master...\n");

	/* Everything is interrupt-driven; nothing to do in the main loop */
	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
