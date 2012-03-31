/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms of
 * such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include "mali_kernel_common.h"
#include "mali_kernel_utilization.h"
#include "mali_osk.h"
#include "mali_platform.h"
/*
 * The Power control scheme consists of 2 timers: utilization timer
 * and sampling timer. The sampling timer checks GPU core running
 * status every CHECK_GPU_ACTIVITY_TIMEOUT milliseconds.
 *
 * Every SEND_GPU_UTILIZATION_TIMEOUT milliseconds the utilization
 * timer signals and the active flags collected by the sampling
 * timer is sent to the mali_gpu_utilization_handler.
 * When there is no activity in the GPU core for more than
 * SEND_GPU_UTILIZATION_TIMEOUT milliseconds then the utilization
 * timer is stopped. Next time when the GPU core starts it starts
 * the utilization and sampling timer.
 *
 * The design is such that when there is no GPU activity for more
 * than SEND_GPU_UTILIZATION_TIMEOUT the utilization timer is inactive
 * and when there is no GPU activity for more than CHECK_GPU_ACTIVITY_TIMEOUT
 * the sampling timer is inactive.
 */

/* Define how often to calculate and report GPU utilization, in milliseconds */
#define SEND_GPU_UTILIZATION_TIMEOUT 1000  /* in milliseconds */
#define CHECK_GPU_ACTIVITY_TIMEOUT   5	 /* in milliseconds */

/* LOAD normalisation */
#define LOAD_NORMALISATION_FACTOR   2

static _mali_osk_lock_t  *time_data_lock;
static _mali_osk_timer_t *send_utilization_timer;
static _mali_osk_timer_t *sampling_timer;

static _mali_osk_atomic_t num_running_cores;

static mali_bool utilization_timer_running; /*  MALI_FALSE */
static mali_bool sampling_timer_running; /* MALI_FALSE */

static unsigned int active_flag;
static unsigned int flag_total;




static void check_GPU_core_activity_status(void *arg)
{
	flag_total += active_flag;

	_mali_osk_lock_wait(time_data_lock, _MALI_OSK_LOCKMODE_RW);
	sampling_timer_running = MALI_FALSE;

	/* MALI_PRINT(("#",flag_total)); */
	if (active_flag) {
		_mali_osk_timer_add(sampling_timer,
			_mali_osk_time_mstoticks(CHECK_GPU_ACTIVITY_TIMEOUT));
		sampling_timer_running = MALI_TRUE;
	}
	_mali_osk_lock_signal(time_data_lock, _MALI_OSK_LOCKMODE_RW);
}



static void send_gpu_utilization_info(void *arg)
{
/*	MALI_PRINT(("flag = %u\n",flag_total)); */

	if (flag_total == 0) {
		/* Don't reschedule timer,
		this will be started if new work arrives */
		utilization_timer_running = MALI_FALSE;

		/* No work done for this period, report zero usage */
		mali_gpu_utilization_handler(0);

		return;
	}

	_mali_osk_lock_wait(time_data_lock, _MALI_OSK_LOCKMODE_RW);
	_mali_osk_timer_add(send_utilization_timer,
		_mali_osk_time_mstoticks(SEND_GPU_UTILIZATION_TIMEOUT));
	utilization_timer_running = MALI_TRUE;
	_mali_osk_lock_signal(time_data_lock, _MALI_OSK_LOCKMODE_RW);

	mali_gpu_utilization_handler(flag_total * LOAD_NORMALISATION_FACTOR);
	flag_total = 0;
}


/* The timers need to deleted when going to suspend state.
 * When the GPU is started after the resume the timers are added in
 * the mali_utilization_core_start function.
 */
void mali_utilization_suspend(void)
{
/*	MALI_PRINT(("mali_util_suspend\n")); */
	if (NULL != send_utilization_timer) {
		_mali_osk_timer_del(send_utilization_timer);
		utilization_timer_running = MALI_FALSE;
	}

	if (NULL != sampling_timer) {
		_mali_osk_timer_del(sampling_timer);
		sampling_timer_running = MALI_FALSE;
	}
}



_mali_osk_errcode_t mali_utilization_init(void)
{
	_mali_osk_atomic_init(&num_running_cores, 0);

	time_data_lock = _mali_osk_lock_init(_MALI_OSK_LOCKFLAG_SPINLOCK_IRQ |
				_MALI_OSK_LOCKFLAG_NONINTERRUPTABLE, 0, 0);
	if (NULL == time_data_lock)
		return _MALI_OSK_ERR_FAULT;

	send_utilization_timer = _mali_osk_timer_init();
	if (NULL == send_utilization_timer)
		return _MALI_OSK_ERR_FAULT;

	_mali_osk_timer_setcallback(send_utilization_timer,
				send_gpu_utilization_info, NULL);

	sampling_timer = _mali_osk_timer_init();
	if (NULL == sampling_timer)
		return _MALI_OSK_ERR_FAULT;

	_mali_osk_timer_setcallback(sampling_timer,
				check_GPU_core_activity_status, NULL);

	return _MALI_OSK_ERR_OK;
}


void mali_utilization_term(void)
{
	utilization_timer_running = MALI_FALSE;
	sampling_timer_running	= MALI_FALSE;

	if (NULL != send_utilization_timer) {
		_mali_osk_timer_del(send_utilization_timer);
		_mali_osk_timer_term(send_utilization_timer);
		send_utilization_timer = NULL;
	}

	if (NULL != sampling_timer) {
		_mali_osk_timer_del(sampling_timer);
		_mali_osk_timer_term(sampling_timer);
		sampling_timer = NULL;
	}

	if (NULL != time_data_lock)
		_mali_osk_lock_term(time_data_lock);

	_mali_osk_atomic_term(&num_running_cores);

}



void mali_utilization_core_start(void)
{

	if (_mali_osk_atomic_inc_return(&num_running_cores) == 1) {
		/*
		 * We went from zero cores working, to one core working,
		 * we now consider the entire GPU for being busy
		 */
		_mali_osk_lock_wait(time_data_lock, _MALI_OSK_LOCKMODE_RW);

		if (utilization_timer_running == MALI_FALSE) {
			_mali_osk_timer_add(send_utilization_timer,
			_mali_osk_time_mstoticks(SEND_GPU_UTILIZATION_TIMEOUT));

			utilization_timer_running = MALI_TRUE;
		}

		if (sampling_timer_running == MALI_FALSE) {
			_mali_osk_timer_add(sampling_timer,
			_mali_osk_time_mstoticks(CHECK_GPU_ACTIVITY_TIMEOUT));

			sampling_timer_running = MALI_TRUE;
		}

		_mali_osk_lock_signal(time_data_lock, _MALI_OSK_LOCKMODE_RW);
		active_flag = 1;
	}
}



void mali_utilization_core_end(void)
{
	if (_mali_osk_atomic_dec_return(&num_running_cores) == 0)
		active_flag = 0;
}
