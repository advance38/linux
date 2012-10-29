/*
 *  include/linux/hot_tracking.h
 *
 * This file has definitions for VFS hot data tracking
 * structures etc.
 *
 * Copyright (C) 2012 IBM Corp. All rights reserved.
 * Written by Zhi Yong Wu <wuzhy@linux.vnet.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 */

#ifndef _LINUX_HOTTRACK_H
#define _LINUX_HOTTRACK_H

#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/kref.h>
#include <linux/fs.h>

#define HEAT_MAP_BITS 8
#define HEAT_MAP_SIZE (1 << HEAT_MAP_BITS)

#define HOT_NAME_MAX 16

/*
 * A frequency data struct holds values that are used to
 * determine temperature of files and file ranges. These structs
 * are members of hot_inode_item and hot_range_item
 */
struct hot_freq_data {
	struct timespec last_read_time;
	struct timespec last_write_time;
	u32 nr_reads;
	u32 nr_writes;
	u64 avg_delta_reads;
	u64 avg_delta_writes;
	u32 flags;
	u32 last_temp;
};

/* List heads in hot map array */
struct hot_map_head {
	struct list_head node_list;
	u8 temp;
};

/* The common info for both following structures */
struct hot_comm_item {
	struct hot_freq_data hot_freq_data;  /* frequency data */
	spinlock_t lock; /* protects object data */
	struct kref refs;  /* prevents kfree */
	struct list_head n_list; /* list node index */
};

/* An item representing an inode and its access frequency */
struct hot_inode_item {
	struct hot_comm_item hot_inode; /* node in hot_inode_tree */
	struct radix_tree_root hot_range_tree; /* tree of ranges */
	spinlock_t lock; /* protect range tree */
	struct radix_tree_root *hot_inode_tree;
	u64 i_ino; /* inode number from inode */
};

/*
 * An item representing a range inside of
 * an inode whose frequency is being tracked
 */
struct hot_range_item {
	struct hot_comm_item hot_range;
	struct hot_inode_item *hot_inode; /* associated hot_inode_item */
	u32 start; /* item index in hot_range_tree */
	u32 len; /* length in bytes */
};

typedef u64 (hot_rw_freq_calc_fn) (struct timespec old_atime,
			struct timespec cur_time, u64 old_avg);
typedef u32 (hot_temp_calc_fn) (struct hot_freq_data *freq_data);
typedef bool (hot_is_obsolete_fn) (struct hot_freq_data *freq_data);

struct hot_func_ops {
	hot_rw_freq_calc_fn *hot_rw_freq_calc_fn;
	hot_temp_calc_fn *hot_temp_calc_fn;
	hot_is_obsolete_fn *hot_is_obsolete_fn;
};

/* identifies an hot func type */
struct hot_func_type {
	char hot_func_name[HOT_NAME_MAX];
	/* fields provided by specific FS */
	struct hot_func_ops ops;
	struct list_head list;
};

struct hot_info {
	struct radix_tree_root hot_inode_tree;
	spinlock_t lock; /*protect inode tree */

	/* map of inode temperature */
	struct hot_map_head heat_inode_map[HEAT_MAP_SIZE];
	/* map of range temperature */
	struct hot_map_head heat_range_map[HEAT_MAP_SIZE];
	unsigned int hot_map_nr;

	struct workqueue_struct *update_wq;
	struct delayed_work update_work;
	struct hot_func_type *hot_func_type;
	struct shrinker hot_shrink;
};

extern void __init hot_cache_init(void);
extern int hot_track_init(struct super_block *sb);
extern void hot_track_exit(struct super_block *sb);
extern void hot_update_freqs(struct inode *inode, u64 start,
				u64 len, int rw);

extern int hot_func_register(struct hot_func_type *h);
extern void hot_func_unregister(struct hot_func_type *h);

#endif  /* _LINUX_HOTTRACK_H */
