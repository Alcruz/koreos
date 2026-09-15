#include "smoke.h"
#include "../../kernel/include/task.h"

static smoke_env_t env;
static task_t *g_boot;
static task_t *g_demo;
static volatile int demo_ran;

static void demo_entry(void *arg)
{
    (void)arg;
    demo_ran = 1;
    switch_to(g_demo, g_boot);
    /* Not reached. */
}

void kernel_main(void *dtb)
{
    smoke_boot(dtb, &env);

    g_boot = task_create(env.pmm, env.heap);
    g_demo = task_create(env.pmm, env.heap);
    if (!g_boot || !g_demo)
        smoke_fail("task_switch", "task_create returned NULL");

    task_start(g_demo, demo_entry, NULL);
    switch_to(g_boot, g_demo);

    int ok = demo_ran == 1;

    task_destroy(env.pmm, env.heap, g_boot);
    task_destroy(env.pmm, env.heap, g_demo);

    SMOKE_REPORT("task_switch", ok);
}
