/* main.c - userspace generic red-black tree test program.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>

#include "common.h"

long run_test(unsigned int count);

struct timezone tz;
struct object_pools objects;

#define BAIL_ON_ERR(var, expr)			\
do {						\
	var = (expr);				\
	if(IS_ERR_VALUE(var)) return var;	\
} while(0)

int process_args(struct grbtest_config *config, int argc, char *argv[]);
void show_usage(void);
void init_basename(void);
void print_human_summary(const struct grbtest_config *config);
void print_human_result(const struct grbtest_result *result);

const char *argv0  = NULL;

int main(int argc, char *argv[]) {
	long		      ret;
	struct grbtest_config config;
	struct grbtest_result result;
	struct container      cont;
	char		     *argv0_copy = strdup(argv[0]);
	argv0				 = basename(argv0_copy);

	if (process_args(&config, argc, argv)) {
		fprintf(stderr, "\n");
		show_usage();
		free(argv0_copy);
		return -1;
	}

	facilities_init();
	init_container(&cont);
	grbtest_init_results(&config, &result);

	BAIL_ON_ERR(ret, grbtest_init(&config));

	if (config.human_readable)
		print_human_summary(&config);
	else if (config.print_header)
		grbtest_print_result_header(&config);

	grbtest_run_test(&config, &result, &cont);

	if (config.human_readable)
		print_human_result(&result);
	else
		grbtest_print_result_row(&config, &result);

	grbtest_cleanup();
	free(config.delimiter);
	free(config.text_enclosure);
	free(argv0_copy);

	return 0;
}

void print_human_summary(const struct grbtest_config *config)
{
	unsigned long long in_seed = (unsigned long long)config->in_seed;
	unsigned long long seed = (unsigned long long)config->seed;

	print_msg("Build Configuration\n%s\n", grbtest_config());

	print_msg(
		"Execution Parameters\n"
		"test            %u (%s)\n"
		"in_seed         %llu (0x%llx)\n"
		"seed            %llu (0x%llx)\n"
		"key_mask        %u (0x%x)\n"
		"count           %u (0x%x)\n"
		"pool_count      %u\n"
		"reps            %u (0x%x)\n"
		"human_readable  %u\n"
		"delimiter       %s\n"
		"text_enclosure  %s\n"
		"print_header    %u\n"
		"\n",
		config->test, grbtest_type_desc[config->test],
		in_seed, in_seed,
		seed, seed,
		config->key_mask, config->key_mask,
		config->object_count, config->object_count,
		config->pool_count,
		config->reps, config->reps,
		config->human_readable,
		config->delimiter,
		config->text_enclosure,
		config->print_header);

	print_msg("Running test...");
}

void print_human_result(const struct grbtest_result *result)
{
	print_msg("completed!\n\n");
	print_msg(
		"Test Results\n"
		"insertions       %llu\n"
		"insertion_time   %llu\n"
		"evictions        %llu\n"
		"deletions        %llu\n"
		"deletion_time    %llu\n",
		(unsigned long long)result->insertions,
		(unsigned long long)result->insertion_time,
		(unsigned long long)result->evictions,
		(unsigned long long)result->deletions,
		(unsigned long long)result->deletion_time);

}

/**
 * Determine base by prefix and offset to number. Uses standard rules
 * (expressed in regex below):
 * 0[xX][0-9a-fA-F]+ denotes a hexidecimal number
 * 0[0-7]+           denotes an octal number
 * (0|[1-9][0-9]*)   denotes a decimal number
 */
int get_param_base_and_start(const char **str)
{
	assert(*str);

	/* Parse an "0x", "0X" or "0" -prefixed string, but not the string
	 * "0" (which will be treated as base ten). */
	if (**str == '0' && *(*str + 1)) {
		++*str;
		if (**str == 'x' || **str == 'X') {
			++*str;
			return 16;
		} else {
			return 8;
		}
	} else {
		return 10;
	}
}

/**
 * @returns zero on success, non-zero if the string didn't represent a clean
 *          number
 *
 * FIXME: wont be correct for u32 on platforms where sizeof(int) == 2
 */
int get_uint_param(const char *str, unsigned int *dest)
{
	const char *start = str;
	char *endptr;
	int base = get_param_base_and_start(&start);

	*dest = strtoul(start, &endptr, base);
	if (*endptr) {
		fprintf(stderr, "bad number: %s\n", str);
		exit (1);
	}

	return *endptr;
}

/**
 * @returns zero on success, non-zero if the string didn't represent a clean
 *          number
 * FIXME: assumes u64 is unsigned long long
 */
int get_u64_param(const char *str, u64 *dest)
{
	char *endptr;
	int base = get_param_base_and_start(&str);

	*dest = strtoull(str, &endptr, base);
	if (*endptr) {
		fprintf(stderr, "bad number: %s\n", str);
		exit (1);
	}

	return *endptr;
}

void show_usage()
{
	fprintf(stderr,
"Usage: %s [options]\n"
"Options:\n"
"  -h,     --help    Show this help\n"
"  -t=NUM, --test    The test to run\n"
"                    0 Performance: Insert\n"
"                    1 Performance: Insert & Delete\n"
"                    2 Validation\n"
"  -s=NUM, --seed    Seed for random key generation (zero to use current\n"
"                    time)\n"
"  -m=NUM, --keymask Bitmask for keys (key range)\n"
"  -c=NUM, --count   Number of objects to use\n"
"  -r=NUM, --reps    Number of times to repeat test(s)\n"
"  -p=NUM, --pools   Number of pools of objects to use\n"
"  -u,     --human   Output in human-readable form\n"
"  -d=STR, --delim   Use the string STR to delimit fields\n"
"  -H,     --header  Output a row header\n"
"\n"
"Fields:\n"
"  compiler        the compiler used\n"
"  use_generic     value of GRBTEST_BUILD_GENERIC\n"
"  use_leftmost    value of GRBTEST_USE_LEFTMOST\n"
"  use_rightmost   value of GRBTEST_USE_RIGHTMOST\n"
"  use_count       value of GRBTEST_USE_COUNT\n"
"  unique_keys     value of GRBTEST_UNIQUE_KEYS\n"
"  insert_replaces value of GRBTEST_INSERT_REPLACES\n"
"  use_augmented   value of GRBTEST_USE_AUGMENTED\n"
"  debug           if CONFIG_DEBUG_RBTREE is enabled (.config)\n"
"  debug_validate  if CONFIG_DEBUG_RBTREE_VALIDATE is enabled (.config)\n"
"  test            \n"
"  in_seed         input seed\n"
"  seed            result seed (differs if supplied seed is zero)\n"
"  key_mask        \n"
"  object_count    number of objects used for test\n"
"  pool_count      \n"
"  reps            number of times test is repeated\n"
"  node_size       sizeof(struct rb_node)\n"
"  object_size     sizeof(struct object)\n"
"  pool_size       \n"
"  time            time in microseconds\n"
"  insertions      number of insertions\n"
"  deletions       number of deletions\n"
"  evictions       number of evictions (always zero unless both \n"
"                  GRBTEST_UNIQUE_KEYS and GRBTEST_USE_AUGMENTED are\n"
"                  non-zero)\n"
"\n"
"Example:\n"
"%s -s 1 --reps 0x8000 --count 0x800 --keymask 0xfff\n"
"\n",
		argv0, argv0);
}

int process_args(struct grbtest_config *config, int argc, char *argv[])
{
	int c;

	memset(config, 0, sizeof(*config));
	config->test		= 0;
	config->in_seed		= 0;
	config->seed		= 0;
	config->key_mask	= 0xff;
	config->object_count	= 0x100;
	config->pool_count	= 1;
	config->reps		= 1;
	config->human_readable	= 0;
	config->delimiter	= strdup(",");
	config->text_enclosure	= strdup("'");
	config->print_header	= 0;

	while (1) {
		static struct option long_options[] = {
			{"help",    no_argument,       0, 'h'},
			{"test",    required_argument, 0, 't'},
			{"seed",    required_argument, 0, 's'},
			{"keymask", required_argument, 0, 'm'},
			{"count",   required_argument, 0, 'c'},
			{"reps",    required_argument, 0, 'r'},
			{"pools",   required_argument, 0, 'p'},
			{"human",   no_argument,       0, 'u'},
			{"delim",   required_argument, 0, 'd'},
			{"quote",   required_argument, 0, 'q'},
			{"header",  no_argument,       0, 'H'},
			{0,         0,                 0, 0}
		};

		/* getopt_long stores the option index here. */
		int option_index = 0;
		c = getopt_long(argc, argv, "ht:s:c:r:p:m:ud:q:H", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
		break;

		switch (c) {
		case 'h': show_usage();			exit(1);
		case 't': get_uint_param(optarg, &config->test);	break;
		case 's': get_u64_param(optarg, &config->in_seed);	break;
		case 'm': get_uint_param(optarg, &config->key_mask);	break;
		case 'c': get_uint_param(optarg, &config->object_count);break;
		case 'r': get_uint_param(optarg, &config->reps);	break;
		case 'p': get_uint_param(optarg, &config->pool_count);	break;
		case 'u': config->human_readable = 1;			break;
		case 'd': free(config->delimiter);
			  config->delimiter = strdup(optarg);		break;
		case 'q': free(config->text_enclosure);
			  config->text_enclosure = strdup(optarg);	break;
		case 'H': config->print_header = 1;			break;
		default : return -1;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
		return -1;
	}

	if (config->test >= GRBTEST_TYPE_COUNT) {
		fprintf(stderr, "Invalid test specified.\n");
		return -1;
	}

	return 0;
}



#if 0
/****************************************************************************
 * rbtree & container-related functions
 */

static __always_inline long compare_long(const long *a, const long *b) {
	return *a - *b;
}

static __always_inline long compare_u32(const u32 *a, const u32 *b) {
	return (long)*a - (long)*b;
}

static inline void init_container(struct container *cunt) {
	cunt->tree = RB_ROOT;
	cunt->count = 0;
}

/****************************************************************************
 * rb_relationship definition
 */
RB_DEFINE_INTERFACE(
	mytree,
	struct container, tree, leftmost, rightmost, count,
	struct object, node, id,
	RB_UNIQUE_KEYS | RB_INSERT_REPLACES, compare_u32, ,
	,
	static __flatten noinline,
	static __flatten,
	static __flatten __maybe_unused);

void dumpNode(struct rb_node *node) {
	fprintf(stderr, "rb_parent_color = 0x%lu (parent = 0x%p, color = %d)\n",
		node->rb_parent_color, rb_parent(node), (int)rb_color(node));
	fprintf(stderr, "rb_right        = %p\n", node->rb_right);
	fprintf(stderr, "rb_left         = %p\n", node->rb_left);
}

void printObj(struct object *obj) {

}

struct obj_display {
	struct object *o;
	char text[64];
};

void populateDisp(struct rb_node *node, struct obj_display *disp_arr, unsigned *index, unsigned target_depth) {
	struct rb_node *left = NULL;
	struct rb_node *right = NULL;

	if (target_depth == 0) {
		struct obj_display *disp = &disp_arr[*index];

		if (node) {
			struct object *obj = rb_entry(node, struct object, node);
			disp->o = obj;
			snprintf(disp->text, sizeof(disp->text),
				"%p=%02d", obj, obj->id);
		} else {
			disp->o = 0;
			snprintf(disp->text, sizeof(disp->text),
				"empty      ");
		}
		++(*index);
	}

	if (target_depth > 0) {
		if (node) {
			left = node->rb_left;
			right = node->rb_right;
		}
		populateDisp(left, disp_arr, index, target_depth - 1);
		populateDisp(right, disp_arr, index, target_depth - 1);
	}
}

void dumpTree(struct rb_root *root, size_t obj_count) {
	int max_depth = 5;
	int max_nodes = 1 << max_depth;
	unsigned index = 0u;
	int i;
	size_t item_text_size;
	struct obj_display *disp = (struct obj_display *)malloc(max_nodes * sizeof(struct obj_display));

	for (i = 0; i < max_depth; ++i) {
		populateDisp(root->rb_node, disp, &index, i);
	}
	item_text_size = strlen(disp[0].text);

	for (i = 0; i < max_nodes; ++i) {
		if (i == 1 || i == 3 || i == 7 || i == 15 || i == 31) {
			int bity;
			fprintf(stderr, "\n");
			for (bity = i + 1; !(bity & 32); bity <<= 1) {
				const char spaces[257] = {[0 ... 255] = ' ', [256] = 0};
				fprintf(stderr, "%s", &spaces[256 - item_text_size]);
			}

		} else {
			fprintf(stderr, "  ");
		}
		fprintf(stderr, "%s", disp[i].text);
	}
	fprintf(stderr, "\n");

}

__attribute__((flatten))
long run_test(unsigned int count) {
	size_t buf_size = sizeof(struct object) * count;
	struct container cont;
	struct object *obj_pools[2];
	struct object **tree_contents;
	size_t pool_size;
	int pool_in_use;
	long i, j, k;
	struct object *found;
	struct rb_node *node;
	long long start, end;

	long long now = getCurTicks();

	srand((now & 0xffffffff) ^ (now >> 32));

	if (count < 1 || count > 0x1000000) { /* 16.8 million should be a reasonable limit */
		return -1;
	}

	fprintf(stderr, "allocating two pools of %u objects each\n", count);

	cont.tree      = RB_ROOT;
	cont.count     = 0;
	cont.leftmost  = 0;
	cont.rightmost = 0;
	pool_in_use      = 0;
	obj_pools[0]     = (struct object *)malloc(buf_size);
	obj_pools[1]     = (struct object *)malloc(buf_size);

	fprintf(stderr, "initializing objects\n");

	/* init a psudo-random using a real-random seed */
//	get_random_bytes(&seed, sizeof(seed));
//	prandom32_seed(&grbtest.rand, seed);

	for (i = 0; i < 2; ++i) {
		struct object *p = obj_pools[i];
		struct object *end = &p[count];

		for (; p != end; ++p) {
			p->id = rand() & 0xfffff;
			rb_init_node(&p->node);
		}
	}
	pool_size = count;

	for (j = 0; j < 2; ++j) {
		struct object *pool = obj_pools[j];
		start = getCurTicks();

		for (i = count; i; ) {
			struct object *new = &pool[--i];

			mytree_insert(&cont, new);
		}
		pool_in_use ^= 1;
		end = getCurTicks();
		fprintf(stderr, "Inserted %u objects in %llu\n", count, end - start);
	}

	fprintf(stderr, "walking tree now...\n");
	tree_contents = (struct object **)malloc(sizeof(void*) * cont.count);
	start = getCurTicks();
	for (i = 0, node = cont.leftmost; node; node = rb_next(node), ++i) {
		tree_contents[i] = rb_entry(node, struct object, node);
	}
	end = getCurTicks();
	fprintf(stderr, "Finished walking tree of %u in %llu\n", cont.count, end - start);

	fprintf(stderr, "root = %p, count = %u\n", cont.tree.rb_node, cont.count);
	//dumpNode(cont.tree.rb_node);

	//dumpTree(&cont.tree, 16);
	start = getCurTicks();
#define NEAR_RANGE 8
#if 0
	for (i = 0; i < cont.count; ++i) {
		for (j = 0; j < cont.count; ++j) {
			found = mytree_find_near(tree_contents[i], &tree_contents[j]->id);
			if (found != tree_contents[j]) {
				fprintf(stderr, "find_near found %p near %p (expected %p)\n",
					found, tree_contents[i], tree_contents[j]);
				found = mytree_find_near(tree_contents[i], &tree_contents[j]->id);
			}
		}
	}
#else
for (k = 0; k < 8; ++k) {
	for (i = 0; i < cont.count; ++i) {
		int max = i + NEAR_RANGE;
		if (max > cont.count)
			max = cont.count;
		for (j = i > NEAR_RANGE ? i - NEAR_RANGE : 0;
		     j < max; ++j) {
			found = mytree_find_near(tree_contents[i], &tree_contents[j]->id);
			if (found != tree_contents[j]) {
				fprintf(stderr, "find_near found %p near %p (expected %p)\n",
					found, tree_contents[i], tree_contents[j]);
				found = mytree_find_near(tree_contents[i], &tree_contents[j]->id);
			}
		}
	}
}

#endif
	end = getCurTicks();
	fprintf(stderr, "find_near duration = %llu\n", end - start);

	start = getCurTicks();
for (k = 0; k < 8; ++k) {
	for (i = 0; i < cont.count; ++i) {
		int max = i + NEAR_RANGE;
		if (max > cont.count)
			max = cont.count;
		for (j = i > NEAR_RANGE ? i - NEAR_RANGE : 0;
		     j < max; ++j) {
			found = mytree_find(&cont, &tree_contents[j]->id);
			if (found != tree_contents[j]) {
				fprintf(stderr, "find_near found %p near %p (expected %p)\n",
					found, tree_contents[i], tree_contents[j]);
				found = mytree_find_near(tree_contents[i], &tree_contents[j]->id);
			}
		}
	}
}

	end = getCurTicks();
	fprintf(stderr, "find duration = %llu\n", end - start);

	// cleanup

	fprintf(stderr, "Forward iteration (%u objects)\n", cont.count);
	for (node = cont.leftmost; node ; node = rb_next(node)) {
		struct object *obj = (struct object *)__rb_node_to_obj(node, &mytree_rel);
		//fprintf(stderr, "id = 0x%08x\n", obj->id);
	}

	fprintf(stderr, "Reverse iteration (%u objects)\n", cont.count);
	for (node = cont.rightmost; node ; node = rb_prev(node)) {
		struct object *obj = (struct object *)__rb_node_to_obj(node, &mytree_rel);
		//fprintf(stderr, "id = 0x%08x\n", obj->id);
	}


	fprintf(stderr, "Starting cleanup, %u objects\n", cont.count);
	while (cont.leftmost) {
		struct object *obj = (struct object *)__rb_node_to_obj(cont.leftmost, &mytree_rel);
		//fprintf(stderr, "Removing object at 0x%p id = 0x%04x\n", obj, obj->id);
		mytree_remove(&cont, obj);
		//--cont.count;
	}

	if(obj_pools[0]) {
		free(obj_pools[0]);
		free(obj_pools[1]);
		obj_pools[0] = 0;
		obj_pools[1] = 0;
		free(tree_contents);
	}
	pool_in_use = 0;
	cont.leftmost = 0;
	cont.rightmost = 0;


	fprintf(stderr, "Cleanup complete, %u objects left in container.\n", cont.count);

	return 0;
}
#endif
