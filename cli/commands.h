/* commands.h — the CLI command registry.
 *
 * Adding a command = new file in commands/ implementing yeptris_cmd_<name>
 * plus one row in the table in commands/registry.c. No core edits, no
 * switches in main (OCP contract, cli/ design mirrors libleptris).
 * Commands do argument parsing and output formatting only — all real work
 * goes through the public API.
 */
#ifndef YEPTRIS_CLI_COMMANDS_H
#define YEPTRIS_CLI_COMMANDS_H

#include <stddef.h>

typedef struct yeptris_command {
    const char* name;   /* invoked as: yeptris <name> [args] */
    const char* usage;  /* argument summary shown in the command listing */
    const char* help;   /* one-line description */
    /* Runs the command. argv[0] is the command name; returns the process
     * exit code (0 success, 1 error/usage — the CLI-wide contract). */
    int (*run)(int argc, char** argv);
} yeptris_command;

/* The registry. Defined in commands/registry.c; read by main.c. */
extern const yeptris_command yeptris_command_table[];
extern const size_t yeptris_command_count;

/* Command entry points (one per file in commands/). */
int yeptris_cmd_version(int argc, char** argv);

#endif /* YEPTRIS_CLI_COMMANDS_H */
