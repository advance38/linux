#ifndef _LINUX_BUG_H
#define _LINUX_BUG_H

#include <asm/bug.h>
#include <linux/compiler.h>

enum bug_trap_type {
	BUG_TRAP_TYPE_NONE = 0,
	BUG_TRAP_TYPE_WARN = 1,
	BUG_TRAP_TYPE_BUG = 2,
};

struct pt_regs;

#ifdef __CHECKER__
#define BUILD_BUG_ON_NOT_POWER_OF_2(n)
#define BUILD_BUG_ON_ZERO(e) (0)
#define BUILD_BUG_ON_NULL(e) ((void*)0)
#define BUILD_BUG_ON(condition)
#define BUILD_BUG() (0)
#else /* __CHECKER__ */

/* Force a compilation error if a constant expression is not a power of 2 */
#define BUILD_BUG_ON_NOT_POWER_OF_2(n)			\
	BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))

/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))

/*
 * BUILD_BUG_ON_INVALID() permits the compiler to check the validity of the
 * expression but avoids the generation of any code, even if that expression
 * has side-effects.
 */
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((__force long)(e))))

/**
 * BUILD_BUG_ON - break compile if a condition is true.
 * @condition: the condition which the compiler should know is false.
 *
 * If you have some code which relies on certain constants being equal, or
 * some other compile-time-evaluated condition, you should use BUILD_BUG_ON to
 * detect if someone changes it.
 *
 * The implementation uses gcc's reluctance to create a negative array, but
 * gcc (as of 4.4) only emits that error for obvious cases (eg. not arguments
 * to inline functions).  Luckily, in 4.3 they added the "error" function
 * attribute just for this type of case.  Thus, we use a negative sized array
 * (should always create an error pre-gcc-4.4) and then call an undefined
 * function with the error attribute (should always creates an error 4.3+).  If
 * for some reason, neither creates a compile-time error, we'll still have a
 * link-time error, which is harder to track down.
 */
#ifndef __OPTIMIZE__
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else
#define BUILD_BUG_ON(condition)						\
	do {								\
		extern void __build_bug_on_failed(void)			\
			__compiletime_error("BUILD_BUG_ON failed");	\
		((void)sizeof(char[1 - 2*!!(condition)]));		\
		if (condition)						\
			__build_bug_on_failed();			\
	} while(0)
#endif

/**
 * BUILD_BUG - break compile if used.
 *
 * If you have some code that you expect the compiler to eliminate at
 * build time, you should use BUILD_BUG to detect if it is
 * unexpectedly used.
 */
#define BUILD_BUG()						\
	do {							\
		extern void __build_bug_failed(void)		\
			__compiletime_error("BUILD_BUG failed");\
		__build_bug_failed();				\
	} while (0)

/**
 * BUILD_BUG_ON_NON_CONST - break compile if expression cannot be determined
 *                          to be a compile-time constant.
 * @exp: value to test for compile-time constness
 *
 * __builtin_constant_p() is a work in progress and is broken in various ways
 * on various versions of gcc and optimization levels. It can fail, even when
 * gcc otherwise determines that the expression is compile-time constant when
 * performing actual optimizations and thus, compile out the value anyway. Do
 * not use this macro for struct members or dereferenced pointers and arrays,
 * as these are broken in many versions of gcc -- use BUILD_BUG_ON_NON_CONST42
 * or another gcc-version-checked macro instead.
 *
 * As long as you are passing a variable declared const (and not modified),
 * this macro should never fail (except for floats).  For information on gcc's
 * behavior in other cases, see below.
 *
 * Gory Details:
 *
 * Normal primitive variables
 * - global non-static non-const values are never compile-time constants (but
 *   you should already know that)
 * - all const values (global/local, non/static) should never fail this test
 *   (3.4+) with one exception (below)
 * - floats (which we wont use anyway) are broken in various ways until 4.2
 *   (-O1 broken until 4.4)
 * - local static non-const broken until 4.2 (-O1 broken until 4.3)
 * - local non-static non-const broken until 4.0
 *
 * Dereferencing pointers & arrays
 * - all static const derefs broken until 4.4 (except arrays at -O2 or better,
 *   which are fixed in 4.2)
 * - global non-static const pointer derefs always fail (<=4.7)
 * - local non-static const derefs broken until 4.3, except for array derefs
 *   to a zero value, which works from 4.0+
 * - local static non-const pointers always fail (<=4.7)
 * - local static non-const arrays broken until 4.4
 * - local non-static non-const arrays broken until 4.0 (unless zero deref,
 *   works in 3.4+)

 */
#ifdef __OPTIMIZE__
#define BUILD_BUG_ON_NON_CONST(exp) \
	BUILD_BUG_ON(!__builtin_constant_p(exp))
#else
#define BUILD_BUG_ON_NON_CONST(exp)
#endif


#if GCC_VERSION >= 40200
/**
 * BUILD_BUG_ON_NON_CONST42 - break compile if expression cannot be determined
 *                            to be a compile-time constant (disabled prior to
 *                            gcc 4.2)
 * @exp: value to test for compile-time constness
 *
 * Use this macro instead of BUILD_BUG_ON_NON_CONST when testing struct
 * members or dereferenced arrays and pointers.  Note that the version checks
 * for this macro are not perfect.  BUILD_BUG_ON_NON_CONST42 expands to nothing
 * prior to gcc-4.2, after which it is the same as BUILD_BUG_ON_NON_CONST.
 * However, there are still many checks that will break with this macro (see
 * the Gory Details section of BUILD_BUG_ON_NON_CONST for more info).
 *
 * See also BUILD_BUG_ON_NON_CONST()
 */
# define BUILD_BUG_ON_NON_CONST42(exp) BUILD_BUG_ON_NON_CONST(exp)

/**
 * BUILD_BUG_ON42 - break compile if expression cannot be determined
 *                   (disabled prior to gcc 4.2)
 *
 * This gcc-version check is necessary due to breakages in testing struct
 * members prior to gcc 4.2.
 *
 * See also BUILD_BUG_ON()
 */
# define BUILD_BUG_ON42(arg) BUILD_BUG_ON(arg)
#else
# define BUILD_BUG_ON_NON_CONST42(exp)
# define BUILD_BUG_ON42(arg)
#endif /* GCC_VERSION >= 40200 */


#endif	/* __CHECKER__ */

#ifdef CONFIG_GENERIC_BUG
#include <asm-generic/bug.h>

static inline int is_warning_bug(const struct bug_entry *bug)
{
	return bug->flags & BUGFLAG_WARNING;
}

const struct bug_entry *find_bug(unsigned long bugaddr);

enum bug_trap_type report_bug(unsigned long bug_addr, struct pt_regs *regs);

/* These are defined by the architecture */
int is_valid_bugaddr(unsigned long addr);

#else	/* !CONFIG_GENERIC_BUG */

static inline enum bug_trap_type report_bug(unsigned long bug_addr,
					    struct pt_regs *regs)
{
	return BUG_TRAP_TYPE_BUG;
}

#endif	/* CONFIG_GENERIC_BUG */
#endif	/* _LINUX_BUG_H */
