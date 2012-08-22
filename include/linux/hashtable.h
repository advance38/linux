/*
 * Hash table implementation
 * (C) 2012  Sasha Levin <levinsasha928@gmail.com>
 */

#ifndef _LINUX_HASHTABLE_H
#define _LINUX_HASHTABLE_H

#include <linux/list.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/hash.h>
#include <linux/rculist.h>

#define DEFINE_HASHTABLE(name, bits)					\
	struct hlist_head name[HASH_SIZE(bits)];

#define HASH_SIZE(bits) (1 << (bits))
#define HASH_BITS(name) (ilog2(ARRAY_SIZE(name)))
#define HASH_REQUIRED_SIZE(bits) (sizeof(struct hlist_head) * HASH_SIZE(bits))

/* Use hash_32 when possible to allow for fast 32bit hashing in 64bit kernels. */
#define hash_min(val, bits) ((sizeof(val)==4) ? hash_32((val), (bits)) : hash_long((val), (bits)))

/**
 * hash_init_size - initialize a hash table
 * @hashtable: hashtable to be initialized
 * @bits: bit count of hashing function
 *
 * Initializes a hash table with 2**bits buckets.
 */
static inline void hash_init_size(struct hlist_head *hashtable, int bits)
{
	int i;

	for (i = 0; i < HASH_SIZE(bits); i++)
		INIT_HLIST_HEAD(hashtable + i);
}

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 */
#define hash_init(name) hash_init_size(name, HASH_BITS(name))

/**
 * hash_add_size - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @bits: bit count used for hashing
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add_size(hashtable, bits, node, key)				\
	hlist_add_head(node, &hashtable[hash_min(key, bits)]);

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	hash_add_size(hashtable, HASH_BITS(hashtable), node, key)

/**
 * hash_add_rcu_size - add an object to a rcu enabled hashtable
 * @hashtable: hashtable to add to
 * @bits: bit count used for hashing
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add_rcu_size(hashtable, bits, node, key)				\
	hlist_add_head_rcu(node, &hashtable[hash_min(key, bits)]);

/**
 * hash_add_rcu - add an object to a rcu enabled hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add_rcu(hashtable, node, key)					\
	hash_add_rcu_size(hashtable, HASH_BITS(hashtable), node, key)

/**
 * hash_hashed - check whether an object is in any hashtable
 * @node: the &struct hlist_node of the object to be checked
 */
#define hash_hashed(node) (!hlist_unhashed(node))

/**
 * hash_empty_size - check whether a hashtable is empty
 * @hashtable: hashtable to check
 * @bits: bit count used for hashing
 */
static inline bool hash_empty_size(struct hlist_head *hashtable, int bits)
{
	int i;

	for (i = 0; i < HASH_SIZE(bits); i++)
		if (!hlist_empty(&hashtable[i]))
			return false;

	return true;
}

/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 */
#define hash_empty(name) hash_empty_size(name, HASH_BITS(name))

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
 * hash_for_each_size - iterate over a hashtable
 * @name: hashtable to iterate
 * @bits: bit count of hashing function of the hashtable
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_size(name, bits, bkt, node, obj, member)			\
	for (bkt = 0; bkt < HASH_SIZE(bits); bkt++)				\
		hlist_for_each_entry(obj, node, &name[bkt], member)

/**
 * hash_for_each - iterate over a hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each(name, bkt, node, obj, member)				\
	hash_for_each_size(name, HASH_BITS(name), bkt, node, obj, member)

/**
 * hash_for_each_rcu_size - iterate over a rcu enabled hashtable
 * @name: hashtable to iterate
 * @bits: bit count of hashing function of the hashtable
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_rcu_size(name, bits, bkt, node, obj, member)		\
	for (bkt = 0; bkt < HASH_SIZE(bits); bkt++)				\
		hlist_for_each_entry_rcu(obj, node, &name[bkt], member)

/**
 * hash_for_each_rcu - iterate over a rcu enabled hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_rcu(name, bkt, node, obj, member)				\
	hash_for_each_rcu_size(name, HASH_BITS(name), bkt, node, obj, member)

/**
 * hash_for_each_safe_size - iterate over a hashtable safe against removal of
 * hash entry
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @tmp: another &struct hlist_node to use as temporary storage
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_safe_size(name, bits, bkt, node, tmp, obj, member)	\
	for (bkt = 0; bkt < HASH_SIZE(bits); bkt++)                     	\
		hlist_for_each_entry_safe(obj, node, tmp, &name[bkt], member)

/**
 * hash_for_each_safe_size - iterate over a hashtable safe against removal of
 * hash entry
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_safe(name, bkt, node, tmp, obj, member)			\
	hash_for_each_safe_size(name, HASH_BITS(name), bkt, node,		\
				tmp, obj, member)

/**
 * hash_for_each_possible - iterate over all possible objects hasing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @bits: bit count of hashing function of the hashtable
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_size(name, obj, bits, node, member, key)		\
	hlist_for_each_entry(obj, node,	&name[hash_min(key, bits)], member)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @bits: bit count of hashing function of the hashtable
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible(name, obj, node, member, key)			\
	hash_for_each_possible_size(name, obj, HASH_BITS(name), node, member, key)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * in a rcu enabled hashtable
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @bits: bit count of hashing function of the hashtable
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_rcu_size(name, obj, bits, node, member, key)	\
	hlist_for_each_entry_rcu(obj, node, &name[hash_min(key, bits)], member)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * in a rcu enabled hashtable
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_rcu(name, obj, node, member, key)		\
	hash_for_each_possible_rcu_size(name, obj, HASH_BITS(name),		\
					node, member, key)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * safe against removal of hash entry
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @bits: bit count of hashing function of the hashtable
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_safe_size(name, obj, bits, node, tmp, member, key)\
	hlist_for_each_entry_safe(obj, node, tmp,				\
		&name[hash_min(key, bits)], member)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * safe against removal of hash entry
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @node: the &struct list_head to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_safe(name, obj, node, tmp, member, key)		\
	hash_for_each_possible_safe_size(name, obj, HASH_BITS(name),		\
						node, tmp, member, key)

#endif
