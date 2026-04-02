/* =========================================================
 * wish.c  —  Entry point for the wish shell
 * =========================================================
 * Responsibilities:
 *   - Validate command-line arguments (0 or 1 allowed).
 *   - Open the batch file when one is provided.
 *   - Run the read-eval loop:
 *       · print the prompt only in interactive mode
 *       · read one line with getline()
 *       · hand the line to run_line() in executor.c
 *       · exit cleanly on EOF or when "exit" was seen
 *
 * All parsing, built-in handling, and process management
 * live in their own translation units (parser.c,
 * builtins.c, executor.c).
 * ========================================================= */

#include "shell.h"

// Prompt printed in interactive mode only (spec: "wish> ")
#define PROMPT  "wish> "

int main(int argc, char *argv[])
{
    // ---- 1. Validate arguments -----------------------------
    if (argc > 2) {
        emit_error();
        exit(1);
    }

    // ---- 2. Open input source ------------------------------
    FILE *input       = stdin;
    int   interactive = (argc == 1); // true → interactive mode

    if (!interactive) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            emit_error();
            exit(1);
        }
    }

    // ---- 3. Initialise search path to {"/bin"} ------------
    path_init();

    // ---- 4. Read-eval loop ---------------------------------
    char *line    = NULL;
    size_t buf_len = 0;
    ssize_t nread;

    while (1) {
        // Print prompt only when reading from a terminal
        if (interactive) {
            printf(PROMPT);
            fflush(stdout);
        }

        nread = getline(&line, &buf_len, input);
        if (nread == -1)
            break; // EOF in either mode → exit cleanly

        // Strip surrounding whitespace; skip blank lines silently
        char *trimmed = strip_whitespace(line);
        if (*trimmed == '\0')
            continue;

        // Dispatch the line; stop the loop if "exit" was issued
        int should_exit = run_line(trimmed);
        if (should_exit)
            break;
    }

    // ---- 5. Clean up ---------------------------------------
    free(line);

    // Release all strdup'd search-path entries
    for (int i = 0; i < dir_count; i++) {
        free(search_dirs[i]);
        search_dirs[i] = NULL;
    }
    dir_count = 0;

    if (!interactive)
        fclose(input);

    return 0;
}