# TWI Slave — nRF54L15DK

I2C slave example for the nRF54L15DK development kit, built with
**nRF Connect SDK v3.2.4**.

The firmware registers an I2C target at address **0x54** using the
Zephyr I2C target buffer-mode API. It echoes back whatever 4-byte
value was last written by the master, allowing the master to verify
each round-trip.

---

## Hardware

**Board:** PCA10156 (nRF54L15DK), `nrf54l15dk/nrf54l15/cpuapp`

**Peripheral used:** `i2c22` (TWIS — I2C slave controller)

---

## Physical wiring

Connect the two nRF54L15DK boards as follows:

| Slave board | Signal | Master board |
|-------------|--------|--------------|
| P1.09       | SDA    | P1.08        |
| P1.13       | SCL    | P1.12        |
| GND         | GND    | GND          |

> **Note:** The slave uses P1.09/P1.13 (rather than P1.08/P1.12) to
> avoid sharing pins with DK buttons/LEDs.
>
> **Note:** Pull-up resistors on SDA and SCL are handled in software
> via `bias-pull-up` in the device tree overlay — no external
> resistors are needed.

---

## Build

From the NCS environment (west workspace at `D:\work\ncs\v3.2.0`):

```bash
cd twi_slave
west build -b nrf54l15dk/nrf54l15/cpuapp
```

---

## Flash

Find the serial number of the **slave** board:

```bash
nrfutil device list
```

Then flash:

```bash
west flash --snr <serial-number>
```

Or using the build directory explicitly:

```bash
west flash --build-dir build --snr <serial-number>
```

---

## Expected serial output

Connect a terminal at **115200 8N1** to the slave's COM port.
After reset the slave prints its start-up message and then reports
each write/read from the master:

```
*** Booting nRF Connect SDK v3.2.4-... ***
*** Using Zephyr OS v4.2.99-... ***
=== TWI Slave starting (addr 0x54) ===
TWI Slave ready - waiting for master...
TWIS: write done - counter = 0
TWIS: read request
TWIS: write done - counter = 1
TWIS: read request
TWIS: write done - counter = 2
TWIS: read request
...
```

Each pair of lines corresponds to one master transaction cycle
(write followed by read-back).

---

## Implementation notes

### Pinctrl function selector
On nRF54L15 the crossbar uses **different function selectors** for
TWIM (master) and TWIS (slave). The overlay must use
`NRF_PSEL(TWIS_SDA, ...)` / `NRF_PSEL(TWIS_SCL, ...)` — using the
TWIM selectors causes the crossbar to route the wrong signal and
the bus will not work.

### NVIC interrupt enable
The Zephyr TWIS shim (`drivers/i2c/i2c_nrfx_twis.c`) calls
`IRQ_CONNECT()` to register the ISR but never calls `irq_enable()`.
As a workaround, `main()` calls `irq_enable(DT_IRQN(NODE_TWIS))`
explicitly after `i2c_target_register()` so that TWIS events are
actually delivered to the CPU.
