/* main.c — dispatch through the command registry; no command logic here. */

#include <stdio.h>
#include <string.h>

#include "commands.h"

static int print_usage(FILE* out) {
    fprintf(out, "usage: yeptris <command> [args]\n\ncommands:\n");
    for (size_t i = 0; i < yeptris_command_count; i++) {
        const yeptris_command* cmd = &yeptris_command_table[i];
        fprintf(out, "  %-24s %s\n", cmd->usage, cmd->help);
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return print_usage(stderr);
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }

    for (size_t i = 0; i < yeptris_command_count; i++) {
        if (strcmp(argv[1], yeptris_command_table[i].name) == 0) {
            /* The command sees its own argv: argv[0] = command name. */
            return yeptris_command_table[i].run(argc - 1, argv + 1);
        }
    }

    fprintf(stderr, "yeptris: unknown command '%s'\n\n", argv[1]);
    return print_usage(stderr);
}
