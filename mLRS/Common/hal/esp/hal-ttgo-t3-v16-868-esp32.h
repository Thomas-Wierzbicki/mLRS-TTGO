//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// hal
//********************************************************

//-------------------------------------------------------
// ESP32, TTGO T3 V1.6 (20180606), SX1276, TX/RX
//-------------------------------------------------------

#define DEVICE_HAS_SINGLE_LED
#define DEVICE_HAS_NO_DEBUG
//#define DEVICE_HAS_NO_SERIAL


//-- UARTS
// UARTB = serial port
// UART = output port, SBus or whatever
// UARTF = debug port

#define UARTB_USE_SERIAL
#ifdef DEVICE_IS_TRANSMITTER
#define UARTB_BAUD TX_SERIAL_BAUDRATE
#define UARTB_TXBUFSIZE TX_SERIAL_TXBUFSIZE
#define UARTB_RXBUFSIZE TX_SERIAL_RXBUFSIZE
#else
#define UARTB_BAUD RX_SERIAL_BAUDRATE
#define UARTB_TXBUFSIZE           RX_SERIAL_TXBUFSIZE
#define UARTB_RXBUFSIZE           RX_SERIAL_RXBUFSIZE
#endif
#define UARTB_USE_TX_IO IO_P1
#define UARTB_USE_RX_IO IO_P3

#define UARTF_USE_SERIAL
#define UARTF_BAUD                115200


//-- SX1: SX12xx & SPI

#define SPI_CS_IO                 IO_P18
#define SPI_SCK IO_P5
#define SPI_MISO IO_P19
#define SPI_MOSI IO_P27
#define SPI_FREQUENCY             10000000L
#define SX_RESET                  IO_P23
#define SX_DIO                    IO_P26
//#define SX_DIO1                   IO_P5

IRQHANDLER(void SX_DIO_EXTI_IRQHandler(void);)

void sx_init_gpio(void)
{
    gpio_init(SX_RESET, IO_MODE_OUTPUT_PP_HIGH);
    gpio_init(SX_DIO, IO_MODE_INPUT_ANALOG);
}

IRAM_ATTR void sx_amp_transmit(void) {}
IRAM_ATTR void sx_amp_receive(void) {}

void sx_dio_init_exti_isroff(void) { detachInterrupt(SX_DIO); }
void sx_dio_enable_exti_isr(void) { attachInterrupt(SX_DIO, SX_DIO_EXTI_IRQHandler, RISING); }
IRAM_ATTR void sx_dio_exti_isr_clearflag(void) {}


//-- No user button on this board; fixed bind phrase in common_conf.h
void button_init(void) {}
IRAM_ATTR bool button_pressed(void) { return false; }

#ifdef DEVICE_IS_TRANSMITTER
// USB UART normally carries data. Ground GPIO13 during reset to select CLI.
// GPIO13 is also SD CS: remove the microSD card when using this jumper.
#define DEVICE_HAS_COM_ON_SERIAL
bool ser_or_com_init(void)
{
    gpio_init(IO_P13, IO_MODE_INPUT_PU);
    return !gpio_read_activelow(IO_P13);
}
#define DEVICE_HAS_I2C_DISPLAY
#define I2C_SDA_IO IO_P21
#define I2C_SCL_IO IO_P22
#define I2C_CLOCKSPEED 400000L
// Status-only OLED: no five-way switch is fitted.
void fiveway_init(void) {}
IRAM_ATTR uint8_t fiveway_read(void) { return 0; }
#endif

//-- LEDs

#define LED_RED                   IO_P25

void leds_init(void)
{
    gpio_init(LED_RED, IO_MODE_OUTPUT_PP_LOW);
}

IRAM_ATTR void led_red_off(void) { gpio_low(LED_RED); }
IRAM_ATTR void led_red_on(void) { gpio_high(LED_RED); }
IRAM_ATTR void led_red_toggle(void) { gpio_toggle(LED_RED); }


//-- POWER

#define POWER_GAIN_DBM            0 // gain of a PA stage if present
#define POWER_SX1276_MAX          SX1276_OUTPUT_POWER_MAX // maximum allowed sx power
#define POWER_USE_DEFAULT_RFPOWER_CALC

#define RFPOWER_DEFAULT           1 // index into rfpower_list array

const rfpower_t rfpower_list[] = {
    { .dbm = POWER_0_DBM, .mW = 1 },
    { .dbm = POWER_10_DBM, .mW = 10 },
    { .dbm = POWER_17_DBM, .mW = 50 },
};
