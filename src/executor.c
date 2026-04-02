/* =========================================================
 * executor.c  —  Process execution for the wish shell
 * =========================================================
 * Responsibilities:
 *   1. resolve_executable : find the full path of a command
 *      by walking the search_dirs[] array.
 *   2. setup_redirection  : open the output file and wire
 *      stdout + stderr to it inside the child process.
 *   3. run_command        : fork a child, set up redirection,
 *      and execv() the resolved executable.  Returns the
 *      child PID so the caller can wait() later.
 *   4. run_line           : top-level orchestrator that ties
 *      the parser, built-in dispatcher, and run_command
 *      together for one full input line.
 * ========================================================= */

#include "shell.h"

/* ---------------------------------------------------------
 * resolve_executable
 *
 * Walk every directory in search_dirs[] and build the
 * candidate path  "<dir>/<name>".  Use access(X_OK) to test
 * whether that file exists and is executable.
 *
 * On success writes the full path into `buf` (size `buf_sz`)
 * and returns `buf`.  Returns NULL when no match is found.
 * --------------------------------------------------------- */
static char *resolve_executable(const char *name, char *buf, size_t buf_sz)
{
    for (int i = 0; i < dir_count; i++) {
        int written = snprintf(buf, buf_sz, "%s/%s", search_dirs[i], name);

        // Guard against truncation (path longer than buf_sz-1)
        if (written < 0 || (size_t)written >= buf_sz)
            continue;

        if (access(buf, X_OK) == 0)
            return buf;
    }
    return NULL;
}

/* ---------------------------------------------------------
 * setup_redirection
 *
 * Called inside the child process only.  Opens `filepath`
 * for writing (creating or truncating it), then redirects
 * both stdout and stderr to that file descriptor so all
 * output from the child lands in the file.
 *
 * Returns  0 on success.
 * Returns -1 on failure (error already emitted).
 * --------------------------------------------------------- */
static int setup_redirection(const char *filepath)
{
    int fd = open(filepath,
                  O_WRONLY | O_CREAT | O_TRUNC,
                  0644);
    if (fd < 0) {
        emit_error();
        return -1;
    }

    if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
        emit_error();
        close(fd);
        return -1;
    }

    close(fd); // the dup'd descriptors keep the file open
    return 0;
}

/* ---------------------------------------------------------
 * run_command  (public)
 *
 * Fork a new process, optionally redirect its output, locate
 * the executable and execv() it.
 *
 * The parent does NOT wait here — it just returns the child
 * PID.  This design lets run_line collect all PIDs first and
 * then wait for all of them together, enabling true parallel
 * execution of commands separated by '&'.
 *
 * Returns the child PID (> 0) on success.
 * Returns -1 if the search path is empty or fork() failed.
 * --------------------------------------------------------- */
pid_t run_command(const Command *cmd)
{
    // Refuse immediately when no search directories are configured
    if (dir_count == 0) {
        emit_error();
        return -1;
    }

    pid_t child = fork();

    if (child < 0) {
        // fork itself failed — OS resource issue
        emit_error();
        return -1;
    }

    if (child == 0) {
        // ---- CHILD PROCESS --------------------------------

        // 1. Wire up output redirection if requested
        if (cmd->output_file != NULL) {
            if (setup_redirection(cmd->output_file) < 0)
                exit(1);
        }

        // 2. Find the executable in the search path
        char full_path[PATH_BUF_SIZE];
        if (resolve_executable(cmd->argv[0], full_path, sizeof(full_path)) == NULL) {
            emit_error();
            exit(1);
        }

        // 3. Replace this process image with the target program
        execv(full_path, cmd->argv);

        /*
         * execv() only returns on failure (e.g. the binary was
         * removed between access() and execv()).
         */
        emit_error();
        exit(1);
    }

    // ---- PARENT PROCESS -----------------------------------
    // Return the child's PID; the caller will wait() on it.
    return child;
}

/* ---------------------------------------------------------
 * run_line  (public)
 *
 * High-level driver for one complete input line:
 *
 *   1. Split the line on '&' to get parallel segments.
 *   2. For each segment, parse it into a Command.
 *   3. If it is a built-in, dispatch it (no fork needed).
 *   4. Otherwise launch it with run_command() and record
 *      the child PID.
 *   5. After launching ALL children, wait() for each one.
 *
 * Waiting is deferred until all children are launched so
 * that commands genuinely run in parallel rather than
 * sequentially.
 *
 * Returns 1 if the shell should exit after this line,
 *         0 otherwise.
 * --------------------------------------------------------- */
int run_line(char *line)
{
    char  *segments[MAX_PARALLEL];
    int    seg_count  = split_parallel(line, segments, MAX_PARALLEL);
    int    should_exit = 0;
    pid_t  child_pids[MAX_PARALLEL];
    int    child_count = 0;

    for (int i = 0; i < seg_count; i++) {
        Command cmd;

        // parse_command modifies the string in place
        if (parse_command(segments[i], &cmd) < 0) {
            free(segments[i]);
            continue; // parse error already emitted; keep going
        }

        if (builtin_dispatch(&cmd, &should_exit)) {
            /*
             * Built-in commands run synchronously in the shell
             * process — no PID to track.
             * If exit was requested we still finish launching any
             * remaining parallel commands on this line before
             * honoring it (matching reference-shell behaviour).
             */
        } else {
            pid_t pid = run_command(&cmd);
            if (pid > 0)
                child_pids[child_count++] = pid;
        }

        free(segments[i]);
    }

    // Wait for every child launched on this line
    for (int i = 0; i < child_count; i++)
        waitpid(child_pids[i], NULL, 0);

    return should_exit;
}