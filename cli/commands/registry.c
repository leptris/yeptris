/* registry.c — the command table. Adding a command = one row here. */

#include "commands.h"

int yeptris_cmd_version(int argc, char** argv);

const yeptris_command yeptris_command_table[] = {
    {"version", "yeptris version", "Print the library and CLI version.", yeptris_cmd_version},
};

const size_t yeptris_command_count =
    sizeof(yeptris_command_table) / sizeof(yeptris_command_table[0]);
