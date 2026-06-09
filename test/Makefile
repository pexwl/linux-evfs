CC = gcc
CFLAGS = -g -Wall -Werror

.PHONY: all clean

SRC_DIR := src
BUILD_DIR := build

NAMES = hello iver balloc bfree ialloc ifree iread\
	dentry_add dentry_read dentry_update dentry_delete\
	extent_move.n extent_move.c

TARGETS = $(addprefix $(BUILD_DIR)/, $(NAMES:=.x))

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.x: $(BUILD_DIR)/%.o | $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*.o build/*.x

