#ifndef _LINUX_PERCPU_RWSEM_H
#define _LINUX_PERCPU_RWSEM_H

#include <linux/rwsem.h>
#include <linux/percpu.h>

#ifndef CONFIG_SMP

#define percpu_rw_semaphore	rw_semaphore
#define percpu_rwsem_ptr	int
#define percpu_down_read(x)	(down_read(x), 0)
#define percpu_up_read(x, y)	up_read(x)
#define percpu_down_write	down_write
#define percpu_up_write		up_write
#define percpu_init_rwsem(x)	(({init_rwsem(x);}), 0)
#define percpu_free_rwsem(x)	do { } while (0)

#else

struct percpu_rw_semaphore {
	struct rw_semaphore __percpu *s;
};

typedef struct rw_semaphore *percpu_rwsem_ptr;

static inline percpu_rwsem_ptr percpu_down_read(struct percpu_rw_semaphore *sem)
{
	struct rw_semaphore *s = __this_cpu_ptr(sem->s);
	down_read(s);
	return s;
}

static inline void percpu_up_read(struct percpu_rw_semaphore *sem, percpu_rwsem_ptr s)
{
	up_read(s);
}

static inline void percpu_down_write(struct percpu_rw_semaphore *sem)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		struct rw_semaphore *s = per_cpu_ptr(sem->s, cpu);
		down_write(s);
	}
}

static inline void percpu_up_write(struct percpu_rw_semaphore *sem)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		struct rw_semaphore *s = per_cpu_ptr(sem->s, cpu);
		up_write(s);
	}
}

static inline int percpu_init_rwsem(struct percpu_rw_semaphore *sem)
{
	int cpu;
	sem->s = alloc_percpu(struct rw_semaphore);
	if (unlikely(!sem->s))
		return -ENOMEM;
	for_each_possible_cpu(cpu) {
		struct rw_semaphore *s = per_cpu_ptr(sem->s, cpu);
		init_rwsem(s);
	}
	return 0;
}

static inline void percpu_free_rwsem(struct percpu_rw_semaphore *sem)
{
	free_percpu(sem->s);
	sem->s = NULL;		/* catch use after free bugs */
}

#endif

#endif
