#include <ada/ada_conf.h>
#include <ada/sprop.h>

struct prop_conf_metadata {
    char *prop_name;
    u8 tag;
    u8 exist;
    enum conf_token token;
    struct ada_conf_item item;
};

void host_prop_mgr_init(struct prop_conf_metadata *prop_conf_table, struct ada_sprop *table, u8 size);
void host_prop_conf_load(struct prop_conf_metadata *prop_conf_table);
void host_prop_unsync_tag(struct ada_sprop *sprop, struct prop_conf_metadata *prop_conf_table);