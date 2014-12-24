/*
 * Pinctrl data for Wondermedia WM8880 SoC
 *
 * Copyright (c) 2014 Alexey Charkov <alchark@gmail.com>
 * Based on pinctrl-wm8850.c by Tony Prisk
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pinctrl-wmt.h"

/*
 * Describe the register offsets within the GPIO memory space
 * The dedicated external GPIO's should always be listed in bank 0
 * so they are exported in the 0..31 range which is what users
 * expect.
 *
 * Do not reorder these banks as it will change the pin numbering
 */
static const struct wmt_pinctrl_bank_registers wm8880_banks[] = {
	WMT_PINCTRL_BANK(0x40, 0x80, 0xC0, 0x00, 0x480, 0x4C0),		/* 0 */
	WMT_PINCTRL_BANK(0x44, 0x84, 0xC4, 0x04, 0x484, 0x4C4),		/* 1 */
	WMT_PINCTRL_BANK(0x48, 0x88, 0xC8, 0x08, 0x488, 0x4C8),		/* 2 */
	WMT_PINCTRL_BANK(0x4C, 0x8C, 0xCC, 0x0C, 0x48C, 0x4CC),		/* 3 */
	WMT_PINCTRL_BANK(0x50, 0x90, 0xD0, 0x10, 0x490, 0x4D0),		/* 4 */
	WMT_PINCTRL_BANK(0x54, 0x94, 0xD4, 0x14, 0x494, 0x4D4),		/* 5 */
	WMT_PINCTRL_BANK(0x58, 0x98, 0xD8, 0x18, 0x498, 0x4D8),		/* 6 */
	WMT_PINCTRL_BANK(0x7C, 0xBC, 0xFC, 0x3C, 0x4BC, 0x4FC),		/* 7 */
};

/* Please keep sorted by bank/bit */
#define WMT_PIN_EXTGPIO0	WMT_PIN(0, 0)
#define WMT_PIN_EXTGPIO1	WMT_PIN(0, 1)
#define WMT_PIN_EXTGPIO2	WMT_PIN(0, 2)
#define WMT_PIN_EXTGPIO3	WMT_PIN(0, 3)
#define WMT_PIN_EXTGPIO4	WMT_PIN(0, 4)
#define WMT_PIN_EXTGPIO5	WMT_PIN(0, 5)
#define WMT_PIN_EXTGPIO6	WMT_PIN(0, 6)
#define WMT_PIN_EXTGPIO7	WMT_PIN(0, 7)
#define WMT_PIN_EXTGPIO8	WMT_PIN(0, 8)
#define WMT_PIN_EXTGPIO9	WMT_PIN(0, 9)
#define WMT_PIN_EXTGPIO10	WMT_PIN(0, 10)
#define WMT_PIN_EXTGPIO11	WMT_PIN(0, 11)
#define WMT_PIN_EXTGPIO12	WMT_PIN(0, 12)
#define WMT_PIN_EXTGPIO13	WMT_PIN(0, 13)
#define WMT_PIN_EXTGPIO14	WMT_PIN(0, 14)
#define WMT_PIN_EXTGPIO15	WMT_PIN(0, 15)
#define WMT_PIN_EXTGPIO16	WMT_PIN(0, 16)
#define WMT_PIN_EXTGPIO17	WMT_PIN(0, 17)
#define WMT_PIN_EXTGPIO18	WMT_PIN(0, 18)
#define WMT_PIN_EXTGPIO19	WMT_PIN(0, 19)
#define WMT_PIN_VDOUT0		WMT_PIN(1, 0)
#define WMT_PIN_VDOUT1		WMT_PIN(1, 1)
#define WMT_PIN_VDOUT2		WMT_PIN(1, 2)
#define WMT_PIN_VDOUT3		WMT_PIN(1, 3)
#define WMT_PIN_VDOUT4		WMT_PIN(1, 4)
#define WMT_PIN_VDOUT5		WMT_PIN(1, 5)
#define WMT_PIN_VDOUT6		WMT_PIN(1, 6)
#define WMT_PIN_VDOUT7		WMT_PIN(1, 7)
#define WMT_PIN_VDOUT8		WMT_PIN(1, 8)
#define WMT_PIN_VDOUT9		WMT_PIN(1, 9)
#define WMT_PIN_VDOUT10		WMT_PIN(1, 10)
#define WMT_PIN_VDOUT11		WMT_PIN(1, 11)
#define WMT_PIN_VDOUT12		WMT_PIN(1, 12)
#define WMT_PIN_VDOUT13		WMT_PIN(1, 13)
#define WMT_PIN_VDOUT14		WMT_PIN(1, 14)
#define WMT_PIN_VDOUT15		WMT_PIN(1, 15)
#define WMT_PIN_VDOUT16		WMT_PIN(1, 16)
#define WMT_PIN_VDOUT17		WMT_PIN(1, 17)
#define WMT_PIN_VDOUT18		WMT_PIN(1, 18)
#define WMT_PIN_VDOUT19		WMT_PIN(1, 19)
#define WMT_PIN_VDOUT20		WMT_PIN(1, 20)
#define WMT_PIN_VDOUT21		WMT_PIN(1, 21)
#define WMT_PIN_VDOUT22		WMT_PIN(1, 22)
#define WMT_PIN_VDOUT23		WMT_PIN(1, 23)
#define WMT_PIN_VDDEN		WMT_PIN(1, 24)
#define WMT_PIN_VDHSYNC		WMT_PIN(1, 25)
#define WMT_PIN_VDVSYNC		WMT_PIN(1, 26)
#define WMT_PIN_VDCLK		WMT_PIN(1, 27)
/* reserved */
#define WMT_PIN_VDIN0		WMT_PIN(2, 0)
#define WMT_PIN_VDIN1		WMT_PIN(2, 1)
#define WMT_PIN_VDIN2		WMT_PIN(2, 2)
#define WMT_PIN_VDIN3		WMT_PIN(2, 3)
#define WMT_PIN_VDIN4		WMT_PIN(2, 4)
#define WMT_PIN_VDIN5		WMT_PIN(2, 5)
#define WMT_PIN_VDIN6		WMT_PIN(2, 6)
#define WMT_PIN_VDIN7		WMT_PIN(2, 7)
#define WMT_PIN_VHSYNC		WMT_PIN(2, 8)
#define WMT_PIN_VVSYNC		WMT_PIN(2, 9)
#define WMT_PIN_VCLK		WMT_PIN(2, 10)
/* reserved */
#define WMT_PIN_I2SDACDAT0	WMT_PIN(2, 16)
#define WMT_PIN_I2SDACDAT1	WMT_PIN(2, 17)
#define WMT_PIN_I2SDACDAT2	WMT_PIN(2, 18)
#define WMT_PIN_I2SDACDAT3	WMT_PIN(2, 19)
#define WMT_PIN_I2SADCDAT2	WMT_PIN(2, 20)
#define WMT_PIN_I2SDACLRC	WMT_PIN(2, 21)
#define WMT_PIN_I2SDACBCLK	WMT_PIN(2, 22)
#define WMT_PIN_I2SDACMCLK	WMT_PIN(2, 23)
#define WMT_PIN_I2SADCDAT0	WMT_PIN(2, 24)
#define WMT_PIN_I2SADCDAT1	WMT_PIN(2, 25)
#define WMT_PIN_I2SSPDIFO	WMT_PIN(2, 26)
/* reserved */
#define WMT_PIN_SPI0CLK		WMT_PIN(3, 0)
#define WMT_PIN_SPI0MISO	WMT_PIN(3, 1)
#define WMT_PIN_SPI0MOSI	WMT_PIN(3, 2)
#define WMT_PIN_SD018SEL	WMT_PIN(3, 3)
/* reserved */
#define WMT_PIN_SD0CLK		WMT_PIN(3, 8)
#define WMT_PIN_SD0CMD		WMT_PIN(3, 9)
#define WMT_PIN_SD0WP		WMT_PIN(3, 10)
#define WMT_PIN_SD0DATA0	WMT_PIN(3, 11)
#define WMT_PIN_SD0DATA1	WMT_PIN(3, 12)
#define WMT_PIN_SD0DATA2	WMT_PIN(3, 13)
#define WMT_PIN_SD0DATA3	WMT_PIN(3, 14)
#define WMT_PIN_SD0PWRSW	WMT_PIN(3, 15)
#define WMT_PIN_NANDALE		WMT_PIN(3, 16)
#define WMT_PIN_NANDCLE		WMT_PIN(3, 17)
#define WMT_PIN_NANDWE		WMT_PIN(3, 18)
#define WMT_PIN_NANDRE		WMT_PIN(3, 19)
#define WMT_PIN_NANDWP		WMT_PIN(3, 20)
#define WMT_PIN_NANDWPD		WMT_PIN(3, 21)
#define WMT_PIN_NANDRB0		WMT_PIN(3, 22)
#define WMT_PIN_NANDRB1		WMT_PIN(3, 23)
#define WMT_PIN_NANDCE0		WMT_PIN(3, 24)
#define WMT_PIN_NANDCE1		WMT_PIN(3, 25)
#define WMT_PIN_NANDCE2		WMT_PIN(3, 26)
#define WMT_PIN_NANDCE3		WMT_PIN(3, 27)
#define WMT_PIN_NANDDQS		WMT_PIN(3, 28)
/* reserved */
#define WMT_PIN_NANDIO0		WMT_PIN(4, 0)
#define WMT_PIN_NANDIO1		WMT_PIN(4, 1)
#define WMT_PIN_NANDIO2		WMT_PIN(4, 2)
#define WMT_PIN_NANDIO3		WMT_PIN(4, 3)
#define WMT_PIN_NANDIO4		WMT_PIN(4, 4)
#define WMT_PIN_NANDIO5		WMT_PIN(4, 5)
#define WMT_PIN_NANDIO6		WMT_PIN(4, 6)
#define WMT_PIN_NANDIO7		WMT_PIN(4, 7)
#define WMT_PIN_I2C0SCL		WMT_PIN(4, 8)
#define WMT_PIN_I2C0SDA		WMT_PIN(4, 9)
#define WMT_PIN_I2C1SCL		WMT_PIN(4, 10)
#define WMT_PIN_I2C1SDA		WMT_PIN(4, 11)
#define WMT_PIN_I2C2SCL		WMT_PIN(4, 12)
#define WMT_PIN_I2C2SDA		WMT_PIN(4, 13)
#define WMT_PIN_C24MOUT		WMT_PIN(4, 14)
/* reserved */
#define WMT_PIN_UART0TXD	WMT_PIN(4, 16)
#define WMT_PIN_UART0RXD	WMT_PIN(4, 17)
#define WMT_PIN_UART0RTS	WMT_PIN(4, 18)
#define WMT_PIN_UART0CTS	WMT_PIN(4, 19)
#define WMT_PIN_UART1TXD	WMT_PIN(4, 20)
#define WMT_PIN_UART1RXD	WMT_PIN(4, 21)
#define WMT_PIN_UART1RTS	WMT_PIN(4, 22)
#define WMT_PIN_UART1CTS	WMT_PIN(4, 23)
#define WMT_PIN_SD2DATA0	WMT_PIN(4, 24)
#define WMT_PIN_SD2DATA1	WMT_PIN(4, 25)
#define WMT_PIN_SD2DATA2	WMT_PIN(4, 26)
#define WMT_PIN_SD2DATA3	WMT_PIN(4, 27)
#define WMT_PIN_SD2CMD		WMT_PIN(4, 28)
#define WMT_PIN_SD2CLK		WMT_PIN(4, 29)
#define WMT_PIN_SD2PWRSW	WMT_PIN(4, 30)
#define WMT_PIN_SD2WP		WMT_PIN(4, 31)
#define WMT_PIN_PWMOUT0		WMT_PIN(5, 0)
#define WMT_PIN_C24MHZCLKI	WMT_PIN(5, 1)
/* reserved */
#define WMT_PIN_HDMIDDCSDA	WMT_PIN(5, 8)
#define WMT_PIN_HDMIDDCSCL	WMT_PIN(5, 9)
#define WMT_PIN_HDMIHPD		WMT_PIN(5, 10)
/* reserved */
#define WMT_PIN_I2C3SCL		WMT_PIN(5, 24)
#define WMT_PIN_I2C3SDA		WMT_PIN(5, 25)
#define WMT_PIN_HDMICEC		WMT_PIN(5, 26)
/* reserved */
#define WMT_PIN_SFCS0		WMT_PIN(6, 0)
#define WMT_PIN_SFCS1		WMT_PIN(6, 1)
#define WMT_PIN_SFCLK		WMT_PIN(6, 2)
#define WMT_PIN_SFDI		WMT_PIN(6, 3)
#define WMT_PIN_SFDO		WMT_PIN(6, 4)
/* reserved */
#define WMT_PIN_PCM1MCLK	WMT_PIN(6, 16)
#define WMT_PIN_PCM1CLK		WMT_PIN(6, 17)
#define WMT_PIN_PCM1SYNC	WMT_PIN(6, 18)
#define WMT_PIN_PCM1OUT		WMT_PIN(6, 19)
#define WMT_PIN_PCM1IN		WMT_PIN(6, 20)
/* reserved */
#define WMT_PIN_USBSW0		WMT_PIN(7, 0)
#define WMT_PIN_USBATTA0	WMT_PIN(7, 1)
#define WMT_PIN_USBOC0		WMT_PIN(7, 2)
#define WMT_PIN_USBOC1		WMT_PIN(7, 3)
#define WMT_PIN_USBOC2		WMT_PIN(7, 4)
/* reserved */
#define WMT_PIN_WAKEUP0		WMT_PIN(7, 16)
#define WMT_PIN_CIRIN		WMT_PIN(7, 17)
#define WMT_PIN_WAKEUP2		WMT_PIN(7, 18)
#define WMT_PIN_WAKEUP3		WMT_PIN(7, 19)
#define WMT_PIN_WAKEUP4		WMT_PIN(7, 20)
#define WMT_PIN_WAKEUP5		WMT_PIN(7, 21)
#define WMT_PIN_SUSGPIO0	WMT_PIN(7, 22)
#define WMT_PIN_SUSGPIO1	WMT_PIN(7, 23)
/* reserved */
#define WMT_PIN_SD0CD		WMT_PIN(7, 28)

static const struct pinctrl_pin_desc wm8880_pins[] = {
	PINCTRL_PIN(WMT_PIN_EXTGPIO0, "extgpio0"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO1, "extgpio1"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO2, "extgpio2"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO3, "extgpio3"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO4, "extgpio4"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO5, "extgpio5"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO6, "extgpio6"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO7, "extgpio7"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO8, "extgpio8"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO9, "extgpio9"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO10, "extgpio10"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO11, "extgpio11"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO12, "extgpio12"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO13, "extgpio13"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO14, "extgpio14"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO15, "extgpio15"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO16, "extgpio16"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO17, "extgpio17"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO18, "extgpio18"),
	PINCTRL_PIN(WMT_PIN_EXTGPIO19, "extgpio19"),
	PINCTRL_PIN(WMT_PIN_VDOUT0, "vdout0"),
	PINCTRL_PIN(WMT_PIN_VDOUT1, "vdout1"),
	PINCTRL_PIN(WMT_PIN_VDOUT2, "vdout2"),
	PINCTRL_PIN(WMT_PIN_VDOUT3, "vdout3"),
	PINCTRL_PIN(WMT_PIN_VDOUT4, "vdout4"),
	PINCTRL_PIN(WMT_PIN_VDOUT5, "vdout5"),
	PINCTRL_PIN(WMT_PIN_VDOUT6, "vdout6"),
	PINCTRL_PIN(WMT_PIN_VDOUT7, "vdout7"),
	PINCTRL_PIN(WMT_PIN_VDOUT8, "vdout8"),
	PINCTRL_PIN(WMT_PIN_VDOUT9, "vdout9"),
	PINCTRL_PIN(WMT_PIN_VDOUT10, "vdout10"),
	PINCTRL_PIN(WMT_PIN_VDOUT11, "vdout11"),
	PINCTRL_PIN(WMT_PIN_VDOUT12, "vdout12"),
	PINCTRL_PIN(WMT_PIN_VDOUT13, "vdout13"),
	PINCTRL_PIN(WMT_PIN_VDOUT14, "vdout14"),
	PINCTRL_PIN(WMT_PIN_VDOUT15, "vdout15"),
	PINCTRL_PIN(WMT_PIN_VDOUT16, "vdout16"),
	PINCTRL_PIN(WMT_PIN_VDOUT17, "vdout17"),
	PINCTRL_PIN(WMT_PIN_VDOUT18, "vdout18"),
	PINCTRL_PIN(WMT_PIN_VDOUT19, "vdout19"),
	PINCTRL_PIN(WMT_PIN_VDOUT20, "vdout20"),
	PINCTRL_PIN(WMT_PIN_VDOUT21, "vdout21"),
	PINCTRL_PIN(WMT_PIN_VDOUT22, "vdout22"),
	PINCTRL_PIN(WMT_PIN_VDOUT23, "vdout23"),
	PINCTRL_PIN(WMT_PIN_VDDEN, "vdden"),
	PINCTRL_PIN(WMT_PIN_VDHSYNC, "vdhsync"),
	PINCTRL_PIN(WMT_PIN_VDVSYNC, "vdvsync"),
	PINCTRL_PIN(WMT_PIN_VDCLK, "vdclk"),
	PINCTRL_PIN(WMT_PIN_VDIN0, "vdin0"),
	PINCTRL_PIN(WMT_PIN_VDIN1, "vdin1"),
	PINCTRL_PIN(WMT_PIN_VDIN2, "vdin2"),
	PINCTRL_PIN(WMT_PIN_VDIN3, "vdin3"),
	PINCTRL_PIN(WMT_PIN_VDIN4, "vdin4"),
	PINCTRL_PIN(WMT_PIN_VDIN5, "vdin5"),
	PINCTRL_PIN(WMT_PIN_VDIN6, "vdin6"),
	PINCTRL_PIN(WMT_PIN_VDIN7, "vdin7"),
	PINCTRL_PIN(WMT_PIN_VHSYNC, "vhsync"),
	PINCTRL_PIN(WMT_PIN_VVSYNC, "vvsync"),
	PINCTRL_PIN(WMT_PIN_VCLK, "vclk"),
	PINCTRL_PIN(WMT_PIN_I2SDACDAT0, "i2sdacdat0"),
	PINCTRL_PIN(WMT_PIN_I2SDACDAT1, "i2sdacdat1"),
	PINCTRL_PIN(WMT_PIN_I2SDACDAT2, "i2sdacdat2"),
	PINCTRL_PIN(WMT_PIN_I2SDACDAT3, "i2sdacdat3"),
	PINCTRL_PIN(WMT_PIN_I2SADCDAT2, "i2sadcdat2"),
	PINCTRL_PIN(WMT_PIN_I2SDACLRC, "i2sdaclrc"),
	PINCTRL_PIN(WMT_PIN_I2SDACBCLK, "i2sdacbclk"),
	PINCTRL_PIN(WMT_PIN_I2SDACMCLK, "i2sdacmclk"),
	PINCTRL_PIN(WMT_PIN_I2SADCDAT0, "i2sadcdat0"),
	PINCTRL_PIN(WMT_PIN_I2SADCDAT1, "i2sadcdat1"),
	PINCTRL_PIN(WMT_PIN_I2SSPDIFO, "i2sspdifo"),
	PINCTRL_PIN(WMT_PIN_SPI0CLK, "spi0clk"),
	PINCTRL_PIN(WMT_PIN_SPI0MISO, "spi0miso"),
	PINCTRL_PIN(WMT_PIN_SPI0MOSI, "spi0mosi"),
	PINCTRL_PIN(WMT_PIN_SD018SEL, "sd018sel"),
	PINCTRL_PIN(WMT_PIN_SD0CLK, "sd0clk"),
	PINCTRL_PIN(WMT_PIN_SD0CMD, "sd0cmd"),
	PINCTRL_PIN(WMT_PIN_SD0WP, "sd0wp"),
	PINCTRL_PIN(WMT_PIN_SD0DATA0, "sd0data0"),
	PINCTRL_PIN(WMT_PIN_SD0DATA1, "sd0data1"),
	PINCTRL_PIN(WMT_PIN_SD0DATA2, "sd0data2"),
	PINCTRL_PIN(WMT_PIN_SD0DATA3, "sd0data3"),
	PINCTRL_PIN(WMT_PIN_SD0PWRSW, "sd0pwrsw"),
	PINCTRL_PIN(WMT_PIN_NANDALE, "nandale"),
	PINCTRL_PIN(WMT_PIN_NANDCLE, "nandcle"),
	PINCTRL_PIN(WMT_PIN_NANDWE, "nandwe"),
	PINCTRL_PIN(WMT_PIN_NANDRE, "nandre"),
	PINCTRL_PIN(WMT_PIN_NANDWP, "nandwp"),
	PINCTRL_PIN(WMT_PIN_NANDWPD, "nandwpd"),
	PINCTRL_PIN(WMT_PIN_NANDRB0, "nandrb0"),
	PINCTRL_PIN(WMT_PIN_NANDRB1, "nandrb1"),
	PINCTRL_PIN(WMT_PIN_NANDCE0, "nandce0"),
	PINCTRL_PIN(WMT_PIN_NANDCE1, "nandce1"),
	PINCTRL_PIN(WMT_PIN_NANDCE2, "nandce2"),
	PINCTRL_PIN(WMT_PIN_NANDCE3, "nandce3"),
	PINCTRL_PIN(WMT_PIN_NANDDQS, "nanddqs"),
	PINCTRL_PIN(WMT_PIN_NANDIO0, "nandio0"),
	PINCTRL_PIN(WMT_PIN_NANDIO1, "nandio1"),
	PINCTRL_PIN(WMT_PIN_NANDIO2, "nandio2"),
	PINCTRL_PIN(WMT_PIN_NANDIO3, "nandio3"),
	PINCTRL_PIN(WMT_PIN_NANDIO4, "nandio4"),
	PINCTRL_PIN(WMT_PIN_NANDIO5, "nandio5"),
	PINCTRL_PIN(WMT_PIN_NANDIO6, "nandio6"),
	PINCTRL_PIN(WMT_PIN_NANDIO7, "nandio7"),
	PINCTRL_PIN(WMT_PIN_I2C0SCL, "i2c0scl"),
	PINCTRL_PIN(WMT_PIN_I2C0SDA, "i2c0sda"),
	PINCTRL_PIN(WMT_PIN_I2C1SCL, "i2c1scl"),
	PINCTRL_PIN(WMT_PIN_I2C1SDA, "i2c1sda"),
	PINCTRL_PIN(WMT_PIN_I2C2SCL, "i2c2scl"),
	PINCTRL_PIN(WMT_PIN_I2C2SDA, "i2c2sda"),
	PINCTRL_PIN(WMT_PIN_C24MOUT, "c24mout"),
	PINCTRL_PIN(WMT_PIN_UART0TXD, "uart0txd"),
	PINCTRL_PIN(WMT_PIN_UART0RXD, "uart0rxd"),
	PINCTRL_PIN(WMT_PIN_UART0RTS, "uart0rts"),
	PINCTRL_PIN(WMT_PIN_UART0CTS, "uart0cts"),
	PINCTRL_PIN(WMT_PIN_UART1TXD, "uart1txd"),
	PINCTRL_PIN(WMT_PIN_UART1RXD, "uart1rxd"),
	PINCTRL_PIN(WMT_PIN_UART1RTS, "uart1rts"),
	PINCTRL_PIN(WMT_PIN_UART1CTS, "uart1cts"),
	PINCTRL_PIN(WMT_PIN_SD2DATA0, "sd2data0"),
	PINCTRL_PIN(WMT_PIN_SD2DATA1, "sd2data1"),
	PINCTRL_PIN(WMT_PIN_SD2DATA2, "sd2data2"),
	PINCTRL_PIN(WMT_PIN_SD2DATA3, "sd2data3"),
	PINCTRL_PIN(WMT_PIN_SD2CMD, "sd2cmd"),
	PINCTRL_PIN(WMT_PIN_SD2CLK, "sd2clk"),
	PINCTRL_PIN(WMT_PIN_SD2PWRSW, "sd2pwrsw"),
	PINCTRL_PIN(WMT_PIN_SD2WP, "sd2wp"),
	PINCTRL_PIN(WMT_PIN_PWMOUT0, "pwmout0"),
	PINCTRL_PIN(WMT_PIN_C24MHZCLKI, "c24mhzclki"),
	PINCTRL_PIN(WMT_PIN_HDMIDDCSDA, "hdmiddcsda"),
	PINCTRL_PIN(WMT_PIN_HDMIDDCSCL, "hdmiddcscl"),
	PINCTRL_PIN(WMT_PIN_HDMIHPD, "hdmihpd"),
	PINCTRL_PIN(WMT_PIN_I2C3SCL, "i2c3scl"),
	PINCTRL_PIN(WMT_PIN_I2C3SDA, "i2c3sda"),
	PINCTRL_PIN(WMT_PIN_HDMICEC, "hdmicec"),
	PINCTRL_PIN(WMT_PIN_SFCS0, "sfcs0"),
	PINCTRL_PIN(WMT_PIN_SFCS1, "sfcs1"),
	PINCTRL_PIN(WMT_PIN_SFCLK, "sfclk"),
	PINCTRL_PIN(WMT_PIN_SFDI, "sfdi"),
	PINCTRL_PIN(WMT_PIN_SFDO, "sfdo"),
	PINCTRL_PIN(WMT_PIN_PCM1MCLK, "pcm1mclk"),
	PINCTRL_PIN(WMT_PIN_PCM1CLK, "pcm1clk"),
	PINCTRL_PIN(WMT_PIN_PCM1SYNC, "pcm1sync"),
	PINCTRL_PIN(WMT_PIN_PCM1OUT, "pcm1out"),
	PINCTRL_PIN(WMT_PIN_PCM1IN, "pcm1in"),
	PINCTRL_PIN(WMT_PIN_USBSW0, "usbsw0"),
	PINCTRL_PIN(WMT_PIN_USBATTA0, "usbatta0"),
	PINCTRL_PIN(WMT_PIN_USBOC0, "usboc0"),
	PINCTRL_PIN(WMT_PIN_USBOC1, "usboc1"),
	PINCTRL_PIN(WMT_PIN_USBOC2, "usboc2"),
	PINCTRL_PIN(WMT_PIN_WAKEUP0, "wakeup0"),
	PINCTRL_PIN(WMT_PIN_CIRIN, "cirin"),
	PINCTRL_PIN(WMT_PIN_WAKEUP2, "wakeup2"),
	PINCTRL_PIN(WMT_PIN_WAKEUP3, "wakeup3"),
	PINCTRL_PIN(WMT_PIN_WAKEUP4, "wakeup4"),
	PINCTRL_PIN(WMT_PIN_WAKEUP5, "wakeup5"),
	PINCTRL_PIN(WMT_PIN_SUSGPIO0, "susgpio0"),
	PINCTRL_PIN(WMT_PIN_SUSGPIO1, "susgpio1"),
	PINCTRL_PIN(WMT_PIN_SD0CD, "sd0cd"),
};

/* Order of these names must match the above list */
static const char * const wm8880_groups[] = {
	"extgpio0",
	"extgpio1",
	"extgpio2",
	"extgpio3",
	"extgpio4",
	"extgpio5",
	"extgpio6",
	"extgpio7",
	"extgpio8",
	"extgpio9",
	"extgpio10",
	"extgpio11",
	"extgpio12",
	"extgpio13",
	"extgpio14",
	"extgpio15",
	"extgpio16",
	"extgpio17",
	"extgpio18",
	"extgpio19",
	"vdout0",
	"vdout1",
	"vdout2",
	"vdout3",
	"vdout4",
	"vdout5",
	"vdout6",
	"vdout7",
	"vdout8",
	"vdout9",
	"vdout10",
	"vdout11",
	"vdout12",
	"vdout13",
	"vdout14",
	"vdout15",
	"vdout16",
	"vdout17",
	"vdout18",
	"vdout19",
	"vdout20",
	"vdout21",
	"vdout22",
	"vdout23",
	"vdden",
	"vdhsync",
	"vdvsync",
	"vdclk",
	"vdin0",
	"vdin1",
	"vdin2",
	"vdin3",
	"vdin4",
	"vdin5",
	"vdin6",
	"vdin7",
	"vhsync",
	"vvsync",
	"vclk",
	"i2sdacdat0",
	"i2sdacdat1",
	"i2sdacdat2",
	"i2sdacdat3",
	"i2sadcdat2",
	"i2sdaclrc",
	"i2sdacbclk",
	"i2sdacmclk",
	"i2sadcdat0",
	"i2sadcdat1",
	"i2sspdifo",
	"spi0clk",
	"spi0miso",
	"spi0mosi",
	"sd018sel",
	"sd0clk",
	"sd0cmd",
	"sd0wp",
	"sd0data0",
	"sd0data1",
	"sd0data2",
	"sd0data3",
	"sd0pwrsw",
	"nandale",
	"nandcle",
	"nandwe",
	"nandre",
	"nandwp",
	"nandwpd",
	"nandrb0",
	"nandrb1",
	"nandce0",
	"nandce1",
	"nandce2",
	"nandce3",
	"nanddqs",
	"nandio0",
	"nandio1",
	"nandio2",
	"nandio3",
	"nandio4",
	"nandio5",
	"nandio6",
	"nandio7",
	"i2c0scl",
	"i2c0sda",
	"i2c1scl",
	"i2c1sda",
	"i2c2scl",
	"i2c2sda",
	"c24mout",
	"uart0txd",
	"uart0rxd",
	"uart0rts",
	"uart0cts",
	"uart1txd",
	"uart1rxd",
	"uart1rts",
	"uart1cts",
	"sd2data0",
	"sd2data1",
	"sd2data2",
	"sd2data3",
	"sd2cmd",
	"sd2clk",
	"sd2pwrsw",
	"sd2wp",
	"pwmout0",
	"c24mhzclki",
	"hdmiddcsda",
	"hdmiddcscl",
	"hdmihpd",
	"i2c3scl",
	"i2c3sda",
	"hdmicec",
	"sfcs0",
	"sfcs1",
	"sfclk",
	"sfdi",
	"sfdo",
	"pcm1mclk",
	"pcm1clk",
	"pcm1sync",
	"pcm1out",
	"pcm1in",
	"usbsw0",
	"usbatta0",
	"usboc0",
	"usboc1",
	"usboc2",
	"wakeup0",
	"cirin",
	"wakeup2",
	"wakeup3",
	"wakeup4",
	"wakeup5",
	"susgpio0",
	"susgpio1",
};

static int wm8880_pinctrl_probe(struct platform_device *pdev)
{
	struct wmt_pinctrl_data *data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to allocate data\n");
		return -ENOMEM;
	}

	data->banks = wm8880_banks;
	data->nbanks = ARRAY_SIZE(wm8880_banks);
	data->pins = wm8880_pins;
	data->npins = ARRAY_SIZE(wm8880_pins);
	data->groups = wm8880_groups;
	data->ngroups = ARRAY_SIZE(wm8880_groups);

	return wmt_pinctrl_probe(pdev, data);
}

static int wm8880_pinctrl_remove(struct platform_device *pdev)
{
	return wmt_pinctrl_remove(pdev);
}

static struct of_device_id wmt_pinctrl_of_match[] = {
	{ .compatible = "wm,wm8880-pinctrl" },
	{ /* sentinel */ },
};

static struct platform_driver wmt_pinctrl_driver = {
	.probe	= wm8880_pinctrl_probe,
	.remove	= wm8880_pinctrl_remove,
	.driver = {
		.name	= "pinctrl-wm8880",
		.of_match_table	= wmt_pinctrl_of_match,
	},
};

module_platform_driver(wmt_pinctrl_driver);

MODULE_AUTHOR("Alexey Charkov <alchark@gmail.com>");
MODULE_DESCRIPTION("Wondermedia WM8880 Pincontrol driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, wmt_pinctrl_of_match);
