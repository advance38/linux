/*
 * fs/hot_tracking.c
 *
 * Copyright (C) 2012 IBM Corp. All rights reserved.
 * Written by Zhi Yong Wu <wuzhy@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 */

#include <linux/list.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/limits.h>
#include "hot_tracking.h"

/* kmem_cache pointers for slab caches */
static struct kmem_cache *hot_inode_item_cachep __read_mostly;
static struct kmem_cache *hot_range_item_cachep __read_mostly;

/*
 * Initialize the inode tree. Should be called for each new inode
 * access or other user of the hot_inode interface.
 */
static void hot_inode_tree_init(struct hot_info *root)
{
	INIT_RADIX_TREE(&root->hot_inode_tree, GFP_ATOMIC);
	spin_lock_init(&root->lock);
}

/*
 * Initialize the hot range tree. Should be called for each new inode
 * access or other user of the hot_range interface.
 */
void hot_range_tree_init(struct hot_inode_item *he)
{
	INIT_RADIX_TREE(&he->hot_range_tree, GFP_ATOMIC);
	spin_lock_init(&he->lock);
}

/*
 * Initialize a new hot_range_item structure. The new structure is
 * returned with a reference count of one and needs to be
 * freed using free_range_item()
 */
static void hot_range_item_init(struct hot_range_item *hr, u32 start,
				struct hot_inode_item *he)
{
	hr->start = start;
	hr->len = RANGE_SIZE;
	hr->hot_inode = he;
	kref_init(&hr->hot_range.refs);
	spin_lock_init(&hr->hot_range.lock);
	INIT_LIST_HEAD(&hr->hot_range.n_list);
	hr->hot_range.hot_freq_data.avg_delta_reads = (u64) -1;
	hr->hot_range.hot_freq_data.avg_delta_writes = (u64) -1;
	hr->hot_range.hot_freq_data.flags = FREQ_DATA_TYPE_RANGE;
}

/*
 * Initialize a new hot_inode_item structure. The new structure is
 * returned with a reference count of one and needs to be
 * freed using hot_free_inode_item()
 */
static void hot_inode_item_init(struct hot_inode_item *he, u64 ino,
				struct radix_tree_root *hot_inode_tree)
{
	he->i_ino = ino;
	he->hot_inode_tree = hot_inode_tree;
	kref_init(&he->hot_inode.refs);
	spin_lock_init(&he->hot_inode.lock);
	INIT_LIST_HEAD(&he->hot_inode.n_list);
	he->hot_inode.hot_freq_data.avg_delta_reads = (u64) -1;
	he->hot_inode.hot_freq_data.avg_delta_writes = (u64) -1;
	he->hot_inode.hot_freq_data.flags = FREQ_DATA_TYPE_INODE;
	hot_range_tree_init(he);
}

static void hot_range_item_free(struct kref *kref)
{
	struct hot_comm_item *comm_item = container_of(kref,
		struct hot_comm_item, refs);
	struct hot_range_item *hr = container_of(comm_item,
		struct hot_range_item, hot_range);
	struct hot_info *root = container_of(
			hr->hot_inode->hot_inode_tree,
		struct hot_info, hot_inode_tree);

	spin_lock(&hr->hot_range.lock);
	if (!list_empty(&hr->hot_range.n_list)) {
		list_del_init(&hr->hot_range.n_list);
		root->hot_map_nr--;
	}
	spin_unlock(&hr->hot_range.lock);

	radix_tree_delete(&hr->hot_inode->hot_range_tree, hr->start);
	kmem_cache_free(hot_range_item_cachep, hr);
}

/*
 * Drops the reference out on hot_range_item by one
 * and free the structure if the reference count hits zero
 */
static void hot_range_item_put(struct hot_range_item *hr)
{
	kref_put(&hr->hot_range.refs, hot_range_item_free);
}

/* Frees the entire hot_range_tree. */
static void hot_range_tree_free(struct hot_inode_item *he)
{
	struct hot_range_item *hr_nodes[8];
	u32 start = 0;
	int i, n;

	while (1) {
		spin_lock(&he->lock);
		n = radix_tree_gang_lookup(&he->hot_range_tree,
					(void **)hr_nodes, start,
					ARRAY_SIZE(hr_nodes));
		if (!n) {
			spin_unlock(&he->lock);
			break;
		}

		start = hr_nodes[n - 1]->start + 1;
		for (i = 0; i < n; i++)
			hot_range_item_put(hr_nodes[i]);
		spin_unlock(&he->lock);
	}
}

static void hot_inode_item_free(struct kref *kref)
{
	struct hot_comm_item *comm_item = container_of(kref,
			struct hot_comm_item, refs);
	struct hot_inode_item *he = container_of(comm_item,
			struct hot_inode_item, hot_inode);
	struct hot_info *root = container_of(he->hot_inode_tree,
		struct hot_info, hot_inode_tree);

	spin_lock(&he->hot_inode.lock);
	if (!list_empty(&he->hot_inode.n_list)) {
		list_del_init(&he->hot_inode.n_list);
		root->hot_map_nr--;
	}
	spin_unlock(&he->hot_inode.lock);

	hot_range_tree_free(he);
	radix_tree_delete(he->hot_inode_tree, he->i_ino);
	kmem_cache_free(hot_inode_item_cachep, he);
}

/*
 * Drops the reference out on hot_inode_item by one
 * and free the structure if the reference count hits zero
 */
void hot_inode_item_put(struct hot_inode_item *he)
{
	kref_put(&he->hot_inode.refs, hot_inode_item_free);
}
EXPORT_SYMBOL_GPL(hot_inode_item_put);

/* Frees the entire hot_inode_tree. */
static void hot_inode_tree_exit(struct hot_info *root)
{
	struct hot_inode_item *hi_nodes[8];
	u64 ino = 0;
	int i, n;

	while (1) {
		spin_lock(&root->lock);
		n = radix_tree_gang_lookup(&root->hot_inode_tree,
					   (void **)hi_nodes, ino,
					   ARRAY_SIZE(hi_nodes));
		if (!n) {
			spin_unlock(&root->lock);
			break;
		}

		ino = hi_nodes[n - 1]->i_ino + 1;
		for (i = 0; i < n; i++)
			hot_inode_item_put(hi_nodes[i]);
		spin_unlock(&root->lock);
	}
}

struct hot_inode_item
*hot_inode_item_find(struct hot_info *root, u64 ino)
{
	struct hot_inode_item *he;
	int ret;

again:
	spin_lock(&root->lock);
	he = radix_tree_lookup(&root->hot_inode_tree, ino);
	if (he) {
		kref_get(&he->hot_inode.refs);
		spin_unlock(&root->lock);
		return he;
	}
	spin_unlock(&root->lock);

	he = kmem_cache_zalloc(hot_inode_item_cachep,
				GFP_KERNEL | GFP_NOFS);
	if (!he)
		return ERR_PTR(-ENOMEM);

	hot_inode_item_init(he, ino, &root->hot_inode_tree);

	ret = radix_tree_preload(GFP_NOFS & ~__GFP_HIGHMEM);
	if (ret) {
		kmem_cache_free(hot_inode_item_cachep, he);
		return ERR_PTR(ret);
	}

	spin_lock(&root->lock);
	ret = radix_tree_insert(&root->hot_inode_tree, ino, he);
	if (ret == -EEXIST) {
		kmem_cache_free(hot_inode_item_cachep, he);
		spin_unlock(&root->lock);
		radix_tree_preload_end();
		goto again;
	}
	spin_unlock(&root->lock);
	radix_tree_preload_end();

	kref_get(&he->hot_inode.refs);
	return he;
}
EXPORT_SYMBOL_GPL(hot_inode_item_find);

static struct hot_range_item
*hot_range_item_find(struct hot_inode_item *he,
			u32 start)
{
	struct hot_range_item *hr;
	int ret;

again:
	spin_lock(&he->lock);
	hr = radix_tree_lookup(&he->hot_range_tree, start);
	if (hr) {
		kref_get(&hr->hot_range.refs);
		spin_unlock(&he->lock);
		return hr;
	}
	spin_unlock(&he->lock);

	hr = kmem_cache_zalloc(hot_range_item_cachep,
				GFP_KERNEL | GFP_NOFS);
	if (!hr)
		return ERR_PTR(-ENOMEM);

	hot_range_item_init(hr, start, he);

	ret = radix_tree_preload(GFP_NOFS & ~__GFP_HIGHMEM);
	if (ret) {
		kmem_cache_free(hot_range_item_cachep, hr);
		return ERR_PTR(ret);
	}

	spin_lock(&he->lock);
	ret = radix_tree_insert(&he->hot_range_tree, start, hr);
	if (ret == -EEXIST) {
		kmem_cache_free(hot_range_item_cachep, hr);
		spin_unlock(&he->lock);
		radix_tree_preload_end();
		goto again;
	}
	spin_unlock(&he->lock);
	radix_tree_preload_end();

	kref_get(&hr->hot_range.refs);
	return hr;
}

/*
 * This function does the actual work of updating
 * the frequency numbers, whatever they turn out to be.
 */
static u64 hot_average_update(struct timespec old_atime,
		struct timespec cur_time, u64 old_avg)
{
	struct timespec delta_ts;
	u64 new_avg;
	u64 new_delta;

	delta_ts = timespec_sub(cur_time, old_atime);
	new_delta = timespec_to_ns(&delta_ts) >> FREQ_POWER;

	new_avg = (old_avg << FREQ_POWER) - old_avg + new_delta;
	new_avg = new_avg >> FREQ_POWER;

	return new_avg;
}

static void hot_freq_data_update(struct hot_freq_data *freq_data, bool write)
{
	struct timespec cur_time = current_kernel_time();

	if (write) {
		freq_data->nr_writes += 1;
		freq_data->avg_delta_writes = hot_average_update(
				freq_data->last_write_time,
				cur_time,
				freq_data->avg_delta_writes);
		freq_data->last_write_time = cur_time;
	} else {
		freq_data->nr_reads += 1;
		freq_data->avg_delta_reads = hot_average_update(
				freq_data->last_read_time,
				cur_time,
				freq_data->avg_delta_reads);
		freq_data->last_read_time = cur_time;
	}
}

static u64 hot_raw_shift(u64 counter, u32 bits, bool dir)
{
	if (dir)
		return counter << bits;
	else
		return counter >> bits;
}

/*
 * hot_temp_calc() is responsible for distilling the six heat
 * criteria down into a single temperature value for the data,
 * which is an integer between 0 and HEAT_MAX_VALUE.
 */
static u32 hot_temp_calc(struct hot_freq_data *freq_data)
{
	u32 result = 0;

	struct timespec ckt = current_kernel_time();
	u64 cur_time = timespec_to_ns(&ckt);

	u32 nrr_heat = (u32)hot_raw_shift((u64)freq_data->nr_reads,
					NRR_MULTIPLIER_POWER, true);
	u32 nrw_heat = (u32)hot_raw_shift((u64)freq_data->nr_writes,
					NRW_MULTIPLIER_POWER, true);

	u64 ltr_heat =
	hot_raw_shift((cur_time - timespec_to_ns(&freq_data->last_read_time)),
			LTR_DIVIDER_POWER, false);
	u64 ltw_heat =
	hot_raw_shift((cur_time - timespec_to_ns(&freq_data->last_write_time)),
			LTW_DIVIDER_POWER, false);

	u64 avr_heat =
	hot_raw_shift((((u64) -1) - freq_data->avg_delta_reads),
			AVR_DIVIDER_POWER, false);
	u64 avw_heat =
	hot_raw_shift((((u64) -1) - freq_data->avg_delta_writes),
			AVW_DIVIDER_POWER, false);

	/* ltr_heat is now guaranteed to be u32 safe */
	if (ltr_heat >= hot_raw_shift((u64) 1, 32, true))
		ltr_heat = 0;
	else
		ltr_heat = hot_raw_shift((u64) 1, 32, true) - ltr_heat;

	/* ltw_heat is now guaranteed to be u32 safe */
	if (ltw_heat >= hot_raw_shift((u64) 1, 32, true))
		ltw_heat = 0;
	else
		ltw_heat = hot_raw_shift((u64) 1, 32, true) - ltw_heat;

	/* avr_heat is now guaranteed to be u32 safe */
	if (avr_heat >= hot_raw_shift((u64) 1, 32, true))
		avr_heat = (u32) -1;

	/* avw_heat is now guaranteed to be u32 safe */
	if (avw_heat >= hot_raw_shift((u64) 1, 32, true))
		avw_heat = (u32) -1;

	nrr_heat = (u32)hot_raw_shift((u64)nrr_heat,
		(3 - NRR_COEFF_POWER), false);
	nrw_heat = (u32)hot_raw_shift((u64)nrw_heat,
		(3 - NRW_COEFF_POWER), false);
	ltr_heat = hot_raw_shift(ltr_heat, (3 - LTR_COEFF_POWER), false);
	ltw_heat = hot_raw_shift(ltw_heat, (3 - LTW_COEFF_POWER), false);
	avr_heat = hot_raw_shift(avr_heat, (3 - AVR_COEFF_POWER), false);
	avw_heat = hot_raw_shift(avw_heat, (3 - AVW_COEFF_POWER), false);

	result = nrr_heat + nrw_heat + (u32) ltr_heat +
		(u32) ltw_heat + (u32) avr_heat + (u32) avw_heat;

	return result;
}

/*
 * Initialize inode and range map arrays.
 */
static void hot_map_array_init(struct hot_info *root)
{
	int i;
	for (i = 0; i < HEAT_MAP_SIZE; i++) {
		INIT_LIST_HEAD(&root->heat_inode_map[i].node_list);
		INIT_LIST_HEAD(&root->heat_range_map[i].node_list);
		root->heat_inode_map[i].temp = i;
		root->heat_range_map[i].temp = i;
	}
}

static void hot_map_list_free(struct list_head *node_list,
				struct hot_info *root)
{
	struct list_head *pos, *next;
	struct hot_comm_item *node;

	list_for_each_safe(pos, next, node_list) {
		node = list_entry(pos, struct hot_comm_item, n_list);
		list_del_init(&node->n_list);
		root->hot_map_nr--;
	}

}

/* Free inode and range map arrays */
static void hot_map_array_exit(struct hot_info *root)
{
	int i;
	for (i = 0; i < HEAT_MAP_SIZE; i++) {
		hot_map_list_free(&root->heat_inode_map[i].node_list, root);
		hot_map_list_free(&root->heat_range_map[i].node_list, root);
	}
}

/*
 * Initialize kmem cache for hot_inode_item and hot_range_item.
 */
void __init hot_cache_init(void)
{
	hot_inode_item_cachep = kmem_cache_create("hot_inode_item",
			sizeof(struct hot_inode_item), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD,
			NULL);
	if (!hot_inode_item_cachep)
		return;

	hot_range_item_cachep = kmem_cache_create("hot_range_item",
			sizeof(struct hot_range_item), 0,
			SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD,
			NULL);
	if (!hot_range_item_cachep)
		goto err;

	return;

err:
	kmem_cache_destroy(hot_inode_item_cachep);
}
EXPORT_SYMBOL_GPL(hot_cache_init);

/*
 * Main function to update access frequency from read/writepage(s) hooks
 */
void hot_update_freqs(struct inode *inode, u64 start,
			u64 len, int rw)
{
	struct hot_info *root = inode->i_sb->s_hot_root;
	struct hot_inode_item *he;
	struct hot_range_item *hr;
	u32 cur, end;

	if (!root || (len == 0))
		return;

	he = hot_inode_item_find(root, inode->i_ino);
	if (IS_ERR(he)) {
		WARN_ON(1);
		return;
	}

	spin_lock(&he->hot_inode.lock);
	hot_freq_data_update(&he->hot_inode.hot_freq_data, rw);
	spin_unlock(&he->hot_inode.lock);

	/*
	 * Align ranges on RANGE_SIZE boundary
	 * to prevent proliferation of range structs
	 */
	end = (start + len + RANGE_SIZE - 1) >> RANGE_BITS;
	for (cur = (start >> RANGE_BITS); cur < end; cur++) {
		hr = hot_range_item_find(he, cur);
		if (IS_ERR(hr)) {
			WARN_ON(1);
			hot_inode_item_put(he);
			return;
		}

		spin_lock(&hr->hot_range.lock);
		hot_freq_data_update(&hr->hot_range.hot_freq_data, rw);
		spin_unlock(&hr->hot_range.lock);

		hot_range_item_put(hr);
	}

	hot_inode_item_put(he);
}
EXPORT_SYMBOL_GPL(hot_update_freqs);

/*
 * Initialize the data structures for hot data tracking.
 */
int hot_track_init(struct super_block *sb)
{
	struct hot_info *root;
	int ret = -ENOMEM;

	root = kzalloc(sizeof(struct hot_info), GFP_NOFS);
	if (!root) {
		printk(KERN_ERR "%s: Failed to malloc memory for "
				"hot_info\n", __func__);
		return ret;
	}

	sb->s_hot_root = root;
	hot_inode_tree_init(root);
	hot_map_array_init(root);

	printk(KERN_INFO "VFS: Turning on hot data tracking\n");

	return 0;
}
EXPORT_SYMBOL_GPL(hot_track_init);

void hot_track_exit(struct super_block *sb)
{
	struct hot_info *root = sb->s_hot_root;

	hot_map_array_exit(root);
	hot_inode_tree_exit(root);
	kfree(root);
}
EXPORT_SYMBOL_GPL(hot_track_exit);
