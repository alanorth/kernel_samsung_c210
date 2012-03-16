#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/mfd/max8997.h>
#include <linux/mfd/max8997-private.h>

#define MAX8997_MOTOR_REG_CONFIG2		0x2
#define MOTOR_LRA		(1<<7)
#define MOTOR_EN		(1<<6)
#define EXT_PWM		(0<<5)
#define DIVIDER_128		(1<<1)

struct vibrator_drvdata {
	struct max8997_motor_data *pdata;
	struct pwm_device	*pwm;
	struct regulator *regulator;
	struct i2c_client *client;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	spinlock_t lock;
	bool running;
	int timeout;
};

#ifdef CONFIG_VIBETONZ
struct vibrator_drvdata *g_data;
#endif

static int vibetonz_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;
	vibetonz_clk = clk_get(dev, "timers");
	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] failed to get clock for the motor\n");
		goto err_clk_get;
	}
	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);
	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static void i2c_max8997_hapticmotor(struct vibrator_drvdata *data, bool en)
{
	int ret;
	u8 value;

	if (en) {
		value = MOTOR_LRA | MOTOR_EN | EXT_PWM | DIVIDER_128;
		ret = max8997_write_reg(data->client,
				MAX8997_MOTOR_REG_CONFIG2, value);
		if (ret < 0)
			pr_err("[VIB] i2c write err : %d\n", ret);

	} else {
		value = MOTOR_LRA | EXT_PWM | DIVIDER_128;
		ret = max8997_write_reg(data->client,
				MAX8997_MOTOR_REG_CONFIG2, value);
		if (ret < 0)
			pr_err("[VIB] i2c write err : %d\n", ret);
	}
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *_timer)
{
	struct vibrator_drvdata *data =
		container_of(_timer, struct vibrator_drvdata, timer);

	data->timeout = 0;

	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

static void vibrator_work(struct work_struct *_work)
{
	struct vibrator_drvdata *data =
		container_of(_work, struct vibrator_drvdata, work);

	printk(KERN_DEBUG "[VIB] time = %dms\n", data->timeout);

	if (0 == data->timeout) {
		if (!data->running)
			return ;
		regulator_force_disable(data->regulator);
		pwm_disable(data->pwm);
		i2c_max8997_hapticmotor(data, false);
		if (data->pdata->motor_en)
			data->pdata->motor_en(false);
		data->running = false;

	} else {
		if (data->running)
			return ;
		if (data->pdata->motor_en)
			data->pdata->motor_en(true);
		i2c_max8997_hapticmotor(data, true);
		pwm_config(data->pwm,
			data->pdata->duty, data->pdata->period);
		pwm_enable(data->pwm);
		regulator_enable(data->regulator);
		data->running = true;
	}
}

static int vibrator_get_time(struct timed_output_dev *_dev)
{
	struct vibrator_drvdata	*data =
		container_of(_dev, struct vibrator_drvdata, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void vibrator_enable(struct timed_output_dev *_dev, int value)
{
	struct vibrator_drvdata	*data =
		container_of(_dev, struct vibrator_drvdata, dev);
	unsigned long	flags;

	cancel_work_sync(&data->work);
	hrtimer_cancel(&data->timer);
	data->timeout = value;
	schedule_work(&data->work);
	spin_lock_irqsave(&data->lock, flags);
	if (value > 0) {
		if (value > data->pdata->max_timeout)
			value = data->pdata->max_timeout;

		hrtimer_start(&data->timer,
			ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

#ifdef CONFIG_VIBETONZ
void vibtonz_en(bool en)
{
	struct vibrator_drvdata	*data = g_data;

	if (en) {
		if (data->running)
			return ;
		if (data->pdata->motor_en)
			data->pdata->motor_en(true);
		i2c_max8997_hapticmotor(data, true);
		pwm_enable(data->pwm);
		regulator_enable(data->regulator);
		data->running = true;
	} else {
		if (!data->running)
			return ;
		regulator_force_disable(data->regulator);
		pwm_disable(data->pwm);
		i2c_max8997_hapticmotor(data, false);
		if (data->pdata->motor_en)
			data->pdata->motor_en(false);
		data->running = false;
	}
}
EXPORT_SYMBOL(vibtonz_en);

void vibtonz_pwm(int nForce)
{
	struct vibrator_drvdata	*data = g_data;
	/* add to avoid the glitch issue */
	static int prev_duty = 0;
	int pwm_period = data->pdata->period;
	int pwm_duty = pwm_period/2 + ((pwm_period/2 - 2) * nForce)/127;

#if defined(CONFIG_MACH_P4W_REV01)
	if (pwm_duty > data->pdata->duty)
		pwm_duty = data->pdata->duty;
	else if (pwm_period - pwm_duty > data->pdata->duty)
		pwm_duty = pwm_period - data->pdata->duty;
#endif

	/* add to avoid the glitch issue */
	if (prev_duty != pwm_duty) {
		prev_duty = pwm_duty;
		pwm_config(data->pwm, pwm_duty, pwm_period);
	}
}
EXPORT_SYMBOL(vibtonz_pwm);
#endif

static int __devinit vibrator_probe(struct platform_device *pdev)
{
	struct max8997_dev *max8997 = dev_get_drvdata(pdev->dev.parent);
	struct max8997_platform_data *max8997_pdata
		= dev_get_platdata(max8997->dev);
	struct max8997_motor_data *pdata = max8997_pdata->motor;
	struct vibrator_drvdata *ddata;
	int error = 0;

	ddata = kzalloc(sizeof(struct vibrator_drvdata), GFP_KERNEL);
	if (NULL == ddata) {
		pr_err("[VIB] Failed to alloc memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	if (pdata->init_hw)
		pdata->init_hw();

	ddata->pdata = pdata;
	ddata->dev.name = "vibrator";
	ddata->dev.get_time = vibrator_get_time;
	ddata->dev.enable = vibrator_enable;
	ddata->client = max8997->hmotor;

	platform_set_drvdata(pdev, ddata);

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = vibrator_timer_func;
	INIT_WORK(&ddata->work, vibrator_work);
	spin_lock_init(&ddata->lock);

	ddata->pwm = pwm_request(pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm)) {
		pr_err("[VIB] Failed to request pwm.\n");
		error = -EFAULT;
		goto err_pwm_request;
	}
	pwm_config(ddata->pwm,
		ddata->pdata->period/2, ddata->pdata->period);

	vibetonz_clk_on(&pdev->dev, true);

	ddata->regulator = regulator_get(NULL, "vmotor");
	if (IS_ERR(ddata->regulator)) {
		pr_err("[VIB] Failed to get vmoter regulator.\n");
		error = -EFAULT;
		goto err_regulator_get;
	}

	error = timed_output_dev_register(&ddata->dev);
	if (error < 0) {
		pr_err("[VIB] Failed to register timed_output : %d\n", error);
		error = -EFAULT;
		goto err_timed_output_register;
	}

#ifdef CONFIG_VIBETONZ
	g_data = ddata;
#endif

	return 0;

err_timed_output_register:
	timed_output_dev_unregister(&ddata->dev);
err_regulator_get:
	regulator_put(ddata->regulator);
err_pwm_request:
	pwm_free(ddata->pwm);
err_free_mem:
	kfree(ddata);
	return error;
}

static int __devexit vibrator_remove(struct platform_device *pdev)
{
	struct vibrator_drvdata *data = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&data->dev);
	regulator_put(data->regulator);
	pwm_free(data->pwm);
	kfree(data);
	return 0;
}

static int vibrator_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	vibetonz_clk_on(&pdev->dev, false);
	return 0;
}
static int vibrator_resume(struct platform_device *pdev)
{
	vibetonz_clk_on(&pdev->dev, true);
	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe	= vibrator_probe,
	.remove	= __devexit_p(vibrator_remove),
	.suspend = vibrator_suspend,
	.resume	= vibrator_resume,
	.driver	= {
		.name	= "max8997-hapticmotor",
		.owner	= THIS_MODULE,
	}
};

static int __init vibrator_init(void)
{
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

late_initcall(vibrator_init);
module_exit(vibrator_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vibrator driver");
