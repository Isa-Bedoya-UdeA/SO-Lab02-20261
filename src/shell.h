#ifndef SHELL_H
#define SHELL_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ---------------------------------------------------------
 * Limits
 * --------------------------------------------------------- */
#define MAX_SEARCH_DIRS 64 // max entries in the search path
#define MAX_TOKENS  128 // max words in a single command
#define MAX_PARALLEL 64 // max parallel commands per input line
#define PATH_BUF_SIZE 512 // scratch buffer for building full paths

/* ---------------------------------------------------------
 * The one and only error message (spec requirement)
 * --------------------------------------------------------- */
#define ERROR_MSG "An error has occurred\n"
#define ERROR_LEN 22 // strlen(ERROR_MSG)

/* ---------------------------------------------------------
 * Parsed representation of a single command
 *
 * Example:  ls -la /tmp > out.txt
 *   argv        = ["ls", "-la", "/tmp", NULL]
 *   argc        = 3
 *   output_file = "out.txt"   (NULL when no redirection)
 * --------------------------------------------------------- */
typedef struct {
    char *argv[MAX_TOKENS]; // NULL-terminated argument vector
    int argc; // number of arguments (argv[0] = command)
    char *output_file; // redirect target, or NULL
} Command;

/* ---------------------------------------------------------
 * Global search-path state  (defined in builtins.c)
 * --------------------------------------------------------- */
extern char *search_dirs[]; // array of directory strings
extern int dir_count; // how many entries are currently active

/* ---------------------------------------------------------
 * Utility  (shell.h inline — used everywhere)
 * --------------------------------------------------------- */

// emit_error: write the standard error message to stderr
static inline void emit_error(void)
{
    write(STDERR_FILENO, ERROR_MSG, ERROR_LEN);
}

/* strip_whitespace: returns a pointer into `s` after leading
 * spaces/tabs/newlines, and NUL-terminates before any trailing ones.
 * Does NOT allocate; the caller owns the original buffer. */
static inline char *strip_whitespace(char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n')
        s++;
    if (*s == '\0')
        return s;
    char *tail = s + strlen(s) - 1;
    while (tail > s && (*tail == ' ' || *tail == '\t' || *tail == '\n'))
        tail--;
    *(tail + 1) = '\0';
    return s;
}

/* ---------------------------------------------------------
 * parser.c  —  public interface
 * --------------------------------------------------------- */

/*
 * split_parallel: split `line` on '&' and return the number of
 * sub-command strings written into `out[]`.  Each element is a
 * freshly malloc'd copy; the caller must free them.
 */
int split_parallel(char *line, char *out[], int max_out);

/*
 * parse_command: fill `cmd` by parsing `raw_cmd` (a single command
 * string, no '&').  Handles '>' redirection detection and
 * tokenisation.  Returns 0 on success, -1 on parse error (the
 * error message has already been emitted).
 */
int parse_command(char *raw_cmd, Command *cmd);

/* ---------------------------------------------------------
 * builtins.c  —  public interface
 * --------------------------------------------------------- */

/* initialise the search path to {"/bin"} */
void path_init(void);

/*
 * builtin_dispatch: if `cmd` is a built-in (exit/cd/path) handle it
 * and return 1.  If `cmd` is NOT a built-in return 0 so the caller
 * can hand it to the executor.  Sets *should_exit = 1 when the shell
 * must terminate cleanly after this line.
 */
int builtin_dispatch(const Command *cmd, int *should_exit);

/* ---------------------------------------------------------
 * executor.c  —  public interface
 * --------------------------------------------------------- */

/*
 * run_command: fork a child, set up optional output redirection, find
 * the executable in search_dirs[], and execv() it.
 * Returns the child PID on success, -1 if fork failed (error emitted).
 * The caller is responsible for wait()-ing on the returned PID.
 */
pid_t run_command(const Command *cmd);

/*
 * run_line: orchestrate parallel execution of all commands on one
 * input line.  Handles splitting on '&', parsing each sub-command,
 * dispatching built-ins, launching external processes, and
 * wait()-ing for all children before returning.
 * Returns 1 if the shell should exit, 0 otherwise.
 */
int run_line(char *line);

#endif