/* linux/arch/arm/plat-s5pc1xx/setup-sdhci-gpio.c
 *
 * Copyright (c) 2009-2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV310 - Helper functions for setting up SDHCI device(s) GPIO (HSMMC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>

#include <mach/gpio.h>
#include <mach/map.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-sdhci.h>

#define GPK0DRV	(S5PV310_VA_GPIO2 + 0x4C)
#define GPK1DRV	(S5PV310_VA_GPIO2 + 0x6C)
#define GPK2DRV	(S5PV310_VA_GPIO2 + 0x8C)
#define GPK3DRV	(S5PV310_VA_GPIO2 + 0xAC)


void s5pv310_setup_sdhci0_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	/* Set all the necessary GPK0/GPK1 pins to special-function 2 */
	for (gpio = S5PV310_GPK0(0); gpio < S5PV310_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}
	switch (width) {
	case 8:
		/* GPK1[3:6] special-funtion 3 */
		for (gpio = S5PV310_GPK1(3); gpio <= S5PV310_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0x3FC0, GPK1DRV);
	case 4:
		/* GPK0[3:6] special-funtion 2 */
		for (gpio = S5PV310_GPK0(3); gpio <= S5PV310_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0x3FFF, GPK0DRV);
		break;
	case 1:
		/* Data pin GPK2[3] to special-function 2 */
		for (gpio = S5PV310_GPK0(3); gpio < S5PV310_GPK0(4); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0xFF, GPK0DRV);
	default:
		break;
	}

	s3c_gpio_setpull(S5PV310_GPK0(2), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PV310_GPK0(2), S3C_GPIO_SFN(2));
}

void s5pv310_setup_sdhci1_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	/* Set all the necessary GPK1[0:1] pins to special-function 2 */
	for (gpio = S5PV310_GPK1(0); gpio < S5PV310_GPK1(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	/* Data pin GPK1[3:6] to special-function 2 */
	for (gpio = S5PV310_GPK1(3); gpio <= S5PV310_GPK1(6); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}
	__raw_writel(0x3FFF, GPK1DRV);

	s3c_gpio_setpull(S5PV310_GPK1(2), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PV310_GPK1(2), S3C_GPIO_SFN(2));
}

void s5pv310_setup_sdhci2_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	/* Set all the necessary GPK2[0:1] pins to special-function 2 */
	for (gpio = S5PV310_GPK2(0); gpio < S5PV310_GPK2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	switch (width) {
	case 8:
		/* Data pin GPK3[3:6] to special-function 3 */
		for (gpio = S5PV310_GPK3(3); gpio <= S5PV310_GPK3(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0x3FC0, GPK3DRV);
	case 4:
		/* Data pin GPK2[3:6] to special-function 2 */
		for (gpio = S5PV310_GPK2(3); gpio <= S5PV310_GPK2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0x3FFF, GPK2DRV);
		break;
	case 1:
		/* Data pin GPK2[3] to special-function 2 */
		for (gpio = S5PV310_GPK2(3); gpio < S5PV310_GPK2(4); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		}
		__raw_writel(0xFF, GPK2DRV);
	default:

		break;
	}

	//s3c_gpio_setpull(S5PV310_GPK2(2), S3C_GPIO_PULL_UP);
	//s3c_gpio_cfgpin(S5PV310_GPK2(2), S3C_GPIO_SFN(2));
}

void s5pv310_setup_sdhci3_cfg_gpio(struct platform_device *dev, int width)
{
	unsigned int gpio;

	printk(KERN_DEBUG "AR6K: %s enter\n", __func__);

	/* Set all the necessary GPK1[0:1] pins to special-function 2 */
	for (gpio = S5PV310_GPK3(0); gpio < S5PV310_GPK3(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	}

	s3c_gpio_setpull(S5PV310_GPK3(1), S3C_GPIO_PULL_UP);

	/* Data pin GPK1[3:6] to special-function 2 */
	for (gpio = S5PV310_GPK3(3); gpio <= S5PV310_GPK3(6); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}
	__raw_writel(0x3FFF, GPK3DRV);

	//s3c_gpio_setpull(S5PV310_GPK3(2), S3C_GPIO_PULL_UP);
	//s3c_gpio_cfgpin(S5PV310_GPK3(2), S3C_GPIO_SFN(2));
}
