/* version.c — `yeptris version`: print CLI + library version. */

#include <stdio.h>

#include <yeptris.h>

#include "commands.h"

int yeptris_cmd_version(int argc, char** argv) {
    (void)argc;
    (void)argv;
    printf("yeptris %s\n", yeptris_version());
    return 0;
}
