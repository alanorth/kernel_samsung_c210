/*
 * linux/drivers/media/video/s5p-mfc/s5p_mfc_cmd.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if 0

#include <linux/jiffies.h>

#if defined(CONFIG_ARCH_S5PV310)
#include <mach/regs-mfc.h>
#elif defined(CONFIG_ARCH_S5PV210)
#include <plat/regs-mfc.h>
#endif

#include "mfc_cmd.h"
#include "mfc_reg.h"
#include "mfc_log.h"
#include "mfc_dec.h"
#include "mfc_enc.h"
#include "mfc_mem.h"
#include "mfc_buf.h"

static unsigned int r2h_cmd;
static struct mfc_cmd_args r2h_args;

#undef MFC_PERF

#ifdef MFC_PERF
static int framecnt = 0;
struct timeval tv1, tv2;
#endif

irqreturn_t mfc_irq(int irq, void *dev_id)
{
	struct mfc_dev *dev = (struct mfc_dev *)dev_id;

	r2h_cmd = read_reg(MFC_RISC2HOST_CMD);
	mfc_dbg("MFC IRQ: 0x%02x\n", r2h_cmd);

	if (((r2h_cmd >= OPEN_CH_RET) && (r2h_cmd <= CLOSE_CH_RET)) ||
	    ((r2h_cmd >= SEQ_DONE_RET) && (r2h_cmd <= EDFU_INIT_RET)) ||
	    ( r2h_cmd == ERR_RET)) {
		memset(&r2h_args, 0, sizeof(struct mfc_cmd_args));

		r2h_args.arg[0] = read_reg(MFC_RISC2HOST_ARG1);
		r2h_args.arg[1] = read_reg(MFC_RISC2HOST_ARG2);
		r2h_args.arg[2] = read_reg(MFC_RISC2HOST_ARG3);
		r2h_args.arg[3] = read_reg(MFC_RISC2HOST_ARG4);

		if (r2h_cmd == ERR_RET)
			//mfc_dbg("F/W error code: %d", r2h_args.arg[1] & 0xFFFF);
			mfc_dbg("F/W error code: 0x%08x", r2h_args.arg[1]);
	} else {
		mfc_err("Unknown R2H return value: 0x%02x\n", r2h_cmd);
	}

#ifdef MFC_PERF
	if (framecnt > 0) {
		do_gettimeofday(&tv2);

		mfc_info("%d, %ld", framecnt,
			(long)(((tv2.tv_sec * 1000000) + tv2.tv_usec) - ((tv1.tv_sec * 1000000) + tv1.tv_usec)));

		framecnt++;
	}
#endif

	/* FIXME: codec wait_queue processing */
	dev->irq_sys = 1;
	wake_up(&dev->wait_sys);

	/*
	 * FIXME: check is codec command return or error
	 * move to mfc_wait_codec() ?
	 */
	write_reg(0xFFFF, MFC_SI_RTN_CHID);

	write_reg(0, MFC_RISC2HOST_CMD);
	write_reg(0, MFC_RISC_HOST_INT);

	return IRQ_HANDLED;
}

#if 0
static bool mfc_wait_codec(struct mfc_inst_ctx *ctx, enum mfc_r2h_ret ret)
{
	/*
	if (wait_event_timeout(dev->wait_codec[0], 0, timeout) == 0) {
		mfc_err("F/W timeout: 0x%02x\n", ret);

		return false;
	}

	if (r2h_cmd == ERR_RET)
		mfc_err("F/W error code: 0x%02x", r2h_args.arg[1] & 0xFFFF);

		return false;
	}

	if (r2h_cmd != ret) {
		mfc_err("F/W return (0x%02x) waiting for (0x%02x)\n",
			r2h_cmd, ret);

		return false;
	}
	*/
	return true;
}
#endif

static bool
mfc_wait_sys(struct mfc_dev *dev, enum mfc_r2h_ret ret, long timeout)
{
	/* FIXME: MUST protect by spinlock_irq */
	dev->irq_sys = 0;

	if (wait_event_timeout(dev->wait_sys, dev->irq_sys, timeout) == 0) {
		mfc_err("F/W timeout: 0x%02x\n", ret);

		return false;
	}

	if (r2h_cmd == ERR_RET) {
		mfc_err("F/W error code: 0x%02x", r2h_args.arg[1] & 0xFFFF);

		return false;
	}

	if (r2h_cmd != ret) {
		mfc_err("F/W return (0x%02x) waiting for (0x%02x)\n",
			r2h_cmd, ret);

		return false;
	}

	return true;
}

static bool write_h2r_cmd(enum mfc_h2r_cmd cmd, struct mfc_cmd_args *args)
{
	enum mfc_h2r_cmd pending_cmd;
	unsigned long timeo = jiffies;

	timeo += msecs_to_jiffies(H2R_CMD_TIMEOUT);

	/* wait until host to risc command register becomes 'NOP' */
	do {
		pending_cmd = read_reg(MFC_HOST2RISC_CMD);

		if (pending_cmd == H2R_NOP)
			break;

		schedule_timeout_uninterruptible(1);
		/* FiXME: cpu_relax() */
	} while (time_before(jiffies, timeo));

	if (pending_cmd != H2R_NOP)
		return false;

	write_reg(args->arg[0], MFC_HOST2RISC_ARG1);
	write_reg(args->arg[1], MFC_HOST2RISC_ARG2);
	write_reg(args->arg[2], MFC_HOST2RISC_ARG3);
	write_reg(args->arg[3], MFC_HOST2RISC_ARG4);

	write_reg(cmd, MFC_HOST2RISC_CMD);

	return true;
}

int s5p_mfc_cmd_fw_start(struct mfc_dev *dev)
{
	/* release RISC reset */
	write_reg(0x3FF, S5P_FIMV_SW_RESET);

	if (mfc_wait_sys(dev, FW_STATUS_RET,
		msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		mfc_err("failed to check F/W\n");
		return MFC_FW_LOAD_FAIL;
	}

	return MFC_OK;
}

int mfc_cmd_sys_init(struct mfc_dev *dev)
{
	struct mfc_cmd_args h2r_args;
	unsigned int fw_version, fw_memsize;

	memset(&h2r_args, 0, sizeof(struct mfc_cmd_args));
	h2r_args.arg[0] = MFC_FW_SYSTEM_SIZE;

	if (write_h2r_cmd(SYS_INIT, &h2r_args) == false)
		return MFC_CMD_FAIL;

	if (mfc_wait_sys(dev, SYS_INIT_RET,
		msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		mfc_err("failed to init system\n");
		return MFC_FW_INIT_FAIL;
	}

	fw_version = read_reg(MFC_FW_VERSION);
	fw_memsize = r2h_args.arg[0];

	mfc_info("MFC F/W version: %02x-%02x-%02x, %dkB\n",
		 (fw_version >> 16) & 0xff,
		 (fw_version >> 8) & 0xff,
		 (fw_version) & 0xff,
		 (fw_memsize) >> 10);

	return MFC_OK;
}

int mfc_cmd_sys_sleep(struct mfc_dev *dev)
{
	struct mfc_cmd_args h2r_args;

	memset(&h2r_args, 0, sizeof(struct mfc_cmd_args));

	if (write_h2r_cmd(SLEEP, &h2r_args) == false)
		return MFC_CMD_FAIL;

	if (mfc_wait_sys(dev, SLEEP_RET,
		msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		mfc_err("failed to sleep\n");
		return MFC_SLEEP_FAIL;
	}

	return MFC_OK;
}

int mfc_cmd_sys_wakeup(struct mfc_dev *dev)
{
	struct mfc_cmd_args h2r_args;

	memset(&h2r_args, 0, sizeof(struct mfc_cmd_args));

	if (write_h2r_cmd(WAKEUP, &h2r_args) == false)
		return MFC_CMD_FAIL;

	/* release RISC reset */
	write_reg(0x3FF, MFC_SW_RESET);

	if (mfc_wait_sys(dev, WAKEUP_RET,
		//msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		msecs_to_jiffies(20000)) == false) {
		mfc_err("failed to wakeup\n");
		return MFC_WAKEUP_FAIL;
	}

	return MFC_OK;
}

int mfc_cmd_inst_open(struct mfc_inst_ctx *ctx)
{
	struct mfc_cmd_args h2r_args;
	unsigned int crc = 0;
	unsigned int pixelcache = 0;
	struct mfc_dec_ctx *dec_ctx;
	struct mfc_enc_ctx *enc_ctx;

	if (ctx->type == DECODER) {
		dec_ctx = (struct mfc_dec_ctx *)ctx->c_priv;

		crc = dec_ctx->crc & 0x1;
		pixelcache = dec_ctx->pixelcache & 0x3;
	} else {
		enc_ctx = (struct mfc_enc_ctx *)ctx->c_priv;

		pixelcache = enc_ctx->pixelcache & 0x3;
	}

	memset(&h2r_args, 0, sizeof(struct mfc_cmd_args));
	h2r_args.arg[0] = ctx->codecid;
	h2r_args.arg[1] = crc << 31 | pixelcache;
	h2r_args.arg[2] = ctx->ctxbufofs;
	h2r_args.arg[3] = ctx->ctxbufsize;

	if (write_h2r_cmd(OPEN_CH, &h2r_args) == false)
		return MFC_CMD_FAIL;

	if (mfc_wait_sys(ctx->dev, OPEN_CH_RET,
		msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		mfc_err("failed to open instance\n");
		return MFC_OPEN_FAIL;
	}

	ctx->cmd_id = r2h_args.arg[0];

	mfc_dbg("inst id: %d, cmd id: %d, codec id: %d",
		ctx->id, ctx->cmd_id, ctx->codecid);

#ifdef MFC_PERF
	framecnt = 0;
#endif

	return ctx->cmd_id;
}

int mfc_cmd_inst_close(struct mfc_inst_ctx *ctx)
{
	struct mfc_cmd_args h2r_args;

	memset(&h2r_args, 0, sizeof(struct mfc_cmd_args));
	h2r_args.arg[0] = ctx->cmd_id;

	if (write_h2r_cmd(CLOSE_CH, &h2r_args) == false)
		return MFC_CMD_FAIL;

	if (mfc_wait_sys(ctx->dev, CLOSE_CH_RET,
		msecs_to_jiffies(H2R_INT_TIMEOUT)) == false) {
		mfc_err("failed to close instance\n");
		return MFC_CLOSE_FAIL;
	}

	return MFC_OK;
}

int mfc_cmd_seq_start(struct mfc_inst_ctx *ctx)
{
	/* all codec command pass the shared mem addrees */
	write_reg(ctx->shmofs, MFC_SI_CH1_HOST_WR_ADR);

	write_reg((SEQ_HEADER << 16 & 0x70000) | ctx->cmd_id,
		  MFC_SI_CH1_INST_ID);

	/* FIXME: close_instance ? */
	/* FIXME: mfc_wait_codec */
	if (mfc_wait_sys(ctx->dev, SEQ_DONE_RET,
		msecs_to_jiffies(CODEC_INT_TIMEOUT)) == false) {
		mfc_err("failed to init seq start\n");
		return MFC_DEC_INIT_FAIL;
	}

	return MFC_OK;
}

int mfc_cmd_init_buffers(struct mfc_inst_ctx *ctx)
{
	/* all codec command pass the shared mem addrees */
	write_reg(ctx->shmofs, MFC_SI_CH1_HOST_WR_ADR);

	write_reg((INIT_BUFFERS << 16 & 0x70000) | ctx->cmd_id,
		  MFC_SI_CH1_INST_ID);

	/* FIXME: close_instance ? */
	/* FIXME: mfc_wait_codec */
	if (mfc_wait_sys(ctx->dev, INIT_BUFFERS_RET,
		msecs_to_jiffies(CODEC_INT_TIMEOUT)) == false) {
		mfc_err("failed to init buffers\n");
		return MFC_DEC_INIT_FAIL;
	}

#ifdef MFC_PERF
	framecnt = 1;
#endif

	return MFC_OK;
}

int mfc_cmd_frame_start(struct mfc_inst_ctx *ctx)
{
	struct mfc_dec_ctx *dec_ctx;

	/* all codec command pass the shared mem addrees */
	write_reg(ctx->shmofs, MFC_SI_CH1_HOST_WR_ADR);

	if (ctx->type == DECODER) {
		dec_ctx = (struct mfc_dec_ctx *)ctx->c_priv;

		mfc_dbg("dec_ctx->lastframe: %d", dec_ctx->lastframe);

		if (dec_ctx->lastframe) {
			write_reg((LAST_SEQ << 16 & 0x70000) | ctx->cmd_id,
				  MFC_SI_CH1_INST_ID);
			dec_ctx->lastframe = 0;
		} else {
			write_reg((FRAME_START << 16 & 0x70000) | ctx->cmd_id,
				  MFC_SI_CH1_INST_ID);
		}
	} else { /* == ENCODER */
		write_reg((FRAME_START << 16 & 0x70000) | ctx->cmd_id,
			  MFC_SI_CH1_INST_ID);
	}

#ifdef MFC_PERF
	do_gettimeofday(&tv1);
#endif

	/* FIXME: close_instance ? */
	/* FIXME: mfc_wait_codec */
	if (mfc_wait_sys(ctx->dev, FRAME_DONE_RET,
		msecs_to_jiffies(CODEC_INT_TIMEOUT)) == false) {
		mfc_err("failed to frame start\n");
		return MFC_DEC_EXE_TIME_OUT;
	}

	return MFC_OK;
}

#endif

#include "regs-mfc.h"

#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_mem.h"

static int s5p_mfc_cmd_host2risc(int cmd, struct s5p_mfc_cmd_args *args)
{
	int cur_cmd;
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);

	/* wait until host to risc command register becomes 'H2R_CMD_EMPTY' */
	do {
		if (time_after(jiffies, timeout)) {
			mfc_err("Timeout while waiting for hardware.\n");
			return -EIO;
		}

		cur_cmd = s5p_mfc_read_reg(S5P_FIMV_HOST2RISC_CMD);
	} while (cur_cmd != S5P_FIMV_H2R_CMD_EMPTY);

	s5p_mfc_write_reg(args->arg[0], S5P_FIMV_HOST2RISC_ARG1);
	s5p_mfc_write_reg(args->arg[1], S5P_FIMV_HOST2RISC_ARG2);
	s5p_mfc_write_reg(args->arg[2], S5P_FIMV_HOST2RISC_ARG3);
	s5p_mfc_write_reg(args->arg[3], S5P_FIMV_HOST2RISC_ARG4);

	/* Issue the command */
	s5p_mfc_write_reg(cmd, S5P_FIMV_HOST2RISC_CMD);

	return 0;
}

int s5p_mfc_sys_init_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));
	h2r_args.arg[0] = FIRMWARE_CODE_SIZE;

	ret = s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_SYS_INIT, &h2r_args);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_sleep_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));

	ret = s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_SLEEP, &h2r_args);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_wakeup_cmd(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));

	ret = s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_WAKEUP, &h2r_args);

	mfc_debug_leave();

	return ret;
}

/* Open a new instance and get its number */
int s5p_mfc_open_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret;

	mfc_debug_enter();

	mfc_debug(2, "Requested codec mode: %d\n", ctx->codec_mode);

	memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));
	h2r_args.arg[0] = ctx->codec_mode;
	h2r_args.arg[1] = 0; /* no crc & no pixelcache */
	h2r_args.arg[2] = ctx->context_ofs;
	h2r_args.arg[3] = ctx->context_size;

	ret = s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_OPEN_INSTANCE, &h2r_args);

	mfc_debug_leave();

	return ret;
}

/* Close instance */
int s5p_mfc_close_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_cmd_args h2r_args;
	int ret = 0;

	mfc_debug_enter();

	if (ctx->state != MFCINST_FREE) {
		memset(&h2r_args, 0, sizeof(struct s5p_mfc_cmd_args));
		h2r_args.arg[0] = ctx->inst_no;

		ret = s5p_mfc_cmd_host2risc(S5P_FIMV_H2R_CMD_CLOSE_INSTANCE,
					    &h2r_args);
	} else {
		ret = -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}

