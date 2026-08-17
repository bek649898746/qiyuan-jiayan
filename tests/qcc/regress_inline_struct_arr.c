// @EXPECTED exit:0
// @EXPECTED out:loose-object-1-pack-2-2
#include <stdio.h>

enum fsync_component {
    FSYNC_COMPONENT_LOOSE_OBJECT = 1,
    FSYNC_COMPONENT_PACK = 2,
};

static const struct fsync_component_name {
    const char *name;
    enum fsync_component component_bits;
} fsync_component_names[] = {
    { "loose-object", FSYNC_COMPONENT_LOOSE_OBJECT },
    { "pack", FSYNC_COMPONENT_PACK },
};

int main(void) {
    printf("%s-%d-%s-%d-%d\n",
           fsync_component_names[0].name,
           fsync_component_names[0].component_bits,
           fsync_component_names[1].name,
           fsync_component_names[1].component_bits,
           (int)(sizeof(fsync_component_names) / sizeof(fsync_component_names[0])));
    return 0;
}
