#include "smoke.h"

static smoke_env_t env;

void kernel_main(void *dtb)
{
    smoke_boot(dtb, &env);

    size_t before = pmm_free_pages(env.pmm);
    pmm_page_t *a = pmm_alloc_page(env.pmm);
    pmm_page_t *b = pmm_alloc_page(env.pmm);
    int ok = a && b && a != b && pmm_free_pages(env.pmm) == before - 2;
    pmm_free_page(env.pmm, a);
    pmm_free_page(env.pmm, b);
    ok = ok && pmm_free_pages(env.pmm) == before;

    SMOKE_REPORT("pmm", ok);
}
