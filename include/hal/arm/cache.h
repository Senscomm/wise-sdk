#ifndef __CACHE_H__
#define __CACHE_H__

#ifdef CONFIG_SYS_ENABLE_DCACHE
#define DCACHE_SIZE	(CONFIG_SYS_DCACHE_LINE_SIZE)
extern void armv7a_dcache_invalidate_all(void);
extern void armv7a_dcache_clean_all(void);
extern void armv7a_dcache_clean_invalidate_all(void);
extern void armv7a_dcache_invalidate_va(void *va);
extern void armv7a_dcache_clean_va(void *va);
extern void armv7a_dcache_clean_invalidate_va(void *va);
#else
#define DCACHE_SIZE	(0)
inline void armv7a_dcache_invalidate_all(void) {};
inline void armv7a_dcache_clean_all(void) {};
inline void armv7a_dcache_clean_invalidate_all(void) {};
inline void armv7a_dcache_invalidate_va(void *va) {(void)va;}
inline void armv7a_dcache_clean_va(void *va) {(void)va;}
inline void armv7a_dcache_clean_invalidate_va(void *va) {(void)va};
#endif
#if defined(CONFIG_SYS_ENABLE_ICACHE) \
	|| defined(CONFIG_SYS_EANBLE_DCACHE)
extern void armv7a_cache_enable(void);
#else
inline void armv7a_cache_enable(void) {};
#endif
#endif /* __CACHE_H__ */
