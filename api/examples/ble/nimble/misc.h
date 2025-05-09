#ifndef _MISC_H_
#define _MISC_H_

#define MODLOG_DFLT(level, ...) printf(__VA_ARGS__)

void print_bytes(const uint8_t *bytes, int len);
void print_mbuf(const struct os_mbuf *om);
void print_addr(const void *addr);
void print_uuid(const ble_uuid_t *uuid);
void print_conn_desc(const struct ble_gap_conn_desc *desc);
void print_adv_fields(const struct ble_hs_adv_fields *fields);
char *addr_str(const void *addr);

#endif //_MISC_H_
