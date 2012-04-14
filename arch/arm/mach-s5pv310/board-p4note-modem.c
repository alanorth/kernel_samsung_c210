/* linux/arch/arm/mach-xxxx/board-tuna-modems.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

/* inlcude platform specific file */
#include <linux/platform_data/modem.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

/* umts target platform data */
static struct modem_io_t umts_io_devices[] = {
	[0] = {
		.name = "umts_ipc0",
		.id = 0x1,
		.format = IPC_FMT,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
	[1] = {
		.name = "umts_rfs0",
		.id = 0x41,
		.format = IPC_RFS,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
	[2] = {
		.name = "umts_boot0",
		.id = 0x0,
		.format = IPC_BOOT,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
	[3] = {
		.name = "multipdp",
		.id = 0x1,
		.format = IPC_MULTI_RAW,
		.io_type = IODEV_DUMMY,
		.link = LINKDEV_HSIC,
	},
	[4] = {
		.name = "rmnet0",
		.id = 0x2A,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_HSIC,
	},
	[5] = {
		.name = "rmnet1",
		.id = 0x2B,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_HSIC,
	},
	[6] = {
		.name = "rmnet2",
		.id = 0x2C,
		.format = IPC_RAW,
		.io_type = IODEV_NET,
		.link = LINKDEV_HSIC,
	},
	[7] = {
		.name = "umts_router",
		.id = 0x39,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
	[8] = {
		.name = "umts_csd",
		.id = 0x21,
		.format = IPC_RAW,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
	[9] = {
		.name = "umts_ramdump0",
		.id = 0x0,
		.format = IPC_RAMDUMP,
		.io_type = IODEV_MISC,
		.link = LINKDEV_HSIC,
	},
};

static int umts_link_ldo_enble(bool enable)
{
	struct regulator *reg = regulator_get(NULL, "vhsic");
	if (IS_ERR(reg)) {
		pr_err("No VHSIC_1.2V regualtor: %d\n", reg);
		return reg;
	}
	if (enable)
		regulator_enable(reg);
	else
		regulator_disable(reg);
}

static struct modemlink_pm_data modem_link_pm_data = {
	.name = "link_pm",
	.link_ldo_enable = umts_link_ldo_enble,
	.gpio_link_enable = NULL,
	.gpio_link_active = GPIO_ACTIVE_STATE,
	.gpio_link_hostwake = GPIO_IPC_HOST_WAKEUP,
	.gpio_link_slavewake = GPIO_IPC_SLAVE_WAKEUP,
};

static struct modem_data umts_modem_data = {
	.name = "xmm6260",

	.gpio_cp_on = GPIO_PHONE_ON,
	.gpio_reset_req_n = GPIO_CP_REQ_RESET,
	.gpio_cp_reset = GPIO_CP_RST,
	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_phone_active = GPIO_PHONE_ACTIVE,
	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,
	.gpio_flm_uart_sel = 0,
	.gpio_cp_warm_reset = 0,

	.modem_type = IMC_XMM6260,
	.link_types = (1 << LINKDEV_HSIC),
	.modem_net = UMTS_NETWORK,

	.num_iodevs = ARRAY_SIZE(umts_io_devices),
	.iodevs = umts_io_devices,

	.link_pm_data = &modem_link_pm_data,
};

/* To get modem state, register phone active irq using resource */
static struct resource umts_modem_res[] = {
	[0] = {
		.name = "umts_phone_active",
		.start = IRQ_EINT14,	/* GPIO_PHONE_ACTIVE */
		.end = IRQ_EINT14,	/* GPIO_PHONE_ACTIVE */
		.flags = IORESOURCE_IRQ,
	},
	[1] = {
		.name = "link_pm_hostwake",
		.start = IRQ_EINT9,	/* GPIO_IPC_HOST_WAKEUP */
		.end = IRQ_EINT9,		/* GPIO_IPC_HOST_WAKEUP */
		.flags = IORESOURCE_IRQ,
	},
};

/* if use more than one modem device, then set id num */
static struct platform_device umts_modem = {
	.name = "modem_if",
	.id = -1,
	.num_resources = ARRAY_SIZE(umts_modem_res),
	.resource = umts_modem_res,
	.dev = {
		.platform_data = &umts_modem_data,
	},
};

static void umts_modem_cfg_gpio(void)
{
	int err = 0;

	unsigned gpio_reset_req_n = umts_modem_data.gpio_reset_req_n;
	unsigned gpio_cp_on = umts_modem_data.gpio_cp_on;
	unsigned gpio_cp_rst = umts_modem_data.gpio_cp_reset;
	unsigned gpio_pda_active = umts_modem_data.gpio_pda_active;
	unsigned gpio_phone_active = umts_modem_data.gpio_phone_active;
	unsigned gpio_cp_dump_int = umts_modem_data.gpio_cp_dump_int;
	unsigned gpio_flm_uart_sel = umts_modem_data.gpio_flm_uart_sel;
	unsigned irq_phone_active = umts_modem_res[0].start;

	if (gpio_reset_req_n) {
		err = gpio_request(gpio_reset_req_n, "RESET_REQ_N");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "RESET_REQ_N", err);
		}
		gpio_direction_output(gpio_reset_req_n, 0);
	}

	if (gpio_cp_on) {
		err = gpio_request(gpio_cp_on, "CP_ON");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "CP_ON", err);
		}
		gpio_direction_output(gpio_cp_on, 0);
	}

	if (gpio_cp_rst) {
		err = gpio_request(gpio_cp_rst, "CP_RST");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "CP_RST", err);
		}
		gpio_direction_output(gpio_cp_rst, 0);
	}

	if (gpio_pda_active) {
		err = gpio_request(gpio_pda_active, "PDA_ACTIVE");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "PDA_ACTIVE", err);
		}
		gpio_direction_output(gpio_pda_active, 0);
	}

	if (gpio_phone_active) {
		err = gpio_request(gpio_phone_active, "PHONE_ACTIVE");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "PHONE_ACTIVE", err);
		}
		gpio_direction_input(gpio_phone_active);
		s3c_gpio_cfgpin(gpio_phone_active, S3C_GPIO_SFN(0xF));
		pr_err("check phone active = %d\n", irq_phone_active);
	}

	if (gpio_cp_dump_int) {
		err = gpio_request(gpio_cp_dump_int, "CP_DUMP_INT");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "CP_DUMP_INT", err);
		}
		gpio_direction_input(gpio_cp_dump_int);
	}

	if (gpio_flm_uart_sel) {
		err = gpio_request(gpio_flm_uart_sel, "GPS_UART_SEL");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "GPS_UART_SEL", err);
		}
		gpio_direction_output(gpio_reset_req_n, 1);
	}

	if (gpio_phone_active) {
		//set_irq_type(irq_phone_active, IRQ_TYPE_LEVEL_HIGH);
		set_irq_type(gpio_to_irq(gpio_phone_active), IRQ_TYPE_LEVEL_HIGH);
	}

	printk(KERN_INFO "umts_modem_cfg_gpio done\n");
}

static void modem_link_pm_config_gpio(void)
{
	int err = 0;

	unsigned gpio_link_enable = modem_link_pm_data.gpio_link_enable;
	unsigned gpio_link_active = modem_link_pm_data.gpio_link_active;
	unsigned gpio_link_hostwake = modem_link_pm_data.gpio_link_hostwake;
	unsigned gpio_link_slavewake = modem_link_pm_data.gpio_link_slavewake;
	unsigned irq_link_hostwake = umts_modem_res[1].start;

	if (gpio_link_enable) {
		err = gpio_request(gpio_link_enable, "LINK_EN");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "LINK_EN", err);
		}
		gpio_direction_output(gpio_link_enable, 0);
	}

	if (gpio_link_active) {
		err = gpio_request(gpio_link_active, "LINK_ACTIVE");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "LINK_ACTIVE", err);
		}
		gpio_direction_output(gpio_link_active, 0);
	}

	if (gpio_link_hostwake) {
		err = gpio_request(gpio_link_hostwake, "HOSTWAKE");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "HOSTWAKE", err);
		}
		gpio_direction_output(gpio_link_hostwake, 0);
		s3c_gpio_cfgpin(gpio_link_hostwake, S3C_GPIO_SFN(0xF));
	}

	if (gpio_link_slavewake) {
		err = gpio_request(gpio_link_slavewake, "SLAVEWAKE");
		if (err) {
			printk(KERN_ERR "fail to request gpio %s : %d\n",
			       "SLAVEWAKE", err);
		}
		gpio_direction_input(gpio_link_slavewake);
	}

	if (gpio_link_hostwake)
		//set_irq_type(irq_link_hostwake, IRQ_TYPE_EDGE_BOTH);
		set_irq_type(gpio_to_irq(gpio_link_hostwake), IRQ_TYPE_EDGE_BOTH);

	printk(KERN_INFO "modem_link_pm_config_gpio done\n");
}

static int __init init_modem(void)
{
	int ret;
	printk(KERN_INFO "[MODEM_IF] init_modem\n");

	/* umts gpios configuration */
	umts_modem_cfg_gpio();
	modem_link_pm_config_gpio();
	ret = platform_device_register(&umts_modem);
	if (ret < 0)
		return ret;

	return ret;
}


int umts_is_modem_on(void)
{
	return gpio_get_value(umts_modem_data.gpio_phone_active);
}
EXPORT_SYMBOL_GPL(umts_is_modem_on);

int umts_set_link_active(int val)
{
	int v = val ? 1 : 0;
	if (umts_is_modem_on()) {
		gpio_set_value(modem_link_pm_data.gpio_link_active, v);
		pr_info("LINK_ACTIVE:%d\n", v);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(umts_set_link_active);

int umts_set_pda_active(int val)
{
	int v = val ? 1 : 0;
	if (umts_is_modem_on()) {
		gpio_set_value(umts_modem_data.gpio_pda_active, v);
		pr_info("PDA_ACTIVE:%d\n", v);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(umts_set_pda_active);

int umts_set_slave_wakeup(int val)
{
	if (umts_is_modem_on()) {
		gpio_set_value(modem_link_pm_data.gpio_link_slavewake, val ? 1 : 0);
		pr_info(">SLAV_WUP:%d,%d\n", val ? 1 : 0,
			gpio_get_value(modem_link_pm_data.gpio_link_slavewake));
	}
	return 0;
}
EXPORT_SYMBOL_GPL(umts_set_slave_wakeup);

#define HOST_WUP_LEVEL 0
int umts_is_host_wakeup(int val)
{
	return (gpio_get_value(modem_link_pm_data.gpio_link_hostwake)
		== HOST_WUP_LEVEL) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(umts_is_host_wakeup);






late_initcall(init_modem);
//device_initcall(init_modem);

#if 0 /* move to ehci-hcd.c */
static int __init init_modem_link(void)
{
	return platform_device_register(&tegra_ehci2_device);
}
late_initcall(init_modem_link);
#endif
