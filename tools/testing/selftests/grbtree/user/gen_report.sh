#!/bin/bash

dbfile=results.$$.db
datafile=runtest.out

die() {
	echo "ERROR${@:+": "}$@" 1>&2
	exit -1
}

find_sqlite() {
	for suffix in "" 4 3; do
		which sqlite${suffix} 2> /dev/null && return 0
	done
	return 1
}

sqlite=$(find_sqlite) || die "failed to find sqlite"

${sqlite} "${dbfile}" << asdf
/* .echo on */
.headers on
create table if not exists grbtest_result (
	compiler	varchar(255),
	key_type	varchar(255),
	userland	tinyint,
	use_generic	tinyint,
	use_leftmost	tinyint,
	use_rightmost	tinyint,
	use_count	tinyint,
	unique_keys	tinyint,
	insert_replaces	tinyint,
	use_augmented	tinyint,
	debug		tinyint,
	debug_validate	tinyint,
	arch		varchar(255),
	arch_flags	varchar(255),
	processor	varchar(255),
	cc		varchar(255),
	cflags		varchar(255),
	test		tinyint,
	in_seed		bigint,
	seed		bigint,
	key_mask	int,
	object_count	int,
	pool_count	int,
	reps		bigint,
	node_size	int,
	object_size	int,
	pool_size	int,
	insertions	bigint,
	insertion_time	bigint,
	evictions	bigint,
	deletions	bigint,
	deletion_time	bigint
);
.separator |
.import ${datafile} grbtest_result
/* .mode column */
select distinct
	key_type,
	userland,
	use_leftmost,
	use_rightmost,
	use_count,
	unique_keys,
	insert_replaces,
	use_augmented,
	debug,
	debug_validate,
	arch,
	arch_flags,
	processor,
	cc,
	test,
	in_seed,
	seed,
	key_mask,
	object_count,
	pool_count,
	reps,
	node_size,
	object_size,
	pool_size,
	insertions,
	evictions,
	deletions
from grbtest_result;

select distinct
	a.compiler as 'Compiler',
	a.key_type,
	(case when a.userland then	  'U' else 'K' end) ||
	(case when a.use_leftmost then	  'L' else '.' end) ||
	(case when a.use_rightmost then	  'R' else '.' end) ||
	(case when a.use_count then	  'C' else '.' end) ||
	(case when a.unique_keys then	  'U' else '.' end) ||
	(case when a.insert_replaces then 'I' else '.' end) ||
	(case when a.debug then		  'D' else '.' end) ||
	(case when a.debug_validate then  'V' else '.' end)
	as config,
	a.insertion_time as 'Generic Insert Time',
	b.insertion_time as 'Hand-Coded Insert Time',
	1.0 * a.insertion_time / b.insertion_time - 1.0 as 'Insert Diff',
	a.deletion_time as 'Generic Delete Time',
	b.deletion_time as 'Hand-Coded Delete Time',
	1.0 * a.deletion_time / b.deletion_time - 1.0 as 'Delete Diff'
from
	grbtest_result as a inner join grbtest_result as b on (
		a.compiler = b.compiler
	)
where
	a.use_generic == 1
	and b.use_generic = 0;
asdf

rm "${dbfile}"

