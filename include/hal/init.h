#include <hal/kernel.h>
#include <hal/compiler.h>
#include <hal/types.h>

typedef int (*initcall_t)(void);

#define __initcall__(_level, _fn) \
	ll_entry_declare(initcall_t, _fn, init_##_level) = _fn

#define __initcall_start(_level) ll_entry_start(initcall_t, init_##_level)
#define __initcall_end(_level) ll_entry_end(initcall_t, init_##_level)

#define hal_init(_level)				\
	do {						\
		initcall_t *start, *end, *f;		\
		start = __initcall_start(_level);	\
		end = __initcall_end(_level);		\
		for (f = start; f < end; f++)		\
			(*f)();				\
	} while (0)

typedef int (*finicall_t)(void);

#define __finicall__(_level, _fn) \
	ll_entry_declare(finicall_t, _fn, fini_##_level) = _fn

#define __finicall_start(_level) ll_entry_start(finicall_t, fini_##_level)
#define __finicall_end(_level) ll_entry_end(finicall_t, fini_##_level)

#define hal_fini(_level)				\
	do {						\
		finicall_t *start, *end, *f;		\
		start = __finicall_start(_level);	\
		end = __finicall_end(_level);		\
		for (f = start; f < end; f++)		\
			(*f)();				\
	} while (0)
