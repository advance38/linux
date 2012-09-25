/* common.h - generic red-black tree test functions for use in both kernel and
 * user space.
 * Copyright (C) 2012  Daniel Santos <daniel.santos@pobox.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * The following preprocessor variables should be defined to either 1 or 0 when
 * compiling common.{c,h}
 *
 * GRBTEST_USERLAND		If non-zero, compile for userspace, kernelspace
 * 				otherwise.
 * GRBTEST_BUILD_GENERIC	If non-zero, use generic implementation,
 * 				otherwise, use hand-coded implementation.
 * GRBTEST_KEY_TYPE
 * GRBTEST_USE_LEFTMOST		Maintain a pointer to the leftmost (smallest)
 * 				in all insert & delete operations.
 * GRBTEST_USE_RIGHTMOST	Same as above, except for rightmost (greatest)
 * 				value
 * GRBTEST_USE_COUNT		Maintain a count of objects in tree
 * GRBTEST_UNIQUE_KEYS		If non-zero, tree contains only unique keys.
 * GRBTEST_INSERT_REPLACES	Valid only if GRBTEST_UNIQUE_KEYS is non-zero.
 * 				If non-zero, insert function will replace any
 * 				existing object with the same key.
 * GRBTEST_USE_AUGMENTED	Simulate an augmented tree (partially
 * 				implemented)
 */
#ifndef _GRBTESTCOMMON_H
#define _GRBTESTCOMMON_H

#include <linux/rbtree.h>

#ifndef GRBTEST_USERLAND
# error GRBTEST_USERLAND not defined
#endif
#ifndef GRBTEST_BUILD_GENERIC
# error GRBTEST_BUILD_GENERIC not defined
#endif
#ifndef GRBTEST_USE_LEFTMOST
# error GRBTEST_USE_LEFTMOST not defined
#endif
#ifndef GRBTEST_KEY_TYPE
# error GRBTEST_KEY_TYPE not defined
#endif
#ifndef GRBTEST_USE_RIGHTMOST
# error GRBTEST_USE_RIGHTMOST not defined
#endif
#ifndef GRBTEST_USE_COUNT
# error GRBTEST_USE_COUNT not defined
#endif
#ifndef GRBTEST_UNIQUE_KEYS
# error GRBTEST_UNIQUE_KEYS not defined
#endif
#ifndef GRBTEST_INSERT_REPLACES
# error GRBTEST_INSERT_REPLACES not defined
#endif
#ifndef GRBTEST_USE_AUGMENTED
# error GRBTEST_INSERT_REPLACES not defined
#endif

#define strize(arg) strize2(arg)
#define strize2(arg) #arg

#ifdef __GNUC__
# define GRBTEST_COMPILER "gcc-" \
		strize(__GNUC__) "." \
		strize(__GNUC_MINOR__) "." \
		strize(__GNUC_PATCHLEVEL__)
#else
/* TODO: Add support for other compilers here */
# define GRBTEST_COMPILER "non-gcc"
#endif


/**
 * grbtest_config - Returns a string describing the build configuration.
 */
static inline const char *grbtest_config(void)
{
	return
		"key type        " strize(GRBTEST_KEY_TYPE) "\n"
#if GRBTEST_BUILD_GENERIC
		"type            generic\n"
#else
		"type            hand-coded\n"
#endif
		"use leftmost    " strize(GRBTEST_USE_LEFTMOST) "\n"
		"use rightmost   " strize(GRBTEST_USE_RIGHTMOST) "\n"
		"use count       " strize(GRBTEST_USE_COUNT) "\n"
		"unique keys     " strize(GRBTEST_UNIQUE_KEYS) "\n"
		"insert replaces " strize(GRBTEST_INSERT_REPLACES) "\n"
		"augmented       " strize(GRBTEST_USE_AUGMENTED) "\n"
		"DEBUG_RBTREE    "
		strize(config_enabled(CONFIG_DEBUG_RBTREE)) "\n"
		"DEBUG_RBTREE_VALIDATE "
		strize(config_enabled(CONFIG_DEBUG_RBTREE_VALIDATE)) "\n"
		"CFLAGS          " strize(GRBTEST_CFLAGS) "\n"
		"CC              " strize(GRBTEST_CC) "\n";

}

/* Functions provided by {module,user}/facilities.c for common.c */

extern void facilities_init(void);
extern u64 getCurTicks(void);
extern void *rand_init(u64 *seed);

//typedef unsigned int uint;


#if GRBTEST_USERLAND
#   define print_msg(...)	printf(__VA_ARGS__)
#   define print_err(...)	fprintf(stderr, __VA_ARGS__)

static inline void *mem_alloc(size_t size)	{return malloc(size);}
static inline void mem_free(void *ptr)		{return free(ptr);}
static inline u32 rand_get(void *state) 	{return rand();}
static inline void rand_free(void *state)	{}

#else /* GRBTEST_USERLAND */

#    define print_msg(...) printk("grbtest: " __VA_ARGS__)
#    define print_err(...) printk(KERN_ALERT "grbtest: " __VA_ARGS__)

static inline void *mem_alloc(size_t size) {return kzalloc(size, GFP_KERNEL);}
static inline void mem_free(void *ptr)	   {return kfree(ptr);}
static inline u32 rand_get(struct rnd_state *state) {return prandom32(state);}
static inline void rand_free(struct rnd_state *state) {kzfree(state);}

#endif

/****************************************************************************
 * Structures
 */

struct object {
	struct rb_node node;
	GRBTEST_KEY_TYPE key;
};

struct container {
	struct rb_root	tree;
	unsigned int	count;
	struct rb_node	*leftmost;
	struct rb_node	*rightmost;
	unsigned int	pool_in_use;
};

struct object_pools {
	struct object	**pools;
	unsigned int	pool_count;	/**< number of pools */
	unsigned int	object_count;	/**< num objects in each pool */
	size_t		pool_size;	/**< size of each pool in bytes */
};

enum grbtest_type {
	GRBTEST_TYPE_INSERTION,
	GRBTEST_TYPE_INSERTION_DELETION,
	GRBTEST_TYPE_VALIDATE_INSERTIONS,
	GRBTEST_TYPE_COUNT
};

extern const char *grbtest_type_desc[GRBTEST_TYPE_COUNT];

struct grbtest_config {
	enum grbtest_type test;
	u64		  in_seed;
	u64		  seed;
	u32		  key_mask;
	unsigned	  object_count;
	unsigned	  pool_count;
	unsigned	  reps;
	int		  human_readable;
	char		 *delimiter;
	char		 *text_enclosure;
	int		  print_header;
};

struct grbtest_result {
	unsigned	node_size;
	unsigned	object_size;
	unsigned	pool_size;
	u64		insertions;
	u64		insertion_time;
	u64		evictions;
	u64		deletions;
	u64		deletion_time;
};

/****************************************************************************
 * Global data
 */

extern struct object_pools objects;


static void inline init_container(struct container *cont)
{
	cont->tree	= RB_ROOT;
	cont->count	= 0;
	cont->leftmost	= 0;
	cont->rightmost	= 0;
}

static void inline init_object(struct object *obj, GRBTEST_KEY_TYPE key)
{
	rb_init_node(&obj->node);
	obj->key = key;
}

static int inline is_inserted(struct object *obj)
{
	return rb_parent(&obj->node) != &obj->node;
}

/* exported by common.c */
extern long grbtest_init(struct grbtest_config *config);
extern void grbtest_cleanup(void);
extern long grbtest_run_test(
		struct grbtest_config *config,
		struct grbtest_result *result,
		struct container *cont);
extern long grbtest_perftest(
		struct grbtest_config *config,
		struct grbtest_result *result,
		struct container *cont,
		int do_deletes);
extern long grbtest_validate_insertion(
		struct grbtest_config *config,
		struct grbtest_result *result,
		struct container *cont);

extern void grbtest_init_results(const struct grbtest_config *config,
				 struct grbtest_result *result);
extern void grbtest_print_result_header(const struct grbtest_config *config);
extern void grbtest_print_result_row(const struct grbtest_config *config,
				     const struct grbtest_result *result);

#endif /* _GRBTESTCOMMON_H */
