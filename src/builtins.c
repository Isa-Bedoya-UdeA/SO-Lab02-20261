/* =========================================================
 * builtins.c  —  Built-in commands for the wish shell
 * =========================================================
 * Owns the global search-path state and implements the three
 * built-in commands required by the spec:
 *
 *   exit  — terminate the shell (no arguments allowed)
 *   cd    — change working directory (exactly one argument)
 *   path  — overwrite the executable search path
 *
 * Built-ins are executed directly in the shell process; they
 * never go through fork/exec.
 * ========================================================= */

#include "shell.h"

/* ---------------------------------------------------------
 * Global search-path state
 * Declared extern in shell.h so executor.c can read it.
 * --------------------------------------------------------- */
char *search_dirs[MAX_SEARCH_DIRS];
int   dir_count = 0;

/* ---------------------------------------------------------
 * path_init
 *
 * Called once at startup.  Sets the initial search path to
 * {"/bin"} as required by the specification.
 * --------------------------------------------------------- */
void path_init(void)
{
    // Release anything that might already be there (defensive)
    for (int i = 0; i < dir_count; i++) {
        free(search_dirs[i]);
        search_dirs[i] = NULL;
    }
    dir_count      = 1;
    search_dirs[0] = strdup("/bin");
}

/* ---------------------------------------------------------
 * handle_exit
 *
 * The spec says passing any argument to exit is an error.
 * On a valid call we set *should_exit so the main loop can
 * finish the current line (waiting for parallel children)
 * before terminating — rather than calling exit() mid-line.
 * --------------------------------------------------------- */
static void handle_exit(const Command *cmd, int *should_exit)
{
    if (cmd->argc != 1) {
        // "exit foo" or "exit foo bar" → error, do NOT quit
        emit_error();
        return;
    }
    *should_exit = 1;
}

/* ---------------------------------------------------------
 * handle_cd
 *
 * Accepts exactly one argument.  Zero arguments or more than
 * one argument are errors.  A failing chdir() is also an
 * error (the directory may not exist or be inaccessible).
 * --------------------------------------------------------- */
static void handle_cd(const Command *cmd)
{
    if (cmd->argc != 2) {
        // covers both "cd" (no arg) and "cd a b" (too many)
        emit_error();
        return;
    }

    if (chdir(cmd->argv[1]) != 0)
        emit_error();
}

/* ---------------------------------------------------------
 * handle_path
 *
 * Replaces the entire search path with the directories given
 * as arguments.  Zero arguments → empty path (the shell can
 * no longer run external commands, which is intentional per
 * the spec).  Each previous entry is freed before the new
 * ones are stored so there are no memory leaks.
 * --------------------------------------------------------- */
static void handle_path(const Command *cmd)
{
    // Free every current entry
    for (int i = 0; i < dir_count; i++) {
        free(search_dirs[i]);
        search_dirs[i] = NULL;
    }
    dir_count = 0;

    /*
     * argv[0] is "path" itself; actual directories start at
     * argv[1].  We stop at MAX_SEARCH_DIRS to stay within
     * the bounds of the array.
     */
    for (int i = 1; cmd->argv[i] != NULL && dir_count < MAX_SEARCH_DIRS; i++)
        search_dirs[dir_count++] = strdup(cmd->argv[i]);
}

/* ---------------------------------------------------------
 * builtin_dispatch  (public)
 *
 * Check whether `cmd` is one of the known built-ins and, if
 * so, execute it.
 *
 * Returns 1  → command was a built-in (caller must NOT fork)
 * Returns 0  → command is external (caller should fork/exec)
 *
 * `should_exit` is set to 1 only when a valid "exit" is seen.
 * --------------------------------------------------------- */
int builtin_dispatch(const Command *cmd, int *should_exit)
{
    const char *name = cmd->argv[0];

    if (strcmp(name, "exit") == 0) {
        handle_exit(cmd, should_exit);
        return 1;
    }

    if (strcmp(name, "cd") == 0) {
        handle_cd(cmd);
        return 1;
    }

    if (strcmp(name, "path") == 0) {
        handle_path(cmd);
        return 1;
    }

    return 0; // not a built-in
}