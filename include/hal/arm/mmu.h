#ifndef __MMU_H
#define __MMU_H

extern uint32_t sect_normal(void);
extern uint32_t sect_normal_nc(void);
extern uint32_t sect_normal_exec(void);
extern uint32_t sect_normal_ro(void);
extern uint32_t sect_normal_rw(void);
extern uint32_t sect_device_ro(void);
extern uint32_t sect_device_rw(void);
extern uint32_t page_l1_4k(void);
extern uint32_t page_l1_64k(void);
extern uint32_t page_4k_device_rw(void);
extern uint32_t page_64k_device_rw(void);

#endif
