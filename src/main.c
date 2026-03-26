#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/irq.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define NODE_TWIS   DT_ALIAS(i2c_slave)
#define SLAVE_ADDR  0x54
#define BUFFER_SIZE 4

static uint8_t slave_buf[BUFFER_SIZE];

#ifdef CONFIG_I2C_TARGET_BUFFER_MODE

static int target_buf_read_requested(struct i2c_target_config *config,
                                     uint8_t **buf, uint32_t *len)
{
        *buf = slave_buf;
        *len = BUFFER_SIZE;
        printk("TWIS: read request\n");
        return 0;
}

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
        .buf_read_requested = target_buf_read_requested,
        .buf_write_received = target_buf_write_received,
};

#endif

static struct i2c_target_config target_cfg = {
        .address   = SLAVE_ADDR,
#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
        .callbacks = &target_callbacks,
#endif
};

int main(void)
{
        const struct device *dev = DEVICE_DT_GET(NODE_TWIS);
        int ret;

        printk("=== TWI Slave starting (addr 0x%02X) ===\n", SLAVE_ADDR);

        if (!device_is_ready(dev)) {
                printk("ERROR: TWIS device not ready\n");
                return -ENODEV;
        }

        ret = i2c_target_register(dev, &target_cfg);
        if (ret != 0) {
                printk("ERROR: i2c_target_register failed (%d)\n", ret);
                return ret;
        }

        /*
         * The Zephyr TWIS shim registers the ISR but never calls irq_enable().
         * Enable the NVIC line explicitly so TWIS events reach the CPU.
         */
        irq_enable(DT_IRQN(NODE_TWIS));

        printk("TWI Slave ready - waiting for master...\n");

        return 0;
}
