#include <stdio.h>
#include <string.h>

/* Usage: test_core [--list] [NAME...]. No names runs every suite in registry
 * order; names run only those suites, so one suite can be iterated on or the
 * registry can be spread across processes by tools/run_tests.py. */
void test_block(void);
#define PE_TEST(name) int pe_test_##name(void);
#include "test_core_list.h"
#undef PE_TEST

static int run_block(void) { test_block(); return 0; }
static const struct { const char *name; int (*run)(void); } tests[] = {
    {"block", run_block},
#define PE_TEST(name) {#name, pe_test_##name},
#include "test_core_list.h"
#undef PE_TEST
};
enum { TEST_COUNT = sizeof tests / sizeof tests[0] };

int main(int argc, char **argv) {
    if(argc == 2 && !strcmp(argv[1], "--list")) {
        for(int i = 0; i < TEST_COUNT; i++) puts(tests[i].name);
        return 0;
    }
    for(int a = 1; a < argc; a++) {
        int known = 0;
        for(int i = 0; i < TEST_COUNT; i++) known |= !strcmp(argv[a], tests[i].name);
        if(!known) { fprintf(stderr, "Unknown core test '%s'; see --list\n", argv[a]); return 2; }
    }
    int failures = 0, ran = 0;
    for(int i = 0; i < TEST_COUNT; i++) {
        int selected = argc == 1;
        for(int a = 1; a < argc && !selected; a++) selected = !strcmp(argv[a], tests[i].name);
        if(!selected) continue;
        failures += tests[i].run();
        ran++;
    }
    printf("Core tests: %s (%d failures, %d of %d suites)\n", failures ? "FAILED" : "PASS", failures, ran, TEST_COUNT);
    return failures ? 1 : 0;
}
