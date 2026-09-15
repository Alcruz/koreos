#include "smoke.h"

static smoke_env_t env;

void kernel_main(void *dtb)
{
    smoke_boot(dtb, &env);

    int ok = 1;
    void *p = kmalloc(env.heap, 64);
    ok = ok && p && ((uintptr_t)p % HEAP_ALIGN) == 0;
    if (p) kfree(env.heap, p);

    uint8_t *z = kzalloc(env.heap, 64);
    ok = ok && z != NULL;
    if (z) {
        for (size_t i = 0; i < 64; i++)
            if (z[i] != 0) { ok = 0; break; }
        kfree(env.heap, z);
    }

    SMOKE_REPORT("heap", ok);
}
