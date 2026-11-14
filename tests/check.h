#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>
static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define REPORT(name) do { printf("%s: %d checks, %d failures\n", name, g_checks, g_failures); \
    return g_failures ? 1 : 0; } while (0)
#endif
