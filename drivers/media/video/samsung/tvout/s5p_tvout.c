/* linux/drivers/media/video/samsung/tvout/s5p_tvout.c
 *
 * Copyright (c) 2009 Samsung Electronics
 *		http://www.samsung.com/
 *
 * Entry file for Samsung TVOut driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/earlysuspend.h>

#ifdef CONFIG_CPU_S5PV210
#include <mach/pd.h>
#endif

#if defined(CONFIG_S5P_SYSMMU_TV)
#include <plat/sysmmu.h>
#endif

#if defined(CONFIG_S5P_MEM_CMA)
#include <linux/cma.h>
#elif defined(CONFIG_S5P_MEM_BOOTMEM)
#include <plat/media.h>
#include <mach/media.h>
#endif

#include "s5p_tvout_common_lib.h"
#include "s5p_tvout_ctrl.h"
#include "s5p_tvout_fb.h"
#include "s5p_tvout_v4l2.h"

#define TV_CLK_GET_WITH_ERR_CHECK(clk, pdev, clk_name)			\
		do {							\
			clk = clk_get(&pdev->dev, clk_name);		\
			if (IS_ERR(clk)) {				\
				printk(KERN_ERR				\
				"failed to find clock %s\n", clk_name);	\
				return -ENOENT;				\
			}						\
		} while (0);

struct s5p_tvout_status s5ptv_status;
bool on_stop_process;
bool on_start_process;
struct s5p_tvout_vp_bufferinfo s5ptv_vp_buff;
#ifdef CONFIG_PM
static struct workqueue_struct *tvout_resume_wq;
struct work_struct      tvout_resume_work;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s5ptv_early_suspend;
static DEFINE_MUTEX(s5p_tvout_mutex);
unsigned int suspend_status;
static void s5p_tvout_early_suspend(struct early_suspend *h);
static void s5p_tvout_late_resume(struct early_suspend *h);
#endif
int hdmi_audio_ext;

/* To provide an interface fo Audio path control */
static ssize_t hdmi_set_audio_read(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int count = 0;

	printk(KERN_ERR "[HDMI]: AUDIO PATH\n");
	return count;
}

static ssize_t hdmi_set_audio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *after;
	unsigned long value = !strncmp(buf, "1", 1) ? true : false;

	printk(KERN_ERR "[HDMI] Change AUDIO PATH: %ld\n", value);

	if (value) {
		s5p_hdmi_ctrl_set_audio(1);
		hdmi_audio_ext = 0;
	} else {
		s5p_hdmi_ctrl_set_audio(0);
		hdmi_audio_ext = 1;
	}

	return size;
}

static DEVICE_ATTR(hdmi_audio_set_ext, 0660,
				hdmi_set_audio_read, hdmi_set_audio_store);

static int __devinit s5p_tvout_clk_get(struct platform_device *pdev,
					struct s5p_tvout_status *ctrl)
{
	struct clk	*ext_xtal_clk,
			*mout_vpll_src,
			*fout_vpll,
			*mout_vpll;

	TV_CLK_GET_WITH_ERR_CHECK(ctrl->i2c_phy_clk,	pdev, "i2c-hdmiphy");

	TV_CLK_GET_WITH_ERR_CHECK(ctrl->sclk_dac,	pdev, "sclk_dac");
	TV_CLK_GET_WITH_ERR_CHECK(ctrl->sclk_hdmi,	pdev, "sclk_hdmi");

	TV_CLK_GET_WITH_ERR_CHECK(ctrl->sclk_pixel,	pdev, "sclk_pixel");
	TV_CLK_GET_WITH_ERR_CHECK(ctrl->sclk_hdmiphy,	pdev, "sclk_hdmiphy");

	TV_CLK_GET_WITH_ERR_CHECK(ext_xtal_clk,		pdev, "ext_xtal");
	TV_CLK_GET_WITH_ERR_CHECK(mout_vpll_src,	pdev, "vpll_src");
	TV_CLK_GET_WITH_ERR_CHECK(fout_vpll,		pdev, "fout_vpll");
	TV_CLK_GET_WITH_ERR_CHECK(mout_vpll,		pdev, "sclk_vpll");

/*
 *	if (clk_set_rate(fout_vpll, 54000000) < 0)
 *		return -1;
*/

	if (clk_set_parent(mout_vpll_src, ext_xtal_clk) < 0)
		return -1;

	if (clk_set_parent(mout_vpll, fout_vpll) < 0)
		return -1;

	/* sclk_dac's parent is fixed as mout_vpll */
	if (clk_set_parent(ctrl->sclk_dac, mout_vpll) < 0)
		return -1;

	/* It'll be moved in the future */
	if (clk_enable(mout_vpll_src) < 0)
		return -1;

	if (clk_enable(fout_vpll) < 0)
		return -1;

	if (clk_enable(mout_vpll) < 0)
		return -1;

	clk_put(ext_xtal_clk);
	clk_put(mout_vpll_src);
	clk_put(fout_vpll);
	clk_put(mout_vpll);

	return 0;
}


static int __devinit s5p_tvout_probe(struct platform_device *pdev)
{
#if defined(CONFIG_S5P_MEM_CMA)
	struct cma_info mem_info;
	int ret;
#elif defined(CONFIG_S5P_MEM_BOOTMEM)
	int mdev_id;
#endif
	unsigned int vp_buff_vir_addr;
	unsigned int vp_buff_phy_addr;
	int i;

	struct class *hdmi_audio_class;
	struct device *hdmi_audio_dev;

	s5p_tvout_pm_runtime_enable(&pdev->dev);

#if defined(CONFIG_S5P_SYSMMU_TV) && defined(CONFIG_VCM)
	if (s5p_tvout_vcm_create_unified() < 0)
		goto err;

	if (s5p_tvout_vcm_init() < 0)
		goto err;
#elif defined(CONFIG_S5P_SYSMMU_TV) && defined(CONFIG_S5P_VMEM)
	sysmmu_on(SYSMMU_TV);
	printk(KERN_ERR, "sysmmu on\n");
	sysmmu_set_tablebase_pgd(SYSMMU_TV, __pa(swapper_pg_dir));
#endif
	if (s5p_tvout_clk_get(pdev, &s5ptv_status) < 0)
		goto err;

	if (s5p_vp_ctrl_constructor(pdev) < 0)
		goto err;

	/* s5p_mixer_ctrl_constructor must be called
		before s5p_tvif_ctrl_constructor */
	if (s5p_mixer_ctrl_constructor(pdev) < 0)
		goto err;

	if (s5p_tvif_ctrl_constructor(pdev) < 0)
		goto err;

	if (s5p_tvout_v4l2_constructor(pdev) < 0)
		goto err;

#ifdef CONFIG_HAS_EARLYSUSPEND
	s5ptv_early_suspend.suspend = s5p_tvout_early_suspend;
	s5ptv_early_suspend.resume = s5p_tvout_late_resume;
	s5ptv_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 4;
	register_early_suspend(&s5ptv_early_suspend);
	suspend_status = 0;
#endif

#ifdef CONFIG_TV_FB
#ifndef CONFIG_USER_ALLOC_TVOUT
	s5p_hdmi_phy_power(true);
	if (s5p_tvif_ctrl_start(TVOUT_720P_60, TVOUT_HDMI) < 0)
		goto err;
#endif

	/* prepare memory */
	if (s5p_tvout_fb_alloc_framebuffer(&pdev->dev))
		goto err;

	if (s5p_tvout_fb_register_framebuffer(&pdev->dev))
		goto err;
#endif
	on_stop_process = false;
	on_start_process = false;

#if defined(CONFIG_S5P_MEM_CMA)
	/* CMA */
	ret = cma_info(&mem_info, &pdev->dev, 0);
	tvout_dbg("[cma_info] start_addr : 0x%x, end_addr : 0x%x, "
			"total_size : 0x%x, free_size : 0x%x\n",
			mem_info.lower_bound, mem_info.upper_bound,
			mem_info.total_size, mem_info.free_size);
	if (ret) {
		tvout_err("get cma info failed\n");
		goto err;
	}
	s5ptv_vp_buff.size = mem_info.total_size;
	if (s5ptv_vp_buff.size < S5PTV_VP_BUFF_CNT * S5PTV_VP_BUFF_SIZE) {
		tvout_err("insufficient vp buffer size (0x%8x), (0x%8x)\n",
				s5ptv_vp_buff.size, S5PTV_VP_BUFF_CNT * S5PTV_VP_BUFF_SIZE);
		goto err;
	}
	vp_buff_phy_addr = (unsigned int)cma_alloc
		(&pdev->dev, (char *)"tvout", (size_t)s5ptv_vp_buff.size, (dma_addr_t)0);

#elif defined(CONFIG_S5P_MEM_BOOTMEM)
	mdev_id = S5P_MDEV_TVOUT;
	/* alloc from bank1 as default */
	vp_buff_phy_addr = s5p_get_media_memory_bank(mdev_id, 1);
	s5ptv_vp_buff.size = s5p_get_media_memsize_bank(mdev_id, 1);
	if (s5ptv_vp_buff.size < S5PTV_VP_BUFF_CNT * S5PTV_VP_BUFF_SIZE) {
		tvout_err("insufficient vp buffer size\n");
		goto err;
	}
#endif

	tvout_dbg("s5ptv_vp_buff.size = 0x%x\n", s5ptv_vp_buff.size);
	tvout_dbg("s5ptv_vp_buff phy_base = 0x%x\n", vp_buff_phy_addr);

	vp_buff_vir_addr = phys_to_virt(vp_buff_phy_addr);
	tvout_dbg("s5ptv_vp_buff vir_base = 0x%x\n", vp_buff_vir_addr);

	if (!vp_buff_vir_addr) {
		tvout_err("io remap failed\n");
		goto err;
	}

	for (i = 0; i < S5PTV_VP_BUFF_CNT; i++) {
		s5ptv_vp_buff.vp_buffs[i].phy_base = vp_buff_phy_addr + (i * S5PTV_VP_BUFF_SIZE);
		s5ptv_vp_buff.vp_buffs[i].vir_base = vp_buff_vir_addr + (i * S5PTV_VP_BUFF_SIZE);
	}

	for (i = 0; i < S5PTV_VP_BUFF_CNT - 1; i++)
		s5ptv_vp_buff.copy_buff_idxs[i] = i;

	s5ptv_vp_buff.curr_copy_idx = 0;
	s5ptv_vp_buff.vp_access_buff_idx = S5PTV_VP_BUFF_CNT - 1;

	hdmi_audio_class = class_create(THIS_MODULE, "hdmi_audio");
	if (IS_ERR(hdmi_audio_class))
		pr_err("Failed to create class(hdmi_audio)!\n");
	hdmi_audio_dev = device_create(hdmi_audio_class, NULL, 0, NULL,
								"hdmi_audio");
	if (IS_ERR(hdmi_audio_dev))
		pr_err("Failed to create device(hdmi_audio_dev)!\n");

	if (device_create_file(hdmi_audio_dev,
					&dev_attr_hdmi_audio_set_ext) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
					dev_attr_hdmi_audio_set_ext.attr.name);

	return 0;

err:
	return -ENODEV;
}

static int s5p_tvout_remove(struct platform_device *pdev)
{
#if defined(CONFIG_S5P_SYSMMU_TV) && defined(CONFIG_S5P_VMEM)
	sysmmu_off(SYSMMU_TV);
	tvout_dbg("sysmmu off\n");
#endif
	s5p_vp_ctrl_destructor();
	s5p_tvif_ctrl_destructor();
	s5p_mixer_ctrl_destructor();

	s5p_tvout_v4l2_destructor();

	clk_disable(s5ptv_status.sclk_hdmi);

	clk_put(s5ptv_status.sclk_hdmi);
	clk_put(s5ptv_status.sclk_dac);
	clk_put(s5ptv_status.sclk_pixel);
	clk_put(s5ptv_status.sclk_hdmiphy);

	s5p_tvout_pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
static void s5p_tvout_early_suspend(struct early_suspend *h)
{
	tvout_dbg("\n");
	mutex_lock(&s5p_tvout_mutex);
	s5p_vp_ctrl_suspend();
	s5p_mixer_ctrl_suspend();
	s5p_tvif_ctrl_suspend();
	suspend_status = 1;
	tvout_dbg("suspend_status is true\n");
	mutex_unlock(&s5p_tvout_mutex);

	return;
}

static void s5p_tvout_late_resume(struct early_suspend *h)
{
	tvout_dbg("\n");
	mutex_lock(&s5p_tvout_mutex);
	suspend_status = 0;
	tvout_dbg("suspend_status is false\n");
	s5p_tvif_ctrl_resume();
	s5p_mixer_ctrl_resume();
	s5p_vp_ctrl_resume();
	mutex_unlock(&s5p_tvout_mutex);

	return;
}

void s5p_tvout_mutex_lock()
{
	mutex_lock(&s5p_tvout_mutex);
}

void s5p_tvout_mutex_unlock()
{
	mutex_unlock(&s5p_tvout_mutex);
}
#endif

static void s5p_tvout_resume_work(void *arg)
{
	mutex_lock(&s5p_tvout_mutex);
	s5p_hdmi_ctrl_phy_power_resume();
	mutex_unlock(&s5p_tvout_mutex);
}

static int s5p_tvout_suspend(struct device *dev)
{
	tvout_dbg("\n");
	return 0;
}
static int s5p_tvout_resume(struct device *dev)
{
	tvout_dbg("\n");
	queue_work_on(0, tvout_resume_wq, &tvout_resume_work);
	return 0;
}

static int s5p_tvout_runtime_suspend(struct device *dev)
{
	tvout_dbg("\n");
	return 0;
}

static int s5p_tvout_runtime_resume(struct device *dev)
{
	tvout_dbg("\n");
	return 0;
}
#else
#define s5p_tvout_suspend		NULL
#define s5p_tvout_resume		NULL
#define s5p_tvout_runtime_suspend	NULL
#define s5p_tvout_runtime_resume	NULL
#endif

static const struct dev_pm_ops s5p_tvout_pm_ops = {
	.suspend		= s5p_tvout_suspend,
	.resume			= s5p_tvout_resume,
	.runtime_suspend	= s5p_tvout_runtime_suspend,
	.runtime_resume		= s5p_tvout_runtime_resume
};

static struct platform_driver s5p_tvout_driver = {
	.probe	= s5p_tvout_probe,
	.remove	= s5p_tvout_remove,
	.driver	= {
		.name	= "s5p-tvout",
		.owner	= THIS_MODULE,
		.pm	= &s5p_tvout_pm_ops
	},
};

static char banner[] __initdata =
	KERN_INFO "S5P TVOUT Driver v3.0 (c) 2010 Samsung Electronics\n";

static int __init s5p_tvout_init(void)
{
	int ret;

	printk(banner);

	ret = platform_driver_register(&s5p_tvout_driver);

	if (ret) {
		printk(KERN_ERR "Platform Device Register Failed %d\n", ret);

		return -1;
	}
#ifdef CONFIG_PM
	tvout_resume_wq = create_freezeable_workqueue("tvout resume work");
	INIT_WORK(&tvout_resume_work, (work_func_t) s5p_tvout_resume_work);
#endif

	return 0;
}

static void __exit s5p_tvout_exit(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	mutex_destroy(&s5p_tvout_mutex);
#endif
	platform_driver_unregister(&s5p_tvout_driver);
}

late_initcall(s5p_tvout_init);
module_exit(s5p_tvout_exit);

MODULE_AUTHOR("SangPil Moon");
MODULE_DESCRIPTION("S5P TVOUT driver");
MODULE_LICENSE("GPL");
