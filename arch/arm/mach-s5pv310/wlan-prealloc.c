/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc. All rights reserved.
 *
 * This file contains the pre-allocated buffer APIs for AR6K module
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#define AR6K_MAX_VIRTUAL_SCATTER_BUFFER_SIZE        (18*1024)
#define AR6K_SCATTER_REQS                           4

static void *ar6k_virtual_scatter_pointer[AR6K_SCATTER_REQS];

/*
 * call ar6000_virtual_scatter_buffer_init() once on system boot up
 */
int ar6000_virtual_scatter_buffer_init(void)
{
	int i;

	if (ar6k_virtual_scatter_pointer[0]) {
		printk(KERN_ERR "AR6K: %s called already\n", __func__);
		return 0;
	}

	for (i = 0; i < AR6K_SCATTER_REQS; i++) {
		ar6k_virtual_scatter_pointer[i] =
			kmalloc(AR6K_MAX_VIRTUAL_SCATTER_BUFFER_SIZE,
					GFP_KERNEL);
		if (ar6k_virtual_scatter_pointer[i] == NULL) {
			printk(KERN_ERR "AR6K: %s(req = %d) error\n",
				__func__, i);
			return -1;
		}
	}

	printk(KERN_ERR "AR6K: %s success. : %d\n", __func__,
			 sizeof(ar6k_virtual_scatter_pointer));

	return 0;
}
EXPORT_SYMBOL(ar6000_virtual_scatter_buffer_init);

/*
 * return the memory preallocated in ar6000_virtual_scatter_buffer_init()
 */
void *ar6000_virtual_scatter_buffer_alloc(int req, int size)
{
	if (size > AR6K_MAX_VIRTUAL_SCATTER_BUFFER_SIZE) {
		printk(KERN_ERR "AR6K: %s(size = %d) error\n", __func__, size);
		return NULL;
	}

	if (req < AR6K_SCATTER_REQS) {
		printk(KERN_INFO "AR6K: %s(req = %d, size = %d) = %p\n",
				__func__, req, size,
				ar6k_virtual_scatter_pointer[req]);
		return ar6k_virtual_scatter_pointer[req];
	}
	printk(KERN_ERR "AR6K: %s(req = %d) error\n", __func__, req);

	return NULL;
}
EXPORT_SYMBOL(ar6000_virtual_scatter_buffer_alloc);

/*
 * memory won't get free actually
 */
int ar6000_virtual_scatter_buffer_free(int req)
{
	printk(KERN_INFO "AR6K: %s(req = %d)\n", __func__, req);
	if (req < AR6K_SCATTER_REQS)
		return 0;

	printk(KERN_ERR "AR6K: %s(req = %d) error\n", __func__, req);
	return -1;
}
EXPORT_SYMBOL(ar6000_virtual_scatter_buffer_free);

/*
 * memory gets free here
 */
void ar6000_virtual_scatter_buffer_deinit(void)
{
	int i;

	printk(KERN_INFO "AR6K: %s\n", __func__);

	if (ar6k_virtual_scatter_pointer[0] == NULL) {
		printk(KERN_ERR "AR6K: %s called but \
			ar6000_virtual_scatter_buffer_init()\
			not called\n", __func__);
		return;
	}

	for (i = 0; i < AR6K_SCATTER_REQS; i++) {
		kfree(ar6k_virtual_scatter_pointer[i]);
		ar6k_virtual_scatter_pointer[i] = NULL;
	}
}
EXPORT_SYMBOL(ar6000_virtual_scatter_buffer_deinit);
