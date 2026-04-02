/* =========================================================
 * parser.c  —  Input parsing for the wish shell
 * =========================================================
 * Responsibilities:
 *   1. split_parallel : break a raw input line on '&' so that
 *      each parallel segment can be handled independently.
 *   2. parse_command  : turn one segment into a Command struct,
 *      resolving any '>' output-redirection on the way.
 * ========================================================= */

#include "shell.h"

/* ---------------------------------------------------------
 * split_parallel
 *
 * Walk `line` using strsep (which modifies the string in
 * place) splitting on '&'.  Each non-empty segment is
 * stripped of surrounding whitespace and strdup'd into
 * out[].  Returns the number of segments stored.
 * --------------------------------------------------------- */
int split_parallel(char *line, char *out[], int max_out)
{
    int count   = 0;
    char *cursor = line;
    char *segment;

    while ((segment = strsep(&cursor, "&")) != NULL) {
        char *trimmed = strip_whitespace(segment);
        if (*trimmed == '\0')
            continue; // skip blank segments
        if (count >= max_out)
            break; // safety: never overflow out[]
        out[count++] = strdup(trimmed);
    }

    return count;
}

/* ---------------------------------------------------------
 * validate_redirection
 *
 * Check that the portion to the right of '>' in a command
 * is exactly one word with no embedded spaces and no second
 * '>' character.  Returns 1 if valid, 0 if malformed.
 * --------------------------------------------------------- */
static int validate_redirection(const char *target)
{
    if (target == NULL || *target == '\0')
        return 0;

    // A second '>' anywhere makes this invalid
    if (strchr(target, '>') != NULL)
        return 0;

    // The target must be a single token — no spaces allowed
    if (strchr(target, ' ') != NULL || strchr(target, '\t') != NULL)
        return 0;

    return 1;
}

/* ---------------------------------------------------------
 * parse_command
 *
 * Given a single-command string `raw_cmd` (no '&'):
 *   a) Detect and extract a '>' redirection target.
 *   b) Tokenise the remaining part into cmd->argv[].
 *   c) Fill cmd->argc and cmd->output_file.
 *
 * Returns  0 on success.
 * Returns -1 on any parse error (error already emitted).
 * --------------------------------------------------------- */
int parse_command(char *raw_cmd, Command *cmd)
{
    // Zero-initialise so every field has a defined value
    memset(cmd, 0, sizeof(Command));
    cmd->output_file = NULL;

    // --- Step 1: handle redirection ------------------------
    char *redir_marker = strchr(raw_cmd, '>');
    if (redir_marker != NULL) {
        *redir_marker = '\0'; // split the string
        char *target = strip_whitespace(redir_marker + 1);

        if (!validate_redirection(target)) {
            emit_error();
            return -1;
        }

        /*
         * Make sure there is no second word after the filename.
         * e.g.  "ls > a b"  should be an error.
         * After strip_whitespace the target starts at the first
         * non-space; find where the first token ends and check
         * that only whitespace follows it.
         */
        char *space_after = strpbrk(target, " \t");
        if (space_after != NULL) {
            char *remainder = strip_whitespace(space_after);
            if (*remainder != '\0') {
                emit_error();
                return -1;
            }
            *space_after = '\0'; // trim trailing junk
        }

        cmd->output_file = target;
    }

    // --- Step 2: tokenise the command part -----------------
    /*
     * Use strtok on the (now possibly shortened) raw_cmd.
     * argv[0] is the program name; subsequent tokens are its
     * arguments.  We stop one short of MAX_TOKENS to guarantee
     * the NULL sentinel fits.
     */
    int  idx   = 0;
    char *word = strtok(raw_cmd, " \t\n");

    while (word != NULL && idx < MAX_TOKENS - 1) {
        cmd->argv[idx++] = word;
        word = strtok(NULL, " \t\n");
    }
    cmd->argv[idx] = NULL;
    cmd->argc      = idx;

    // An empty command is not valid
    if (cmd->argc == 0) {
        // Don't emit an error — blank lines are silently ignored
        return -1;
    }

    return 0;   // success
}