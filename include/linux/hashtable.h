/*
 * Statically sized hash table implementation
 * (C) 2012  Sasha Levin <levinsasha928@gmail.com>
 */

#ifndef _LINUX_HASHTABLE_H
#define _LINUX_HASHTABLE_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/hash.h>
#include <linux/rculist.h>

#define DEFINE_HASHTABLE(name, bits)						\
	struct hlist_head name[HASH_SIZE(bits)] =				\
			{ [0 ... HASH_SIZE(bits) - 1] = HLIST_HEAD_INIT }

#define DECLARE_HASHTABLE(name, bits)                                   	\
	struct hlist_head name[1 << (bits)]

#define HASH_SIZE(name) (1 << HASH_BITS(name))
#define HASH_BITS(name) ilog2(ARRAY_SIZE(name))

/* Use hash_32 when possible to allow for fast 32bit hashing in 64bit kernels. */
#define hash_min(val, bits)							\
({										\
	sizeof(val) <= 4 ?							\
	hash_32(val, bits) :							\
	hash_long(val, bits);							\
})

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_init(hashtable)							\
({										\
	int __i;								\
										\
	for (__i = 0; __i < HASH_SIZE(hashtable); __i++)			\
		INIT_HLIST_HEAD(&hashtable[__i]);				\
})

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	hlist_add_head(node, &hashtable[hash_min(key, HASH_BITS(hashtable))]);

/**
 * hash_add_rcu - add an object to a rcu enabled hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add_rcu(hashtable, node, key)					\
	hlist_add_head_rcu(node, &hashtable[hash_min(key, HASH_BITS(hashtable))]);

/**
 * hash_hashed - check whether an object is in any hashtable
 * @node: the &struct hlist_node of the object to be checked
 */
#define hash_hashed(node) (!hlist_unhashed(node))

/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_empty(hashtable)							\
({										\
	int __i;								\
	bool __ret = true;							\
										\
	for (__i = 0; __i < HASH_SIZE(hashtable); __i++)			\
		if (!hlist_empty(&hashtable[__i]))				\
			__ret = false;						\
										\
	__ret;									\
})

/**
 * hash_del - remove an object from a hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del(struct hlist_node *node)
{
	hlist_del_init(node);
}

/**
 * hash_del_rcu - remove an object from a rcu enabled hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del_rcu(struct hlist_node *node)
{
	hlist_del_init_rcu(node);
}

/**
 * hash_for_each - iterate over a hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each(name, bkt, node, obj, member)				\
	for (bkt = 0, node = NULL; node == NULL && bkt < HASH_SIZE(name); bkt++)\
		hlist_for_each_entry(obj, node, &name[bkt], member)

/**
 * hash_for_each_rcu - iterate over a rcu enabled hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_rcu(name, bkt, node, obj, member)				\
	for (bkt = 0, node = NULL; node == NULL && bkt < HASH_SIZE(name); bkt++)\
		hlist_for_each_entry_rcu(obj, node, &name[bkt], member)

/**
 * hash_for_each_safe - iterate over a hashtable safe against removal of
 * hash entry
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @tmp: a &struct used for temporary storage
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_safe(name, bkt, node, tmp, obj, member)			\
	for (bkt = 0, node = NULL; node == NULL && bkt < HASH_SIZE(name); bkt++)\
		hlist_for_each_entry_safe(obj, node, tmp, &name[bkt], member)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible(name, obj, node, member, key)			\
	hlist_for_each_entry(obj, node,	&name[hash_min(key, HASH_BITS(name))], member)

/**
 * hash_for_each_possible_rcu - iterate over all possible objects hashing to the
 * same bucket in an rcu enabled hashtable
 * in a rcu enabled hashtable
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_rcu(name, obj, node, member, key)		\
	hlist_for_each_entry_rcu(obj, node, &name[hash_min(key, HASH_BITS(name))], member)

/**
 * hash_for_each_possible_safe - iterate over all possible objects hashing to the
 * same bucket safe against removals
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @tmp: a &struct used for temporary storage
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_safe(name, obj, node, tmp, member, key)		\
	hlist_for_each_entry_safe(obj, node, tmp,				\
		&name[hash_min(key, HASH_BITS(name))], member)


#endif
