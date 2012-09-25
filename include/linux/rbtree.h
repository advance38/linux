/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2012  Daniel Santos <daniel.santos@pobox.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  Some example of insert and search follows here. The search is a plain
  normal search over an ordered tree. The insert instead must be implemented
  in two steps: First, the code must insert the element in order as a red leaf
  in the tree, and then the support library function rb_insert_color() must
  be called. Such function will do the not trivial work to rebalance the
  rbtree, if necessary.

-----------------------------------------------------------------------
static inline struct page * rb_search_page_cache(struct inode * inode,
						 unsigned long offset)
{
	struct rb_node * n = inode->i_rb_page_cache.rb_node;
	struct page * page;

	while (n)
	{
		page = rb_entry(n, struct page, rb_page_cache);

		if (offset < page->offset)
			n = n->rb_left;
		else if (offset > page->offset)
			n = n->rb_right;
		else
			return page;
	}
	return NULL;
}

static inline struct page * __rb_insert_page_cache(struct inode * inode,
						   unsigned long offset,
						   struct rb_node * node)
{
	struct rb_node ** p = &inode->i_rb_page_cache.rb_node;
	struct rb_node * parent = NULL;
	struct page * page;

	while (*p)
	{
		parent = *p;
		page = rb_entry(parent, struct page, rb_page_cache);

		if (offset < page->offset)
			p = &(*p)->rb_left;
		else if (offset > page->offset)
			p = &(*p)->rb_right;
		else
			return page;
	}

	rb_link_node(node, parent, p);

	return NULL;
}

static inline struct page * rb_insert_page_cache(struct inode * inode,
						 unsigned long offset,
						 struct rb_node * node)
{
	struct page * ret;
	if ((ret = __rb_insert_page_cache(inode, offset, node)))
		goto out;
	rb_insert_color(node, &inode->i_rb_page_cache);
 out:
	return ret;
}
-----------------------------------------------------------------------
*/

#ifndef	_LINUX_RBTREE_H
#define	_LINUX_RBTREE_H

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/bug.h>
#include <linux/kconfig.h>

struct rb_node
{
	unsigned long  rb_parent_color;
#define	RB_RED		0
#define	RB_BLACK	1
	struct rb_node *rb_right;
	struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rb_root
{
	struct rb_node *rb_node;
};


#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)   ((r)->rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}
static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_ROOT	(struct rb_root) { NULL, }
#define	rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)	((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))

static inline void rb_init_node(struct rb_node *rb)
{
	rb->rb_parent_color = 0;
	rb->rb_right = NULL;
	rb->rb_left = NULL;
	RB_CLEAR_NODE(rb);
}

extern void rb_insert_color(struct rb_node *, struct rb_root *);
extern void rb_erase(struct rb_node *, struct rb_root *);

typedef void (*rb_augment_f)(struct rb_node *node, void *data);
typedef long (*rb_compare_f)(const void *a, const void *b);

extern void rb_augment_insert(struct rb_node *node,
			      rb_augment_f func, void *data);
extern struct rb_node *rb_augment_erase_begin(struct rb_node *node);
extern void rb_augment_erase_end(struct rb_node *node,
				 rb_augment_f func, void *data);

/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(const struct rb_node *);
extern struct rb_node *rb_prev(const struct rb_node *);
extern struct rb_node *rb_first(const struct rb_root *);
extern struct rb_node *rb_last(const struct rb_root *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *new,
			    struct rb_root *root);

static inline void rb_link_node(struct rb_node * node, struct rb_node * parent,
				struct rb_node ** rb_link)
{
	node->rb_parent_color = (unsigned long )parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}


#define __JUNK junk,
#define _iff_empty(test_or_junk, t, f) __iff_empty(test_or_junk, t, f)
#define __iff_empty(__ignored1, __ignored2, result, ...) result

/**
 * IFF_EMPTY() - Expands to the second argument when the first is empty, the
 *               third if non-empty.
 * @test:        An argument to test for emptiness.
 * @t:           A value to expand to if test is empty.
 * @f:           A value to expand to if test is non-empty.
 *
 * Caveats:
 * IFF_EMPTY isn't perfect.  The test parameter must either be empty or a valid
 * pre-processor token as well as result in a valid token when pasted to the
 * end of a word.
 *
 * Valid Examples:
 * IFF_EMPTY(a, b, c) = c
 * IFF_EMPTY( , b, c) = b
 * IFF_EMPTY( ,  , c) = (nothing)
 *
 * Invalid Examples:
 * IFF_EMPTY(.,  b, c)
 * IFF_EMPTY(+,  b, c)
 */
#define IFF_EMPTY(test, t, f) _iff_empty(__JUNK##test, t, f)

/**
 * IS_EMPTY() - test if a pre-processor argument is empty.
 * @arg:        An argument (empty or non-empty)
 *
 * If empty, expands to 1, 0 otherwise.  See IFF_EMPTY() for caveats &
 * limitations.
 */
#define IS_EMPTY(arg)	IFF_EMPTY(arg, 1, 0)

/**
 * OPT_OFFSETOF() - return the offsetof for the supplied expression, or zero
 *                  if m is an empty argument.
 * @type:           struct/union type
 * @member:         (optional) struct member name
 *
 * Since any offsetof can return zero if the specified member is the first in
 * the struct/union, you should also check if the argument is empty separately
 * with IS_EMPTY(m).
 */
#define OPT_OFFSETOF(type, member) IFF_EMPTY(member, 0, offsetof(type, member))

/**
 * enum rb_flags - values for strct rb_relationship's flags
 * @RB_HAS_LEFTMOST:	The container has a struct rb_node *leftmost member
 * 			that will receive a pointer to the leftmost (smallest)
 * 			object in the tree that is updated during inserts &
 * 			deletions.
 * @RB_HAS_RIGHTMOST:	Same as above (for right side of tree).
 * @RB_HAS_COUNT:	The container has an unsigned long field that will
 * 			receive updates of the object count in the tree.
 * @RB_UNIQUE_KEYS:	The tree contains only unique values.
 * @RB_INSERT_REPLACES:	When set, the rb_insert() will replace a value if it
 * 			matches the supplied one (valid only when
 * 			RB_UNIQUE_KEYS is set).
 * @RB_IS_AUGMENTED:	is an augmented tree
 * @RB_ALL_FLAGS:	(internal use)
 */

enum rb_flags {
	RB_HAS_LEFTMOST		= 0x00000001,
	RB_HAS_RIGHTMOST	= 0x00000002,
	RB_HAS_COUNT		= 0x00000004,
	RB_UNIQUE_KEYS		= 0x00000008,
	RB_INSERT_REPLACES	= 0x00000010,
	RB_IS_AUGMENTED		= 0x00000040,
	RB_ALL_FLAGS		= RB_HAS_LEFTMOST | RB_HAS_RIGHTMOST
			| RB_HAS_COUNT | RB_UNIQUE_KEYS
			| RB_INSERT_REPLACES | RB_IS_AUGMENTED,
};

/**
 * struct rb_relationship - Defines relationship between a container and the
 *			    objects it contains.
 * @root_offset:  Offset of container's struct rb_root member.
 * @left_offset:  (Used only if RB_HAS_LEFTMOST is set) Offset of the
 *		  container's struct rb_node *leftmost member for storing a
 *		  pointer to the leftmost node in the tree, which is kept
 *		  updated as inserts and deletions are made.
 * @right_offset: Same as left_offset, except for right side of tree.
 * @count_offset: Offset of container's unsigned long count member.
 * @node_offset:  Offset of object's struct rb_node member.
 * @key_offset:   Offset of object's key member.
 * @flags:        See enum rb_flags.
 * @compare:      Pointer to key rb_compare_f function to compare keys.
 *		  Although it will be cast to and called as type long (*)(const
 *		  void *a, const void *b), you should declare it as accepting
 *		  pointers to your key members, or sanity checks will fail.
 *		  Further, it is optimal if the function is declared inline.
 * @augment:      Pointer to the rb_augment_f or zero if tree is not augmented.
 *
 * Instances of struct rb_relationship should be compile-time constants (or
 * rather, the value of its members).
 */
struct rb_relationship {
	ssize_t root_offset;
	ssize_t left_offset;
	ssize_t right_offset;
	ssize_t count_offset;
	ssize_t node_offset;
	ssize_t key_offset;
	int flags;
	const rb_compare_f compare;
	const rb_compare_f ins_compare;
	const rb_augment_f augment;
	unsigned key_size;
};

#define __RB_PTR(type, ptr, offset) ((type *)((char *)(ptr) + (offset)))

/* public conversion functions for use with run-time values */
static __always_inline
struct rb_root *rb_to_root(const void *ptr,
			   const struct rb_relationship *rel)
{
	return __RB_PTR(struct rb_root, ptr, rel->root_offset);
}

static __always_inline
struct rb_node **rb_root_to_left(struct rb_root *root,
				 const struct rb_relationship *rel)
{
	return __RB_PTR(struct rb_node *, root,
			rel->left_offset - rel->root_offset);
}

static __always_inline
struct rb_node **rb_root_to_right(struct rb_root *root,
				  const struct rb_relationship *rel)
{
	return __RB_PTR(struct rb_node *, root,
			rel->right_offset - rel->root_offset);
}

static __always_inline
unsigned long *rb_root_to_count(struct rb_root *root,
			       const struct rb_relationship *rel)
{
	return __RB_PTR(unsigned long, root,
			rel->count_offset - rel->root_offset);
}

static __always_inline
const void *rb_node_to_key(const struct rb_node *node,
			   const struct rb_relationship *rel)
{
	return __RB_PTR(const void, node,
			rel->key_offset - rel->node_offset);
}

static __always_inline
void *rb_node_to_obj(const struct rb_node *node,
		     const struct rb_relationship *rel)
{
	return __RB_PTR(void, node, -rel->node_offset);
}

static __always_inline
struct rb_node *rb_to_node(const void *ptr, const struct rb_relationship *rel)
{
	return __RB_PTR(struct rb_node, ptr, rel->node_offset);
}

static __always_inline
const void *rb_to_key(const void *ptr, const struct rb_relationship *rel)
{
	return __RB_PTR(const void, ptr, rel->key_offset);
}


/* checked conversion functions that will error on run-time values */
static __always_inline
struct rb_root *__rb_to_root(const void *ptr,
			     const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->root_offset);
	return rb_to_root(ptr, rel);
}

static __always_inline
struct rb_node **__rb_root_to_left(struct rb_root *root,
				   const struct rb_relationship *rel)
{
	BUILD_BUG_ON42(!(rel->flags & RB_HAS_LEFTMOST));
	BUILD_BUG_ON_NON_CONST42(rel->root_offset);
	BUILD_BUG_ON_NON_CONST42(rel->left_offset);
	return rb_root_to_left(root, rel);
}

static __always_inline
struct rb_node **__rb_root_to_right(struct rb_root *root,
				    const struct rb_relationship *rel)
{
	BUILD_BUG_ON42(!(rel->flags & RB_HAS_RIGHTMOST));
	BUILD_BUG_ON_NON_CONST42(rel->root_offset);
	BUILD_BUG_ON_NON_CONST42(rel->right_offset);
	return rb_root_to_right(root, rel);
}

static __always_inline
unsigned long *__rb_root_to_count(struct rb_root *root,
				  const struct rb_relationship *rel)
{
	BUILD_BUG_ON42(!(rel->flags & RB_HAS_COUNT));
	BUILD_BUG_ON_NON_CONST42(rel->root_offset);
	BUILD_BUG_ON_NON_CONST42(rel->count_offset);
	return rb_root_to_count(root, rel);
}

static __always_inline
const void *__rb_node_to_key(const struct rb_node *node,
			     const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->node_offset);
	BUILD_BUG_ON_NON_CONST42(rel->key_offset);
	return rb_node_to_key(node, rel);
}

static __always_inline
void *__rb_node_to_obj(const struct rb_node *node,
		       const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->node_offset);
	return rb_node_to_obj(node, rel);
}

static __always_inline
struct rb_node *__rb_to_node(const void *ptr, const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->node_offset);
	return rb_to_node(ptr, rel);
}

static __always_inline
const void *__rb_to_key(const void *ptr, const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->key_offset);
	return rb_to_key(ptr, rel);
}

/**
 * __rb_assert_good_rel() - Perform compile-time sanity checks on a struct
 *			    rb_relationship.
 * @rel:	Pointer to a const struct rb_relationship to check.
 */
static __always_inline
void __rb_assert_good_rel(const struct rb_relationship *rel)
{
	BUILD_BUG_ON_NON_CONST42(rel->flags);
	BUILD_BUG_ON42(rel->flags & ~RB_ALL_FLAGS);

	BUILD_BUG_ON_NON_CONST42(rel->root_offset);
	BUILD_BUG_ON_NON_CONST42(rel->node_offset);
	BUILD_BUG_ON_NON_CONST42(rel->key_offset);

	if (rel->flags & RB_HAS_LEFTMOST)
		BUILD_BUG_ON_NON_CONST42(rel->count_offset);

	if (rel->flags & RB_HAS_RIGHTMOST)
		BUILD_BUG_ON_NON_CONST42(rel->right_offset);

	if (rel->flags & RB_HAS_COUNT)
		BUILD_BUG_ON_NON_CONST42(rel->left_offset);

	/* Due to a bug in versions of gcc prior to 4.6, the following
	 * expressions are always evalulated at run-time:
	 *
	 * (!(rel->flags & RB_UNIQUE_KEYS) && (rel->flags & RB_INSERT_REPLACES))
	 *
	 * The work-around for this bug is separate each bitwise AND test using
	 * an if/else construct and evaluate only the last test with the
	 * BUILD_BUG_ON macro.
	 */

	if (rel->flags & RB_INSERT_REPLACES)
		BUILD_BUG_ON42(!(rel->flags & RB_UNIQUE_KEYS));
}


/**
 * __rb_find() - Perform a (normal) search on a Red-Black Tree, starting at the
 *		 specified node, traversing downward.
 * @node:	Node (subtree) to start the search from.
 * @key:	Pointer to a key to search for.
 * @rel:	Pointer to the relationship definition constant.
 */
static __always_inline __flatten
struct rb_node *__rb_find(
		struct rb_node *node,
		const void *key,
		const struct rb_relationship *rel)
{
	__rb_assert_good_rel(rel);
	while (node) {
		long diff = rel->compare(key, __rb_node_to_key(node, rel));

		if (diff > 0)
			node = node->rb_right;
		else if (diff < 0)
			node = node->rb_left;
		else
			return node;
	}

	return 0;
}

/**
 * rb_find() - Perform a (normal) search on a Red-Black Tree.
 * @root:	Root of the tree.
 * @key:	Pointer to a key to search for.
 * @rel:	Pointer to the relationship definition constant.
 */
static __always_inline __flatten
struct rb_node *rb_find(
		struct rb_root *root,
		const void *key,
		const struct rb_relationship *rel)
{
	return __rb_find(root->rb_node, key, rel);
}

static __always_inline __flatten
struct rb_node *__rb_find_first_last(
		struct rb_node *node,
		const void *key,
		const struct rb_relationship *rel,
		const int find_first)
{
	__rb_assert_good_rel(rel);

	/* don't use this function on a tree with unique keys */
	BUILD_BUG_ON42(rel->flags & RB_UNIQUE_KEYS);
	BUILD_BUG_ON_NON_CONST(find_first);

	while (node) {
		long diff = rel->compare(key, __rb_node_to_key(node, rel));

		if (diff > 0)
			node = node->rb_right;
		else if (diff < 0)
			node = node->rb_left;
		else {
			if (find_first && node->rb_left)
				node = node->rb_left;
			else if (!find_first && node->rb_right)
				node = node->rb_right;
			else
				return node;
		}
	}

	return 0;
}

/**
 * rb_find_first() - Search for first occurrence of key in a tree containing
 * 		     non-unique keys.
 * @root:	Root of the tree.
 * @key:	Pointer to a key.
 * @rel:	Pointer to the relationship definition constant.
 *
 * This function is intended for use with trees containing non-unique keys.
 * When called for trees with unique keys, it maps to __rb_find (a normal
 * search).
 */
static __always_inline __flatten
struct rb_node *rb_find_first(
		struct rb_root *root,
		const void *key,
		const struct rb_relationship *rel)
{
	return __rb_find_first_last(root->rb_node, key, rel, 1);
}

/**
 * rb_find_last() - Search for last occurrence of key in a tree containing
 * 		    non-unique keys.
 * @root:	Root of the tree.
 * @key:	Pointer to a key.
 * @rel:	Pointer to the relationship definition constant.
 *
 * This function is intended for use with trees containing non-unique keys.
 * When called for trees with unique keys, it maps to __rb_find (a normal
 * search).
 */
static __always_inline __flatten
struct rb_node *rb_find_last(
		struct rb_root *root,
		const void *key,
		const struct rb_relationship *rel)
{
	return __rb_find_first_last(root->rb_node, key, rel, 0);
}

/**
 * rb_find_next() - Locate the next node in a tree containing non-unique keys,
 *		    whos key matches the supplied node.
 * @node:	Node of the current object in the tree.
 * @rel:	Pointer to the relationship definition constant.
 *
 * Generally for use after calling rb_find_first().  Only valid for use with a
 * tree with non-unique keys.
 */
static __always_inline __flatten
struct rb_node *rb_find_next(
		const struct rb_node *node,
		const struct rb_relationship *rel)
{
	const void *key      = __rb_node_to_key(node, rel);
	struct rb_node *next = rb_next(node);

	/* don't use this function on a tree with unique keys */
	BUILD_BUG_ON42(rel->flags & RB_UNIQUE_KEYS);
	return (next && !rel->compare(key, __rb_node_to_key(next, rel)))
	       ? next : NULL;
}

/**
 * rb_find_prev() - Locate the previous node in a tree containing non-unique
 *		    keys, whos key matches the supplied node.
 * @node:	Node of the current object in the tree.
 * @rel:	Pointer to the relationship definition constant.
 *
 * Generally for use after calling rb_find_last().  Only valid for use with a
 * tree with non-unique keys.
 */
static __always_inline __flatten
struct rb_node *rb_find_prev(
		const struct rb_node *node,
		const struct rb_relationship *rel)
{
	const void *key      = __rb_node_to_key(node, rel);
	struct rb_node *prev = rb_prev(node);

	/* don't use this function on a tree with unique keys */
	BUILD_BUG_ON42(rel->flags & RB_UNIQUE_KEYS);
	return (prev && !rel->compare(key, __rb_node_to_key(prev, rel)))
	       ? prev : NULL;
}


enum rb_find_subtree_match {
	RB_MATCH_NONE		= 0,
	RB_MATCH_IMMEDIATE	= 2,
	RB_MATCH_LEFT		= -1,
	RB_MATCH_RIGHT		= 1,
};

/**
 * __rb_find_subtree() - Locate the subtree that contains the specified key (if
 *			 it exists) traversing upwards.
 * @root:	Root of the tree
 * @start:	Node to start from
 * @key:	Key to search for
 * @matched:	Pointer for a result value to be returned to (see enum
 * 		rb_find_subtree_match)
 * @ret_link:	Pointer for a link pointer to be returned
 * @ret_parent:	Pointer for a parent pointer to be returned
 * @rel:	A constant relationship definition
 * @doing_insert: Rather or not we're doing an insert.
 *
 * Travels up a tree, starting from the specified node, until it locates the
 * node (representing the subtree) under which the object (specified by key)
 * can be located, or until that object its self is located.
 *
 * This function is used by find_near and insert_near, but behaves differently
 * for each case (and perhaps could have been implemented as two separate
 * functions). Specifically, when doing_insert is non-zero, it will set values
 * in the location provided by populate ret_link & ret_parent.  Unused
 * functionality when doing_insert is zero should be "compiled-out" in an
 * optimized build.
 */
static __always_inline __flatten
struct rb_node *__rb_find_subtree(
		struct rb_root *root,
		struct rb_node *start,
		const void *key,
		int *matched,
		struct rb_node ***ret_link, /* wow, triple indirection.
					       Am I smart or just nuts? */
		struct rb_node **ret_parent,
		const struct rb_relationship *rel,
		const int doing_insert)
{
	struct rb_node *prev = start;
	struct rb_node *node = rb_parent(start);
	long diff;

	__rb_assert_good_rel(rel);
	BUILD_BUG_ON_NON_CONST(doing_insert);
	BUG_ON(doing_insert && (!root || !ret_link || !ret_parent));

	/* already at top of tree, so return start value */
	if (!node) {
		*matched = RB_MATCH_NONE;
		if (doing_insert) {
			*ret_link = &root->rb_node;
			*ret_parent = **ret_link;
		}
		return start;
	}

	/* The first compare is just to figure out which direction up the tree
	 * we're traveling.  When compare returns a value with a different
	 * sign, we'll have found our subtree, or an exact match if zero.
	 */
	diff = rel->compare(key, __rb_node_to_key(node, rel));

	if (diff) {
		int go_left = diff < 0;
		while (1) {
			prev = node;
			node = rb_parent(prev);
			if (!node)
				/* Reached top of tree.  In this case. rather
				 * than having the top down search start from
				 * the root, we'll start on the prev sibling
				 * since we've already tested the root node, we
				 * know that we don't need to go back the way
				 * we came.
				 */
				break;

			diff = rel->compare(key, __rb_node_to_key(node, rel));
			if (go_left ? diff > 0 : diff < 0)
				/* found the diverging node, so the child on
				 * the opposite side (of prev) is the subtree
				 * that will contain the key
				 */
				break;
			else if (!diff) {
				/* exact match */
				*matched = go_left
					 ? RB_MATCH_LEFT
					 : RB_MATCH_RIGHT;

				goto find_parent_link;
			}
		}

		*matched = RB_MATCH_NONE;
		if (doing_insert) {
			*ret_parent = prev;
			*ret_link = go_left ? &prev->rb_left : &prev->rb_right;
			return **ret_link;
		} else {
			return go_left ? prev->rb_left : prev->rb_right;
		}
	}

	/* start node's parent was an exact match */
	*matched = RB_MATCH_IMMEDIATE;

find_parent_link:
	if (doing_insert) {
		struct rb_node *parent = rb_parent(node);

		if (!parent) {
			*ret_link = &root->rb_node;
			*ret_parent = **ret_link;
		} else if (parent->rb_left == node) {
			*ret_link = &parent->rb_left;
			*ret_parent = parent;
		} else if (parent->rb_right == node) {
			*ret_link = &parent->rb_left;
			*ret_parent = parent;
		} else {
			BUG();
		}
	}

	return node;
}

/**
 * rb_find_near() - Perform a search starting at the specified node instead of
 *		    the top of the tree.
 * @from:	Node to start search from
 * @key:	Key to search for
 * @rel:	Pointer to the relationship definition constant.
 *
 * Travels up the tree starting from the specified node and then back down
 * again, searching for the object specified by key.  This function is larger
 * than a normal search, but can yield better performance if the target object
 * is near the supplied node. Performance is roughly O(log2(distance / 2) * 2
 * + 1).
 */
static __always_inline __flatten
struct rb_node *rb_find_near(
		struct rb_node *from,
		const void *key,
		const struct rb_relationship *rel)
{
	int matched;
	struct rb_node *subtree;

	subtree = __rb_find_subtree(NULL, from, key, &matched, NULL, NULL,
				    rel, 0);

	if (matched)
		return subtree;

	return __rb_find(subtree, key, rel);
}

/* common insert epilogue used by rb_insert() and rb_insert_near() */
static __always_inline __flatten
struct rb_node *__rb_insert_epilogue(
		struct rb_root *root,
		struct rb_node *parent,
		struct rb_node *node,
		struct rb_node *found,
		struct rb_node **rb_link,
		const struct rb_relationship *rel)
{
	if ((rel->flags & RB_UNIQUE_KEYS) && found) {
		if (rel->flags & RB_INSERT_REPLACES) {
			/* if we're replacing the entry, we don't increment
			 * count, but we do still need to do augment
			 */
			rb_replace_node(found, node, root);
			goto do_augment;
		} else {
			/* otherwise, we don't do either */
			goto done;
		}
	} else {
		rb_link_node(node, parent, rb_link);
		rb_insert_color(node, root);
	}

	if ((rel->flags & RB_HAS_COUNT))
		++*__rb_root_to_count(root, rel);

do_augment:
	if (rel->augment)
		rb_augment_insert(node, rel->augment, NULL);

done:
	return found;
}


/**
 * rb_insert() - Insert a node into a tree.
 * @root:	Pointer to struct rb_root.
 * @node:	Pointer to the node of the new object to insert.
 * @rel:	Pointer to the relationship definition constant.
 *
 * If an object with the same key already exists and RB_INSERT_REPLACES is set
 * then it is replaced with new object node; if RB_INSERT_REPLACES is not set,
 * then no change is made. In either case, a pointer to the existing object
 * node is returned.
 *
 * If no object with the same key exists, then the new object node is inserted
 * and NULL is returned.
 */
static __always_inline __flatten
struct rb_node *rb_insert(
		struct rb_root *root,
		struct rb_node *node,
		const struct rb_relationship *rel)
{
	struct rb_node **p     = &root->rb_node;
	struct rb_node *parent = NULL;
	const void * const key = __rb_node_to_key(node, rel);
	int leftmost           = 1;
	int rightmost          = 1;

	/* optimization/hack good on gcc 4.6.0+, when -findirect-inline is able
	 * to inline the compare function.  This manages to force gcc to put
	 * the value of the key in a register, instead of retrieving it prior
	 * to each compare.  The necessity of this hasn't been tested beyond
	 * gcc 4.7.1.
	 */
#if GCC_VERSION >= 40600
	u16 __maybe_unused key16;
	u32 __maybe_unused key32;
	u64 __maybe_unused key64;

	if (rel->key_size == 2)
		key16 = *(u16*)key;
	else if (rel->key_size == 4)
		key32 = *(u32*)key;
	else if (rel->key_size == 8)
		key64 = *(u64*)key;
#endif

	__rb_assert_good_rel(rel);


	while (*p) {
		long diff;
		const void *cur_key = __rb_node_to_key(*p, rel);

#if GCC_VERSION >= 40600
		if (rel->key_size == 2)
			diff = rel->ins_compare(&key16, cur_key);
		else if (rel->key_size == 4)
			diff = rel->ins_compare(&key32, cur_key);
		else if (rel->key_size == 8)
			diff = rel->ins_compare(&key64, cur_key);
		else
#endif
			/* On gcc 4.5.x & prior, or for other key sizes, we
			 * pass key ptr as a const void*, which tends to
			 * optimize more poorly
			 */
			diff = rel->ins_compare(key, cur_key);

		parent = *p;

		if (diff > 0) {
			p = &(*p)->rb_right;
			if (rel->flags & RB_HAS_LEFTMOST)
				leftmost = 0;
		} else if (!(rel->flags & RB_UNIQUE_KEYS) || diff < 0) {
			p = &(*p)->rb_left;
			if (rel->flags & RB_HAS_RIGHTMOST)
				rightmost = 0;
		} else
			break;
	}

	if ((rel->flags & RB_HAS_LEFTMOST) && leftmost) {
		struct rb_node **left = __rb_root_to_left(root, rel);

		if (!(rel->flags & RB_INSERT_REPLACES) || !(*p) || *left == *p)
			*left = node;
	}
	if ((rel->flags & RB_HAS_RIGHTMOST) && rightmost) {
		struct rb_node **right = __rb_root_to_right(root, rel);

		if (!(rel->flags & RB_INSERT_REPLACES) || !(*p) || *right == *p)
			*right = node;
	}

	return __rb_insert_epilogue(root, parent, node, *p, p, rel);
}

/**
 * rb_insert_near() - Perform an insert, but use the supplied start node to
 *		      find the location for the new node.
 * @root:	Pointer to struct rb_root.
 * @start:	Node to start search for insert location from.
 * @node:	Pointer to the node of the new object to insert.
 * @rel:	Pointer to the relationship definition constant.
 *
 * This function is larger than rb_insert, but can yield better performance
 * when the position where the new node is being is close to start. Performance
 * is roughly O(log2(distance / 2) * 2 + 1).
 */
static __always_inline __flatten
struct rb_node *rb_insert_near(
		struct rb_root *root,
		struct rb_node *start,
		struct rb_node *node,
		const struct rb_relationship *rel)
{
	const void *key = __rb_node_to_key(node, rel);
	struct rb_node **p;
	struct rb_node *parent;
	struct rb_node *ret;
	int matched;
	long diff;

	BUILD_BUG_ON_NON_CONST42(rel->flags);

	ret = __rb_find_subtree(root, start, key, &matched, &p, &parent, rel,
				1);

	if (!matched) {
		while (*p) {
			diff   = rel->compare(__rb_node_to_key(*p, rel), key);
			parent = *p;

			if (diff > 0)
				p = &(*p)->rb_right;
			else if (!(rel->flags & RB_UNIQUE_KEYS) || diff < 0)
				p = &(*p)->rb_left;
			else
				break;
		}
		ret = *p;
	}

	/* the longer way to see if we're left- or right-most (since we aren't
	 * starting from the top, we can't use the mechanism rb_insert()
	 * does.)
	 */
	if (rel->flags & RB_HAS_LEFTMOST) {
		struct rb_node **left = __rb_root_to_left(root, rel);
		if (!*left || *left == ret ||
				(*left == parent && &parent->rb_left == p))
			*left = node;
	}

	if (rel->flags & RB_HAS_RIGHTMOST) {
		struct rb_node **right = __rb_root_to_right(root, rel);
		if (!*right || *right == ret ||
				(*right == parent && &parent->rb_right == p))
			*right = node;
	}

	return __rb_insert_epilogue(root, parent, node, ret, p, rel);
}

/**
 * rb_remove() - Remove a node from an rbtree.
 * @root:	Pointer to struct rb_root.
 * @node:	Pointer to the node of the object to be removed.
 * @rel:	Pointer to the relationship definition constant.
 */
static __always_inline __flatten
void rb_remove(
	struct rb_root *root,
	struct rb_node *node,
	const struct rb_relationship *rel)
{
	struct rb_node *uninitialized_var(deepest);

	BUILD_BUG_ON_NON_CONST42(rel->flags);

	if (rel->augment)
		deepest = rb_augment_erase_begin(node);

	if (rel->flags & RB_HAS_LEFTMOST) {
		struct rb_node **left = __rb_root_to_left(root, rel);

		if (*left == node)
			*left = rb_next(node);
	}

	if (rel->flags & RB_HAS_RIGHTMOST) {
		struct rb_node **right = __rb_root_to_right(root, rel);

		if (*right == node)
			*right = rb_prev(node);
	}

	rb_erase(node, root);

	if ((rel->flags & RB_HAS_COUNT))
		--*__rb_root_to_count(root, rel);

	if (rel->augment)
		rb_augment_erase_end(deepest, rel->augment, NULL);
}


/**
 * RB_RELATIONSHIP - Define the relationship between a container with a struct
 *		    rb_root member, and the objects it contains.
 * @cont_type: container type
 * @root:      Container's struct rb_root member name
 * @left:      (Optional) If the container needs a pointer to the tree's
 *             leftmost (smallest) object, then specify the container's struct
 *             rb_node *leftmost member.  Otherwise, leave this parameter
 *             empty.
 * @right:     (Optional) Same as left, but for the rightmost (largest)
 * @count:     (Optional) Name of container's unsigned long member that will be
 *             updated with the number of objects in the tree.  Note that if
 *             you add or remove objects from the tree without using the
 *             generic functions, you must update this value yourself.
 * @obj_type:  Type of object stored in container
 * @node:      The struct rb_node member of the object
 * @key:       The key member of the object
 * @_flags:    see enum rb_flags.  Note: you do not have to specify
 *             RB_HAS_LEFTMOST, RB_HAS_RIGHTMOST, RB_HAS_COUNT or
 *             RB_IS_AUGMENTED as these will be added automatically if their
 *             respective field is non-empty.
 * @_compare:  Pointer to key rb_compare_f function to compare keys.
 *             Although it will be cast to and called as type long (*)(const
 *             void *a, const void *b), you should declare it as accepting
 *             pointers to your key members, or sanity checks will fail.
 *             Further, it is optimal if the function is declared inline.
 * @_ins_compare:
 * @_augment:  (optional) pointer to the rb_augment_f or empty if tree is not
 *             augmented.
 *
 * Example:
 * struct my_container {
 *     struct rb_root root;
 *     unsigned int count;
 *     struct rb_node *left;
 * };
 *
 * struct my_object {
 *     struct rb_node node;
 *     int key;
 * };
 *
 * static inline long compare_int(const int *a, const int *b)
 * {
 *     return *a - *b;
 * }
 *
 * static inline long greater_int(const int *a, const int *b)
 * {
 *     return *a > *b;
 * }
 *
 * static const struct rb_relationship my_rel = RB_RELATIONSHIP(
 *     struct my_container, root, left, , count, // no rightmost
 *     struct my_object, node, key,
 *     0, compare_int, greater_int, );           // no augment
 */
#define RB_RELATIONSHIP(						\
		cont_type, root, left, right, count,			\
		obj_type, node, key,					\
		_flags, _compare, _ins_compare, _augment) {		\
	.root_offset	= offsetof(cont_type, root),			\
	.left_offset	= OPT_OFFSETOF(cont_type, left),		\
	.right_offset	= OPT_OFFSETOF(cont_type, right),		\
	.count_offset	= OPT_OFFSETOF(cont_type, count),		\
	.node_offset	= offsetof(obj_type, node),			\
	.key_offset	= offsetof(obj_type, key),			\
	.flags		= (_flags)					\
			| IFF_EMPTY(left ,    0,  RB_HAS_LEFTMOST)	\
			| IFF_EMPTY(right,    0,  RB_HAS_RIGHTMOST)	\
			| IFF_EMPTY(count,    0,  RB_HAS_COUNT)		\
			| IFF_EMPTY(_augment, 0,  RB_IS_AUGMENTED),	\
	.compare	= (const rb_compare_f) (_compare),		\
	.ins_compare	= (const rb_compare_f) (_ins_compare),		\
	.augment	= IFF_EMPTY(_augment, 0, _augment),		\
	.key_size	= sizeof(((obj_type *)0)->key)			\
}

/* compile-time type-validation functions used by __rb_sanity_check_##prefix */
static inline void __rb_verify_root(struct rb_root *root) {}
static inline void __rb_verify_left(struct rb_node * const *left) {}
static inline void __rb_verify_right(struct rb_node * const *right) {}
static inline void __rb_verify_count(const unsigned int *count) {}
static inline void __rb_verify_node(struct rb_node *node) {}
static inline void __rb_verify_compare_fn_ret(long *diff) {}
static inline void __rb_verify_ins_compare_fn_ret(long *diff) {}

/**
 * RB_DEFINE_INTERFACE - Defines a complete interface for a relationship
 *                       between container and object including a struct
 *                       rb_relationship and an interface of type-safe wrapper
 *                       functions.
 * @prefix:    name for the relationship (see explanation below)
 * @cont_type:   see RB_RELATIONSHIP
 * @root:        see RB_RELATIONSHIP
 * @left:        see RB_RELATIONSHIP
 * @right:       see RB_RELATIONSHIP
 * @count:       see RB_RELATIONSHIP
 * @obj_type:    see RB_RELATIONSHIP
 * @node:        see RB_RELATIONSHIP
 * @key:         see RB_RELATIONSHIP
 * @flags:       see RB_RELATIONSHIP
 * @compare:     see RB_RELATIONSHIP
 * @ing_compare: see RB_RELATIONSHIP
 * @augment:     see RB_RELATIONSHIP
 * @insert_mod:      (Optional) Function modifiers for insert function,
 *                   defaults to "static __always_inline" if left empty.
 * @insert_near_mod: (Optional) Same as above, for insert_near.
 * @find_mod:        (Optional) Same as above, for find.
 * @find_near_mod:   (Optional) Same as above, for find_near.
 *
 * This macro can be declared in the global scope of either a source or header
 * file and will generate a static const struct rb_relationship variable named
 * prefix##_rel as well as similarly named (i.e., prefix##_##func_name)
 * type-safe wrapper functions for find, find_near, insert, insert_near and
 * remove.  If these function names are not sufficient, you can use the
 * __RB_DEFINE_INTERFACE macro to specify them explicitly.
 *
 * The compare function will be passed pointers to the key members of two
 * objects.  If your compare function needs access to other members of your
 * struct (e.g., compound keys, etc.) , you can use the rb_entry macro to
 * access other members.  However, if you use this mechanism, your find
 * function must always pass it's key parameter as a pointer to the key member
 * of an object of type obj_type, since the compare function is used for both
 * inserts and lookups (else, you'll be screwed).
 *
 * Example:
 * struct my_container {
 *     struct rb_root root;
 *     unsigned int count;
 *     struct rb_node *left;
 * };
 *
 * struct my_object {
 *     struct rb_node node;
 *     int key;
 * };
 *
 * static inline long compare_int(const int *a, const int *b)
 * {
 *     return (long)*a - (long)*b;
 * }
 *
 * RB_DEFINE_INTERFACE(
 *     my_tree,
 *     struct my_container, root, left, , count, // no rightmost
 *     struct my_object, node, key,
 *     0, compare_int,
 *     ,                     // defaults for find
 *     static __flatten,     // let gcc decide rather or not to inline insert()
 *     ,                     // defaults on find_near
 *     static __flatten noinline) // don't let gcc inline insert_near()
 */
#define RB_DEFINE_INTERFACE(						\
	prefix,								\
	cont_type, root, left, right, count,				\
	obj_type, node, key,						\
	flags, compare, ins_compare, augment,				\
	find_mod, insert_mod, find_near_mod, insert_near_mod)		\
									\
/* Compile-time sanity checks. You need not call this function for	\
 * validation to occur.  We define __rb_sanity_check function first,	\
 * so errors will (hopefully) get caught here and produce a more helpful\
 * error message than a failure in the RB_RELATIONSHIP macro expansion.	\
 */									\
static inline __maybe_unused						\
void __rb_sanity_check_ ## prefix(cont_type *cont, obj_type *obj)	\
{									\
	/* {,ins_}compare functions should take ptr to key member */	\
	typeof((compare)(&obj->key, &obj->key)) _diff =			\
			(compare)(&obj->key, &obj->key);		\
	typeof((ins_compare)(&obj->key, &obj->key)) _ins_diff =		\
			(ins_compare)(&obj->key, &obj->key);		\
	__rb_verify_compare_fn_ret(&_diff);				\
	__rb_verify_ins_compare_fn_ret(&_ins_diff);			\
									\
	/* validate types of container members */			\
	__rb_verify_root(&cont->root);					\
	__rb_verify_left (IFF_EMPTY(left  , 0, &cont->left));		\
	__rb_verify_right(IFF_EMPTY(right , 0, &cont->right));		\
	__rb_verify_count(IFF_EMPTY(count , 0, &cont->count));		\
									\
	/* validate types of object node */				\
	__rb_verify_node(&obj->node);					\
}									\
									\
static const struct rb_relationship prefix ## _rel =			\
RB_RELATIONSHIP(							\
	cont_type, root, left, right, count,				\
	obj_type, node, key,						\
	flags, compare, ins_compare, augment);				\
									\
IFF_EMPTY(find_mod, static __always_inline, find_mod)			\
obj_type *prefix ## _find(cont_type *cont,				\
			  const typeof(((obj_type *)0)->key) *_key)	\
{									\
	struct rb_node *ret = rb_find(					\
			&cont->root, _key, &prefix ## _rel);		\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
IFF_EMPTY(insert_mod, static __always_inline, insert_mod)		\
obj_type *prefix ## _insert(cont_type *cont, obj_type *obj)		\
{									\
	struct rb_node *ret = rb_insert(				\
			&cont->root, &obj->node, &prefix ## _rel);	\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
IFF_EMPTY(find_near_mod, static __always_inline, find_near_mod)		\
obj_type *prefix ## _find_near(obj_type *near,				\
		    const typeof(((obj_type *)0)->key) *_key)		\
{									\
	struct rb_node *ret = rb_find_near(				\
			&near->node, _key, &prefix ## _rel);		\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
IFF_EMPTY(insert_near_mod, static __always_inline, insert_near_mod)	\
obj_type *prefix ## _insert_near(cont_type *cont, obj_type *near,	\
				 obj_type *obj)				\
{									\
	struct rb_node *ret = rb_insert_near(				\
			&cont->root, &near->node, &obj->node,		\
			&prefix ## _rel);				\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline							\
void prefix ## _remove(cont_type *cont, obj_type *obj)			\
{									\
	rb_remove(&cont->root, &obj->node, &prefix ## _rel);		\
}									\
									\
IFF_EMPTY(find_mod, static __always_inline, find_mod) __maybe_unused	\
obj_type *prefix ## _find_first(cont_type *cont,			\
			  const typeof(((obj_type *)0)->key) *_key)	\
{									\
	struct rb_node *ret = rb_find_first(				\
			&cont->root, _key, &prefix ## _rel);		\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
IFF_EMPTY(find_mod, static __always_inline, find_mod) __maybe_unused	\
obj_type *prefix ## _find_last(cont_type *cont,				\
			  const typeof(((obj_type *)0)->key) *_key)	\
{									\
	struct rb_node *ret = rb_find_last(				\
			&cont->root, _key, &prefix ## _rel);		\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline							\
obj_type *prefix ## _find_next(const obj_type *obj)			\
{									\
	struct rb_node *ret = rb_find_next(&obj->node, &prefix ## _rel);\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline							\
obj_type *prefix ## _find_prev(const obj_type *obj)			\
{									\
	struct rb_node *ret = rb_find_prev(&obj->node, &prefix ## _rel);\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline obj_type *prefix ## _next(const obj_type *obj)	\
{									\
	struct rb_node *ret = rb_next(&obj->node);			\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline obj_type *prefix ## _prev(const obj_type *obj)	\
{									\
	struct rb_node *ret = rb_prev(&obj->node);			\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline obj_type *prefix ## _first(cont_type *cont)	\
{									\
	struct rb_node *ret = rb_first(&cont->root);			\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}									\
									\
static __always_inline obj_type *prefix ## _last(cont_type *cont)	\
{									\
	struct rb_node *ret = rb_last(&cont->root);			\
	return ret ? rb_entry(ret, obj_type, node) : 0;			\
}

#endif	/* _LINUX_RBTREE_H */
