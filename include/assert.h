#ifndef __WISE_ASSERT_H__
#define __WISE_ASSERT_H__

#ifndef ASSERT_VERBOSITY
#define ASSERT_VERBOSITY CONFIG_DEFAULT_ASSERT_VERBOSITY
#endif

extern void _hal_assert_fail(const char *assertion, const char *file,
			    unsigned line, const char *func);
extern void (*hal_assert_fail)(const char *assert, const char *file,
	unsigned line, const char *func);

#if defined(NOBUG) || ASSERT_VERBOSITY == 0

/*
 * assert() does not do anything. If you think your module
 * is mature enough, add ccflags-y += -NODEBUG, or
 * ccflgas-y += -DASSERT_VERBOSITY=0 in the Makefile or Kbuild
 * of your module.
 */
#define assert(expr) 	((void) (expr))

#else

#if ASSERT_VERBOSITY == 1
/*
 * assert() just let the system die without printing the information.
 * This is to reduce the string literals. In order to use this,
 * add ccflags-y += -DASSERT_VERBOSITY=1 in the Makefile or Kbuild of
 * your module.
 */
#define assert(expr)							\
	((expr)								\
	 ? (void) 0							\
	 : hal_assert_fail(NULL, NULL, 0, NULL))

#else
/*
 * assert() prints out more verbose output and die.
 * This is default.
 */
#define assert(expr)							\
	((expr)								\
	 ? (void) 0							\
	 : hal_assert_fail(#expr, NULL, __LINE__, __func__))

#endif /* CONFIG_ASSERT_VERBOSITY == 1 */

#endif /* NOBUG */

#endif /* __WISE_ASSERT_H__ */
