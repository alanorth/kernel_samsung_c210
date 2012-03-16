/*
 * kernel/power/main.c - PM subsystem core functionality.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/resume-trace.h>
#include <linux/workqueue.h>

#include "power.h"

#define CONFIG_DVFS_LIMIT

#ifdef CONFIG_DVFS_LIMIT
#include <mach/cpufreq.h>
#endif

DEFINE_MUTEX(pm_mutex);

unsigned int pm_flags;
EXPORT_SYMBOL(pm_flags);

#ifdef CONFIG_PM_SLEEP

/* Routines for PM-transition notifications */

static BLOCKING_NOTIFIER_HEAD(pm_chain_head);

int register_pm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&pm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_pm_notifier);

int unregister_pm_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&pm_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_pm_notifier);

int pm_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&pm_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

/* If set, devices may be suspended and resumed asynchronously. */
int pm_async_enabled = 1;

static ssize_t pm_async_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "%d\n", pm_async_enabled);
}

static ssize_t pm_async_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t n)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val > 1)
		return -EINVAL;

	pm_async_enabled = val;
	return n;
}

power_attr(pm_async);

#ifdef CONFIG_PM_DEBUG
int pm_test_level = TEST_NONE;

static const char * const pm_tests[__TEST_AFTER_LAST] = {
	[TEST_NONE] = "none",
	[TEST_CORE] = "core",
	[TEST_CPUS] = "processors",
	[TEST_PLATFORM] = "platform",
	[TEST_DEVICES] = "devices",
	[TEST_FREEZER] = "freezer",
};

static ssize_t pm_test_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char *s = buf;
	int level;

	for (level = TEST_FIRST; level <= TEST_MAX; level++)
		if (pm_tests[level]) {
			if (level == pm_test_level)
				s += sprintf(s, "[%s] ", pm_tests[level]);
			else
				s += sprintf(s, "%s ", pm_tests[level]);
		}

	if (s != buf)
		/* convert the last space to a newline */
		*(s-1) = '\n';

	return (s - buf);
}

static ssize_t pm_test_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	const char * const *s;
	int level;
	char *p;
	int len;
	int error = -EINVAL;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	mutex_lock(&pm_mutex);

	level = TEST_FIRST;
	for (s = &pm_tests[level]; level <= TEST_MAX; s++, level++)
		if (*s && len == strlen(*s) && !strncmp(buf, *s, len)) {
			pm_test_level = level;
			error = 0;
			break;
		}

	mutex_unlock(&pm_mutex);

	return error ? error : n;
}

power_attr(pm_test);
#endif /* CONFIG_PM_DEBUG */

#endif /* CONFIG_PM_SLEEP */

struct kobject *power_kobj;

/**
 *	state - control system power state.
 *
 *	show() returns what states are supported, which is hard-coded to
 *	'standby' (Power-On Suspend), 'mem' (Suspend-to-RAM), and
 *	'disk' (Suspend-to-Disk).
 *
 *	store() accepts one of those strings, translates it into the 
 *	proper enumerated value, and initiates a suspend transition.
 */
static ssize_t state_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *s = buf;
#ifdef CONFIG_SUSPEND
	int i;

	for (i = 0; i < PM_SUSPEND_MAX; i++) {
		if (pm_states[i] && valid_state(i))
			s += sprintf(s,"%s ", pm_states[i]);
	}
#endif
#ifdef CONFIG_HIBERNATION
	s += sprintf(s, "%s\n", "disk");
#else
	if (s != buf)
		/* convert the last space to a newline */
		*(s-1) = '\n';
#endif
	return (s - buf);
}

static ssize_t state_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
#ifdef CONFIG_SUSPEND
#ifdef CONFIG_EARLYSUSPEND
	suspend_state_t state = PM_SUSPEND_ON;
#else
	suspend_state_t state = PM_SUSPEND_STANDBY;
#endif
	const char * const *s;
#endif
	char *p;
	int len;
	int error = -EINVAL;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	/* First, check if we are requested to hibernate */
	if (len == 4 && !strncmp(buf, "disk", len)) {
		error = hibernate();
  goto Exit;
	}

#ifdef CONFIG_SUSPEND
	for (s = &pm_states[state]; state < PM_SUSPEND_MAX; s++, state++) {
		if (*s && len == strlen(*s) && !strncmp(buf, *s, len))
			break;
	}
	if (state < PM_SUSPEND_MAX && *s)
#ifdef CONFIG_EARLYSUSPEND
		if (state == PM_SUSPEND_ON || valid_state(state)) {
			error = 0;
			request_suspend_state(state);
		}
#else
		error = enter_state(state);
#endif
#endif

 Exit:
	return error ? error : n;
}

power_attr(state);

#ifdef CONFIG_PM_SLEEP
/*
 * The 'wakeup_count' attribute, along with the functions defined in
 * drivers/base/power/wakeup.c, provides a means by which wakeup events can be
 * handled in a non-racy way.
 *
 * If a wakeup event occurs when the system is in a sleep state, it simply is
 * woken up.  In turn, if an event that would wake the system up from a sleep
 * state occurs when it is undergoing a transition to that sleep state, the
 * transition should be aborted.  Moreover, if such an event occurs when the
 * system is in the working state, an attempt to start a transition to the
 * given sleep state should fail during certain period after the detection of
 * the event.  Using the 'state' attribute alone is not sufficient to satisfy
 * these requirements, because a wakeup event may occur exactly when 'state'
 * is being written to and may be delivered to user space right before it is
 * frozen, so the event will remain only partially processed until the system is
 * woken up by another event.  In particular, it won't cause the transition to
 * a sleep state to be aborted.
 *
 * This difficulty may be overcome if user space uses 'wakeup_count' before
 * writing to 'state'.  It first should read from 'wakeup_count' and store
 * the read value.  Then, after carrying out its own preparations for the system
 * transition to a sleep state, it should write the stored value to
 * 'wakeup_count'.  If that fails, at least one wakeup event has occured since
 * 'wakeup_count' was read and 'state' should not be written to.  Otherwise, it
 * is allowed to write to 'state', but the transition will be aborted if there
 * are any wakeup events detected after 'wakeup_count' was written to.
 */

static ssize_t wakeup_count_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	unsigned long val;

	return pm_get_wakeup_count(&val) ? sprintf(buf, "%lu\n", val) : -EINTR;
}

static ssize_t wakeup_count_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lu", &val) == 1) {
		if (pm_save_wakeup_count(val))
			return n;
	}
	return -EINVAL;
}

power_attr(wakeup_count);
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_TRACE
int pm_trace_enabled;

static ssize_t pm_trace_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "%d\n", pm_trace_enabled);
}

static ssize_t
pm_trace_store(struct kobject *kobj, struct kobj_attribute *attr,
	       const char *buf, size_t n)
{
	int val;

	if (sscanf(buf, "%d", &val) == 1) {
		pm_trace_enabled = !!val;
		return n;
	}
	return -EINVAL;
}

power_attr(pm_trace);
#endif /* CONFIG_PM_TRACE */

#ifdef CONFIG_USER_WAKELOCK
power_attr(wake_lock);
power_attr(wake_unlock);
#endif

#ifdef CONFIG_DVFS_LIMIT
static int dvfs_ctrl_val;
static int dvfslock_level;
static int dvfsctrl_locked;
static void do_dvfsunlock_timer(struct work_struct *work);
static DECLARE_DELAYED_WORK(dvfslock_ctrl_unlock_work, do_dvfsunlock_timer);
DEFINE_MUTEX(dvfslock_mutex);

static void dvfslock_ctrl(const char *buf, size_t count)
{
	int ret;
	int dlevel;
	int dtime_msec;
	int temp;

	mutex_lock(&dvfslock_mutex);

	temp = dvfs_ctrl_val;
	ret = sscanf(buf, "%u", &dvfs_ctrl_val);
	if (ret != 1) {
		pr_err("%s: invalid format(%d)\n", __func__, ret);
		ret = -EINVAL;
		goto out;
	}

	if (dvfs_ctrl_val == 0) {
		if (dvfsctrl_locked) {
			s5pv310_cpufreq_lock_free(DVFS_LOCK_ID_APP);
			dvfsctrl_locked = 0;
			ret = 0;
		} else {
			pr_warn("%s: there is no dvfslock!\n", __func__);
			ret = -EINVAL;
		}
		goto out;
	}

	if (dvfsctrl_locked) {
		pr_info("%s - already locked\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	dlevel = dvfs_ctrl_val >> 16;
	if (dlevel >= CPUFREQ_LEVEL_END || dlevel < 0) {
		pr_warn("%s: Invalid level = %d,\n", __func__, dlevel);
		ret = -EINVAL;
		goto out;
	}

	dvfslock_level = dlevel;

	dtime_msec = dvfs_ctrl_val & 0xFFFF;

	pr_debug("%s: dvfs_ctrl_val=%d, level=%d, time=%d\n",
		__func__, dvfs_ctrl_val, dlevel, dtime_msec);

	s5pv310_cpufreq_lock(DVFS_LOCK_ID_APP, dlevel);
	dvfsctrl_locked = 1;

	/* If the dtime_msec is zero, wait until lock free */
	if (dtime_msec)
		schedule_delayed_work(&dvfslock_ctrl_unlock_work,
					msecs_to_jiffies(dtime_msec));
out:
	if (ret < 0)
		dvfs_ctrl_val = temp;

	mutex_unlock(&dvfslock_mutex);
	return;
}

static void do_dvfsunlock_timer(struct work_struct *work)
{
	printk(KERN_DEBUG "%s: dvfs_ctrl_val=%d\n", __func__, dvfs_ctrl_val);

	dvfsctrl_locked = 0;
	s5pv310_cpufreq_lock_free(DVFS_LOCK_ID_APP);
}

static ssize_t dvfslock_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%08x\n", dvfs_ctrl_val);
}

static ssize_t dvfslock_ctrl_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	dvfslock_ctrl(buf, 0);
	return n;
}

static int dvfslimit_val;
static int dvfslimit_level;
static int dvfslimit_locked;
static void do_dvfslimit_unlock_timer(struct work_struct *work);
static DECLARE_DELAYED_WORK(dvfslimit_ctrl_unlock_work, do_dvfslimit_unlock_timer);
DEFINE_MUTEX(dvfslimit_mutex);

static void dvfslimit_ctrl(const char *buf)
{
	unsigned int ret;
	int limit_level;
	int dtime_msec;
	int temp;

	mutex_lock(&dvfslimit_mutex);

	temp = dvfslimit_val;
	ret = sscanf(buf, "%u", &dvfslimit_val);
	if (ret != 1) {
		printk(KERN_ERR "dvfslimit_ctrl is invalid format\n");
		goto out;
	}

	if (dvfslimit_val == 0) {
		if (dvfslimit_locked) {
			s5pv310_cpufreq_upper_limit_free(DVFS_LOCK_ID_APP);
			dvfslimit_locked = 0;
		} else {
			printk(KERN_ERR
			"Write lower level than CPU_L0 for upper limit\n");
		}
		goto out;
	}

	limit_level = dvfslimit_val >> 16;
	if (limit_level >= CPUFREQ_LEVEL_END || limit_level < 0) {
		printk(KERN_ERR "%s - Invalid level = %d,\n", __func__, limit_level);
		goto out;
	}

	if (dvfslimit_locked) {
		printk(KERN_ERR "%s - already locked dvfslimit_ctrl."
				"so, your request is ignored!\n", __func__);
		dvfslimit_val = temp;
		goto out;
	}

	dvfslimit_level = limit_level;

	dtime_msec = dvfslimit_val & 0xFFFF;

	printk(KERN_DEBUG "%s: dvfslimit=%d, level=%d, time=%d\n",
			__func__, dvfslimit_val, limit_level, dtime_msec);

	s5pv310_cpufreq_upper_limit(DVFS_LOCK_ID_APP, limit_level);
	dvfslimit_locked = 1;

	if (dtime_msec)
		schedule_delayed_work(&dvfslimit_ctrl_unlock_work,
					msecs_to_jiffies(dtime_msec));
out:
	mutex_unlock(&dvfslimit_mutex);
	return;
}

static void do_dvfslimit_unlock_timer(struct work_struct *work)
{
	s5pv310_cpufreq_upper_limit_free(DVFS_LOCK_ID_APP);
	dvfslimit_locked = 0;
}

static ssize_t dvfslimit_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (dvfslimit_locked)
		return sprintf(buf, "0x%08x\n", dvfslimit_val);
	else
		return sprintf(buf, "%s\n", "dvfslimit unlocked");
}

static ssize_t dvfslimit_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	dvfslimit_ctrl(buf);
	return n;
}

power_attr(dvfslimit_ctrl);
power_attr(dvfslock_ctrl);
#endif

static struct attribute * g[] = {
	&state_attr.attr,
#ifdef CONFIG_PM_TRACE
	&pm_trace_attr.attr,
#endif
#ifdef CONFIG_PM_SLEEP
	&pm_async_attr.attr,
	&wakeup_count_attr.attr,
#ifdef CONFIG_PM_DEBUG
	&pm_test_attr.attr,
#endif
#ifdef CONFIG_USER_WAKELOCK
	&wake_lock_attr.attr,
	&wake_unlock_attr.attr,
#endif
#endif
#ifdef CONFIG_DVFS_LIMIT
	&dvfslock_ctrl_attr.attr,
	&dvfslimit_ctrl_attr.attr,
#endif
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

#ifdef CONFIG_PM_RUNTIME
struct workqueue_struct *pm_wq;
EXPORT_SYMBOL_GPL(pm_wq);

static int __init pm_start_workqueue(void)
{
	pm_wq = create_freezeable_workqueue("pm");

	return pm_wq ? 0 : -ENOMEM;
}
#else
static inline int pm_start_workqueue(void) { return 0; }
#endif

static int __init pm_init(void)
{
	int error = pm_start_workqueue();
	if (error)
		return error;
	power_kobj = kobject_create_and_add("power", NULL);
	if (!power_kobj)
		return -ENOMEM;
	return sysfs_create_group(power_kobj, &attr_group);
}

core_initcall(pm_init);
