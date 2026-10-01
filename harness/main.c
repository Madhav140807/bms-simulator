#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csv.h"
#include "scenario.h"

#define DEFAULT_SCENARIO "discharge_1c"
#define DEFAULT_EVERY_S  60

static void usage(FILE *out)
{
    fprintf(out, "usage: bms [scenario] [--every SECONDS] [--can] | --list\n"
                 "Runs a scenario and prints CSV to stdout (default %s, every %d s).\n"
                 "--can prints every CAN frame in candump log format instead.\n",
            DEFAULT_SCENARIO, DEFAULT_EVERY_S);
}

static void list(void)
{
    for (int i = 0; i < scenario_count(); i++) {
        const scenario_t *s = scenario_get(i);
        printf("%-14s %s\n", s->name, s->desc);
    }
}

int main(int argc, char **argv)
{
    const char *name = DEFAULT_SCENARIO;
    long every = DEFAULT_EVERY_S;
    int can = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            list();
            return 0;
        } else if (strcmp(argv[i], "--can") == 0) {
            can = 1;
        } else if (strcmp(argv[i], "--every") == 0 && i + 1 < argc) {
            every = strtol(argv[++i], NULL, 10);
        } else if (argv[i][0] == '-') {
            usage(stderr);
            return 2;
        } else {
            name = argv[i];
        }
    }
    const scenario_t *scn = scenario_find(name);
    if (scn == NULL || every <= 0) {
        if (scn == NULL) {
            fprintf(stderr, "unknown scenario: %s\n", name);
        }
        usage(stderr);
        return 2;
    }
    if (can) {
        csv_run_can(stdout, scn);
    } else {
        csv_run(stdout, scn, (uint32_t)every);
    }
    return 0;
}
