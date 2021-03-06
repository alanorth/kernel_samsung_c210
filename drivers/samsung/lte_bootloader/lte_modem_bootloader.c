#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/lte_modemctl.h>

#include "lte_modem_bootloader.h"
#include <mach/gpio.h>

#include <linux/pm_runtime.h>

#define LEN_XMIT_DELEY	100

extern s5pv210_usb_port0_power(int enable);
extern struct platform_device s3c_device_usb_ehci;

extern int mc_get_active_state();
extern int mc_control_active_state(int val);

int lte_airplain_mode;
int lte_silent_reset_mode;

enum xmit_bootloader_status {
	XMIT_BOOT_DOWNLOAD_NOT_YET = 0,
	XMIT_BOOTLOADER_OK,
};

struct lte_modem_bootloader {
	struct spi_device *spi_dev;
	struct miscdevice dev;

	struct mutex lock;

	unsigned gpio_lte2ap_status;
	unsigned gpio_lte_active;

	enum xmit_bootloader_status xmit_status;
};
#define to_loader(misc)	container_of(misc, struct lte_modem_bootloader, dev);

static inline
int spi_xmit(struct lte_modem_bootloader *loader,
		const unsigned char val)
{
	unsigned char buf[1];
	int ret;
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len = 1,
		.tx_buf = buf,
	};

	buf[0] = val;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(loader->spi_dev, &msg);

	if (ret < 0)
		dev_err(&loader->spi_dev->dev, "%s - error %d\n",
				__func__, ret);

	return ret;
}

static
int bootloader_write(struct lte_modem_bootloader *loader,
		const char *addr, const int len)
{
	int i;
	int ret = 0;
	unsigned char lenbuf[4];

	if (loader->xmit_status == XMIT_BOOTLOADER_OK) {
		memcpy(lenbuf, &len, ARRAY_SIZE(lenbuf));
		for (i = 0 ; i < ARRAY_SIZE(lenbuf) ; i++) {
			ret = spi_xmit(loader, lenbuf[i]);
			if (ret < 0)
				goto exit_err;
		}
		msleep(LEN_XMIT_DELEY);
	}

	for (i = 0 ; i < len ; i++) {
		ret = spi_xmit(loader, addr[i]);
		if (ret < 0)
			goto exit_err;
	}

exit_err:
	return ret;
}

static inline
int bootloader_download(struct lte_modem_bootloader *loader,
		const char *buf, const int len)
{
	return bootloader_write(loader, buf, len);
}

static inline
int get_lte2ap_status(struct lte_modem_bootloader *loader)
{
	return gpio_get_value(loader->gpio_lte2ap_status);
}

static inline
int get_lte_active(struct lte_modem_bootloader *loader)
{
	return gpio_get_value(loader->gpio_lte_active);
}

static
int bootloader_open(struct inode *inode, struct file *flip)
{
	struct lte_modem_bootloader *loader = to_loader(flip->private_data);
	flip->private_data = loader;

	return 0;
}

static
long bootloader_ioctl(struct file *flip,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int status;
	struct lte_modem_bootloader_param param;
	struct lte_modem_bootloader *loader = flip->private_data;

	struct usb_hcd *ehci_hcd = platform_get_drvdata(&s3c_device_usb_ehci);

	mutex_lock(&loader->lock);
	switch (cmd) {
	case IOCTL_LTE_MODEM_XMIT_BOOT:

		if (loader->xmit_status == XMIT_BOOT_DOWNLOAD_NOT_YET) {
			if (!mc_get_active_state()) {
				mc_control_active_state(1);
			}

			lte_silent_reset_mode = 1;
			s5pv210_usb_port0_power(0);
			pm_runtime_get_sync(&s3c_device_usb_ehci.dev);
		}

		ret = copy_from_user(&param, (const void __user *)arg,
				sizeof(param));
		if (ret) {
			dev_err(&loader->spi_dev->dev, "%s - can not copy userdata\n",
					__func__);
			ret = -EFAULT;
			goto exit_err;
		}

		if (param.buf == NULL) {
			dev_err(&loader->spi_dev->dev, "%s - incorrect userdata\n",
					__func__);
			goto exit_err;
		}

		dev_err(&loader->spi_dev->dev, "xmit lte modem bin - size: %d\n",
				param.len);

		ret = bootloader_download(loader, param.buf, param.len);
		if (ret < 0)
			dev_err(&loader->spi_dev->dev, "failed to xmit boot bin\n");
		else
			if (loader->xmit_status == XMIT_BOOT_DOWNLOAD_NOT_YET)
				loader->xmit_status = XMIT_BOOTLOADER_OK;
			else if (loader->xmit_status == XMIT_BOOTLOADER_OK) {
				loader->xmit_status = XMIT_BOOT_DOWNLOAD_NOT_YET;
				s5pv210_usb_port0_power(1);
			}

		break;

	case IOCTL_LTE_MODEM_LTE2AP_STATUS:
		status = get_lte2ap_status(loader);
		ret = copy_to_user((unsigned int *)arg, &status,
				sizeof(status));

		break;

	case IOCTL_LTE_MODEM_LTE_ACTIVE:
		status = get_lte_active(loader);
		ret = copy_to_user((unsigned int *)arg, &status,
				sizeof(status));

		break;

	case IOCTL_LTE_MODEM_AIRPLAIN_ON:
		lte_airplain_mode = 1;
		dev_err(&loader->spi_dev->dev, "IOCTL_LTE_MODEM LPM_ON\n");
		break;

	case IOCTL_LTE_MODEM_AIRPLAIN_OFF:
		dev_err(&loader->spi_dev->dev, "IOCTL_LTE_MODEM LPM_OFF\n");
		lte_airplain_mode = 0;
		break;

	case IOCTL_LTE_SILENT_RESET_ON:

		lte_silent_reset_mode = 1;
		dev_err(&loader->spi_dev->dev, "IOCTL_LTE_SILENT_RESET_ON\n");
		break;

	case IOCTL_LTE_SILENT_RESET_OFF:
		dev_err(&loader->spi_dev->dev, "IOCTL_LTE_SILENT_RESET_OFF\n");
		lte_silent_reset_mode = 0;
		break;

	default:
		dev_err(&loader->spi_dev->dev, "ioctl cmd error\n");
		ret = -ENOIOCTLCMD;

		break;
	}
	mutex_unlock(&loader->lock);

exit_err:
	return ret;
}

static const struct file_operations lte_modem_bootloader_fops = {
	.owner = THIS_MODULE,
	.open = bootloader_open,
	.unlocked_ioctl = bootloader_ioctl,
};

static
int gpio_setup(struct lte_modem_bootloader *loader)
{
	if (!loader->gpio_lte2ap_status)
		return -EINVAL;

	gpio_request(loader->gpio_lte2ap_status, "GPIO_LTE2AP_STATUS");
	gpio_direction_input(loader->gpio_lte2ap_status);

	return 0;
}

static
int spi_test(struct lte_modem_bootloader *loader)
{
	int i;
	int ret;

	int len = 100;

	pr_err("%s\n", __func__);

	for (i = 0 ; i < len ; i++) {
		ret = spi_xmit(loader, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static
int __devinit lte_modem_bootloader_probe(struct spi_device *spi)
{
	int ret;

	struct lte_modem_bootloader *loader;
	struct lte_modem_bootloader_platform_data *pdata;

	loader = kzalloc(sizeof(struct lte_modem_bootloader), GFP_KERNEL);
	if (!loader) {
		pr_err("failed to allocate for lte_modem_bootloader\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	mutex_init(&loader->lock);

	spi->bits_per_word = 8;
	if (spi_setup(spi)) {
		pr_err("failed to setup spi for lte_modem_bootloader\n");
		ret = -EINVAL;
		goto err_setup;
	}

	loader->spi_dev = spi;

	if (!spi->dev.platform_data) {
		pr_err("failed to get platform data for lte_modem_bootloader\n");
		ret = -EINVAL;
		goto err_setup;
	}

	pdata = (struct lte_modem_bootloader_platform_data *)spi->dev.platform_data;

	loader->gpio_lte2ap_status = pdata->gpio_lte2ap_status;
	loader->gpio_lte_active = pdata->gpio_lte_active;
	loader->xmit_status = XMIT_BOOT_DOWNLOAD_NOT_YET;

	ret = gpio_setup(loader);
	if (ret) {
		pr_err("failed to set gpio for lte_modem_boot_loader\n");
		goto err_setup;
	}

	spi_set_drvdata(spi, loader);

	loader->dev.minor = MISC_DYNAMIC_MINOR;
	loader->dev.name = "lte_spi";
	loader->dev.fops = &lte_modem_bootloader_fops;
	ret = misc_register(&loader->dev);
	if (ret) {
		pr_err("failed to register misc dev for lte_modem_bootloader\n");
		goto err_setup;
	}

	pr_info("lte_modem_bootloader successfully probed\n");

	lte_airplain_mode = 0;

	return 0;

err_setup:
	mutex_destroy(&loader->lock);
	kfree(loader);

err_alloc:

	return ret;
}

static
int __devexit lte_modem_bootloader_remove(struct spi_device *spi)
{
	struct lte_modem_bootloader *loader = spi_get_drvdata(spi);

	misc_deregister(&loader->dev);
	mutex_destroy(&loader->lock);
	kfree(loader);

	return 0;
}

static
struct spi_driver lte_modem_bootloader_driver = {
	.driver = {
		.name = "lte_loader",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = lte_modem_bootloader_probe,
	.remove = __devexit_p(lte_modem_bootloader_remove),
};

static
int __init lte_modem_bootloader_init(void)
{
	int ret;
	pr_err("%s\n", __func__);

	ret = spi_register_driver(&lte_modem_bootloader_driver);

	if (ret)
		pr_err("failed to register lte bootloader - %x\n", ret);

	return ret;
}

static
void __exit lte_modem_bootloader_exit(void)
{
	spi_unregister_driver(&lte_modem_bootloader_driver);
}

module_init(lte_modem_bootloader_init);
module_exit(lte_modem_bootloader_exit);

MODULE_DESCRIPTION("LTE Modem Bootloader driver");
MODULE_LICENSE("GPL");
