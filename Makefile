# =============================================================
# Makefile  —  wish shell
# =============================================================
# Targets:
#   make          →  compile everything, produce ./wish
#   make clean    →  remove compiled objects and the binary
#   make run      →  launch wish in interactive mode
#
# The binary is placed in the project root (one level above
# src/) so the test script can find it with ./wish.
# =============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
SRC_DIR = src

# Source files (order does not affect linking)
SRCS =  $(SRC_DIR)/wish.c     \
        $(SRC_DIR)/parser.c   \
        $(SRC_DIR)/builtins.c \
        $(SRC_DIR)/executor.c

# Object files alongside their sources
OBJS = $(SRCS:.c=.o)

# Final executable in the project root
TARGET = wish

# -----------------------------------------------------------
# Default target
# -----------------------------------------------------------
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Pattern rule: compile each .c → .o
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/shell.h
	$(CC) $(CFLAGS) -c -o $@ $<

# -----------------------------------------------------------
# Convenience targets
# -----------------------------------------------------------
.PHONY: run
run: all
	./$(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)