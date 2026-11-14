CC = gcc
CFLAGS = -g -Wall -Werror

.PHONY: all clean

SRC_DIR := src
BUILD_DIR := build

SRCS = $(wildcard $(SRC_DIR)/*.c)
TARGETS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.x, $(SRCS))

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.x: $(BUILD_DIR)/%.o | $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*.o build/*.x

