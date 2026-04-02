#!/bin/bash
# ============================================================
# test-wish.sh  —  Test suite for the wish shell
# ============================================================
# Covers every feature required by the lab specification:
#   1. Compilation
#   2. Batch mode basics
#   3. External command execution
#   4. Built-in: exit
#   5. Built-in: cd
#   6. Built-in: path
#   7. Output redirection
#   8. Parallel commands  (&)
#   9. Edge cases
#  10. Cleanup
# ============================================================

# ── Colours ─────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ── Counters ─────────────────────────────────────────────────
PASS=0
FAIL=0

# ── Helpers ──────────────────────────────────────────────────
print_header() {
    echo
    echo -e "${BLUE}${BOLD}╔══════════════════════════════════════╗${NC}"
    printf "${BLUE}${BOLD}║  %-36s║${NC}\n" "$1"
    echo -e "${BLUE}${BOLD}╚══════════════════════════════════════╝${NC}"
}

print_subheader() {
    echo -e "${CYAN}── $1 ──${NC}"
}

pass() {
    echo -e "  ${GREEN}✔  $1${NC}"
    PASS=$((PASS + 1))
}

fail() {
    echo -e "  ${RED}✘  $1${NC}"
    FAIL=$((FAIL + 1))
}

warn() {
    echo -e "  ${YELLOW}⚠  $1${NC}"
}

# run_input <commands_string>
#   Write the string to a temp batch file and run wish on it.
#   Captures both stdout and stderr.
run_input() {
    local tmp
    tmp=$(mktemp /tmp/wish_test_XXXXXX)
    printf '%s\n' "$1" > "$tmp"
    ./wish "$tmp" 2>&1
    local rc=$?
    rm -f "$tmp"
    return $rc
}

# ============================================================
# 1. Compilation
# ============================================================
print_header "1. Compilation"

make clean -s 2>/dev/null
make -s 2>&1
if [ $? -eq 0 ] && [ -x ./wish ]; then
    pass "make succeeded — ./wish is executable"
else
    fail "Compilation failed — aborting test suite"
    exit 1
fi

# ============================================================
# 2. Batch mode basics
# ============================================================
print_header "2. Batch mode"

print_subheader "Simple command executes in batch mode"
OUT=$(run_input "echo hello")
if echo "$OUT" | grep -q "hello"; then
    pass "Output contains 'hello'"
else
    fail "Expected 'hello', got: $OUT"
fi

print_subheader "No prompt printed in batch mode"
OUT=$(run_input "echo hi")
if echo "$OUT" | grep -q "wish>"; then
    fail "Prompt 'wish>' was printed in batch mode"
else
    pass "No 'wish>' prompt in batch mode"
fi

print_subheader "Too many arguments to wish"
ERR=$(./wish a b 2>&1)
RC=$?
if [ $RC -ne 0 ] && echo "$ERR" | grep -qi "error"; then
    pass "Error + non-zero exit for 'wish a b'"
else
    fail "Expected error exit for 'wish a b' (RC=$RC, out='$ERR')"
fi

print_subheader "Non-existent batch file"
ERR=$(./wish /tmp/__no_such_file_wish__ 2>&1)
RC=$?
if [ $RC -ne 0 ] && echo "$ERR" | grep -qi "error"; then
    pass "Error + non-zero exit for missing batch file"
else
    fail "Expected error for missing batch file (RC=$RC)"
fi

print_subheader "EOF exits cleanly"
OUT=$(printf '' | ./wish 2>&1)
RC=$?
if [ $RC -eq 0 ]; then
    pass "EOF causes clean exit (RC=0)"
else
    fail "Unexpected exit code on EOF: $RC"
fi

print_subheader "Blank lines are silently ignored"
OUT=$(run_input "   ")
if [ -z "$OUT" ]; then
    pass "Blank line produces no output"
else
    fail "Blank line produced unexpected output: $OUT"
fi

# ============================================================
# 3. External command execution
# ============================================================
print_header "3. External commands"

print_subheader "ls produces output"
OUT=$(run_input "ls")
if [ -n "$OUT" ]; then
    pass "ls produced output"
else
    fail "ls produced no output"
fi

print_subheader "echo with multiple arguments"
OUT=$(run_input "echo foo bar baz")
if echo "$OUT" | grep -q "foo bar baz"; then
    pass "echo passed arguments correctly"
else
    fail "Expected 'foo bar baz', got: $OUT"
fi

print_subheader "Unknown command emits error"
OUT=$(run_input "nonexistent_cmd_xyz_wish")
if echo "$OUT" | grep -qi "error"; then
    pass "Error emitted for unknown command"
else
    fail "No error for unknown command (got: $OUT)"
fi

print_subheader "Multiple sequential commands on separate lines"
OUT=$(run_input "echo line1
echo line2
echo line3")
LINES=$(echo "$OUT" | grep -c "line")
if [ "$LINES" -eq 3 ]; then
    pass "Three sequential commands all executed"
else
    fail "Expected 3 outputs, got $LINES (out: $OUT)"
fi

# ============================================================
# 4. Built-in: exit
# ============================================================
print_header "4. Built-in: exit"

print_subheader "exit with no args — clean termination"
OUT=$(run_input "exit")
RC=$?
if [ $RC -eq 0 ]; then
    pass "exit returns 0"
else
    fail "exit returned non-zero: $RC"
fi

print_subheader "exit with an argument is an error (shell keeps running)"
OUT=$(run_input "exit foo
echo still_running")
if echo "$OUT" | grep -qi "error" && echo "$OUT" | grep -q "still_running"; then
    pass "Error emitted and shell continued after 'exit foo'"
else
    fail "'exit foo' behaviour wrong (got: $OUT)"
fi

print_subheader "Commands before exit still run"
OUT=$(run_input "echo before_exit
exit")
if echo "$OUT" | grep -q "before_exit"; then
    pass "Command before exit executed"
else
    fail "Command before exit did not run (got: $OUT)"
fi

# ============================================================
# 5. Built-in: cd
# ============================================================
print_header "5. Built-in: cd"

print_subheader "cd /tmp then pwd shows /tmp"
OUT=$(run_input "cd /tmp
pwd")
if echo "$OUT" | grep -q "/tmp"; then
    pass "cd /tmp succeeded"
else
    fail "cd /tmp did not change directory (got: $OUT)"
fi

print_subheader "cd with no argument is an error"
OUT=$(run_input "cd")
if echo "$OUT" | grep -qi "error"; then
    pass "Error emitted for bare 'cd'"
else
    fail "No error for bare 'cd' (got: $OUT)"
fi

print_subheader "cd with two arguments is an error"
OUT=$(run_input "cd /tmp /var")
if echo "$OUT" | grep -qi "error"; then
    pass "Error emitted for 'cd /tmp /var'"
else
    fail "No error for 'cd /tmp /var' (got: $OUT)"
fi

print_subheader "cd to non-existent directory is an error"
OUT=$(run_input "cd /no_such_dir_wish_test_xyz")
if echo "$OUT" | grep -qi "error"; then
    pass "Error emitted for non-existent directory"
else
    fail "No error for bad cd target (got: $OUT)"
fi

print_subheader "Shell continues after failed cd"
OUT=$(run_input "cd /no_such_dir_wish_test_xyz
echo after_bad_cd")
if echo "$OUT" | grep -q "after_bad_cd"; then
    pass "Shell kept running after failed cd"
else
    fail "Shell did not continue after failed cd (got: $OUT)"
fi

# ============================================================
# 6. Built-in: path
# ============================================================
print_header "6. Built-in: path"

print_subheader "Initial path includes /bin (echo works by default)"
OUT=$(run_input "echo default_path")
if echo "$OUT" | grep -q "default_path"; then
    pass "Default path (/bin) works for echo"
else
    fail "echo failed with default path (got: $OUT)"
fi

print_subheader "path /usr/bin — commands in /usr/bin become accessible"
OUT=$(run_input "path /usr/bin
echo via_usr_bin")
if echo "$OUT" | grep -q "via_usr_bin"; then
    pass "echo works after 'path /usr/bin'"
else
    fail "echo failed after 'path /usr/bin' (got: $OUT)"
fi

print_subheader "path (empty) — no external commands can run"
OUT=$(run_input "path
echo should_fail")
if echo "$OUT" | grep -qi "error"; then
    pass "Error emitted when path is empty"
else
    fail "Expected error with empty path, got: $OUT"
fi

print_subheader "path overwrites previous entries (does not append)"
OUT=$(run_input "path /usr/bin
path /bin
echo overwrite_check")
if echo "$OUT" | grep -q "overwrite_check"; then
    pass "path correctly replaced /usr/bin with /bin"
else
    fail "path overwrite broken (got: $OUT)"
fi

print_subheader "path with multiple directories"
OUT=$(run_input "path /bin /usr/bin
echo multi_path")
if echo "$OUT" | grep -q "multi_path"; then
    pass "Multiple directories in path work"
else
    fail "Multi-dir path failed (got: $OUT)"
fi

# ============================================================
# 7. Output redirection
# ============================================================
print_header "7. Output redirection"

TMP_REDIR="/tmp/wish_redir_$$"

print_subheader "Redirect output to a file"
run_input "echo redirected > $TMP_REDIR" > /dev/null 2>&1
if grep -q "redirected" "$TMP_REDIR" 2>/dev/null; then
    pass "Output written to file"
else
    fail "Redirect: file missing or wrong content"
fi
rm -f "$TMP_REDIR"

print_subheader "Redirect truncates existing file content"
echo "old_content" > "$TMP_REDIR"
run_input "echo new_content > $TMP_REDIR" > /dev/null 2>&1
CONTENT=$(cat "$TMP_REDIR" 2>/dev/null)
if echo "$CONTENT" | grep -q "new_content" && ! echo "$CONTENT" | grep -q "old_content"; then
    pass "Redirect truncated existing file"
else
    fail "Redirect did not truncate (content: $CONTENT)"
fi
rm -f "$TMP_REDIR"

print_subheader "Nothing printed to terminal stdout when redirecting"
OUT=$(run_input "echo silent > $TMP_REDIR")
rm -f "$TMP_REDIR"
if [ -z "$OUT" ]; then
    pass "No output on terminal stdout during redirect"
else
    fail "Unexpected terminal output during redirect: $OUT"
fi

print_subheader "Redirect without spaces around > (echo hi>file)"
run_input "echo nospace>$TMP_REDIR" > /dev/null 2>&1
if grep -q "nospace" "$TMP_REDIR" 2>/dev/null; then
    pass "Redirect without spaces works"
else
    fail "Redirect without spaces failed"
fi
rm -f "$TMP_REDIR"

print_subheader "Multiple filenames after > is an error"
OUT=$(run_input "echo x > a_wish_$$.txt b_wish_$$.txt")
rm -f "a_wish_$$.txt" "b_wish_$$.txt"
if echo "$OUT" | grep -qi "error"; then
    pass "Error for multiple redirect targets"
else
    fail "No error for multiple redirect targets (got: $OUT)"
fi

print_subheader "No filename after > is an error"
OUT=$(run_input "echo x >")
if echo "$OUT" | grep -qi "error"; then
    pass "Error for missing redirect filename"
else
    fail "No error for missing redirect filename (got: $OUT)"
fi

print_subheader "Two > operators in one command is an error"
OUT=$(run_input "echo x > a_wish_$$.txt > b_wish_$$.txt")
rm -f "a_wish_$$.txt" "b_wish_$$.txt"
if echo "$OUT" | grep -qi "error"; then
    pass "Error for double redirection"
else
    fail "No error for double redirection (got: $OUT)"
fi

# ============================================================
# 8. Parallel commands (&)
# ============================================================
print_header "8. Parallel commands (&)"

REDIR_A="/tmp/wish_par_a_$$"
REDIR_B="/tmp/wish_par_b_$$"

print_subheader "Two parallel commands both produce output"
run_input "echo alpha > $REDIR_A & echo beta > $REDIR_B" > /dev/null 2>&1
A_OK=0; B_OK=0
grep -q "alpha" "$REDIR_A" 2>/dev/null && A_OK=1
grep -q "beta"  "$REDIR_B" 2>/dev/null && B_OK=1
if [ $A_OK -eq 1 ] && [ $B_OK -eq 1 ]; then
    pass "Both parallel commands wrote their output"
else
    fail "Parallel output missing (alpha_file=$A_OK beta_file=$B_OK)"
fi
rm -f "$REDIR_A" "$REDIR_B"

print_subheader "Shell waits for all parallel children before next line"
REDIR_C="/tmp/wish_par_c_$$"
OUT=$(run_input "echo parallel_done > $REDIR_C & echo also_done >> $REDIR_C
echo next_line")
NEXT=$(echo "$OUT" | grep "next_line")
FILE_LINES=$(wc -l < "$REDIR_C" 2>/dev/null || echo 0)
rm -f "$REDIR_C"
if [ -n "$NEXT" ] && [ "$FILE_LINES" -ge 1 ]; then
    pass "Next line ran after all parallel children finished"
else
    fail "Parallel wait issue (next_line='$NEXT' file_lines=$FILE_LINES)"
fi

print_subheader "& without surrounding spaces (echo a&echo b)"
OUT=$(run_input "echo a_par&echo b_par")
if echo "$OUT" | grep -q "a_par" && echo "$OUT" | grep -q "b_par"; then
    pass "& without spaces parsed correctly"
else
    fail "& without spaces failed (got: $OUT)"
fi

print_subheader "One bad command in parallel does not block the good one"
REDIR_D="/tmp/wish_par_d_$$"
OUT=$(run_input "echo good_cmd > $REDIR_D & nonexistent_cmd_xyz_wish")
GOOD=$(grep -c "good_cmd" "$REDIR_D" 2>/dev/null || echo 0)
ERR_COUNT=$(echo "$OUT" | grep -ci "error" || echo 0)
rm -f "$REDIR_D"
if [ "$GOOD" -ge 1 ] && [ "$ERR_COUNT" -ge 1 ]; then
    pass "Good command ran and bad command emitted error in parallel"
else
    fail "Parallel mix failed (good=$GOOD errors=$ERR_COUNT)"
fi

# ============================================================
# 9. Edge cases
# ============================================================
print_header "9. Edge cases"

print_subheader "Extra whitespace around command and args"
OUT=$(run_input "   echo   spaced   test   ")
if echo "$OUT" | grep -q "spaced"; then
    pass "Extra whitespace handled correctly"
else
    fail "Extra whitespace broke parsing (got: $OUT)"
fi

print_subheader "Tabs between tokens"
OUT=$(run_input "$(printf 'echo\ttabbed\ttest')")
if echo "$OUT" | grep -q "tabbed"; then
    pass "Tabs between tokens handled correctly"
else
    fail "Tab parsing failed (got: $OUT)"
fi

print_subheader "cd then run command in new directory"
TMP_DIR=$(mktemp -d /tmp/wish_dir_test_XXXXXX)
OUT=$(run_input "cd $TMP_DIR
pwd")
rmdir "$TMP_DIR"
if echo "$OUT" | grep -q "$TMP_DIR"; then
    pass "cd then pwd reflects new directory"
else
    fail "cd + pwd mismatch (got: $OUT)"
fi

print_subheader "path set, then cleared, then reset — full cycle"
OUT=$(run_input "path /bin
path
path /bin
echo cycle_ok")
if echo "$OUT" | grep -q "cycle_ok"; then
    pass "Full path set/clear/reset cycle works"
else
    fail "Path cycle failed (got: $OUT)"
fi

# ============================================================
# 10. Cleanup
# ============================================================
print_header "10. Cleanup"

make clean -s 2>/dev/null
if [ ! -f ./wish ]; then
    pass "Binary removed by make clean"
else
    fail "make clean did not remove ./wish"
fi

# ============================================================
# Summary
# ============================================================
TOTAL=$((PASS + FAIL))
echo
echo -e "${BOLD}════════════════════════════════════════${NC}"
echo -e "${BOLD}  Results: ${GREEN}${PASS} passed${NC} / ${RED}${FAIL} failed${NC} / ${TOTAL} total${NC}"
echo -e "${BOLD}════════════════════════════════════════${NC}"
if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}${BOLD}  All tests passed! 🎉${NC}"
    echo
    exit 0
else
    echo -e "${RED}${BOLD}  Some tests failed — review the output above.${NC}"
    echo
    exit $FAIL
fi