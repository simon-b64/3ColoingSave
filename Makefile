#/**
# * @file Makefile
# * @author Simon Buchinger 12220026 <e12220026@student.tuwien.ac.at>
# * @date 27.11.2023
# * @program: 3coloring
# *
# **/

CC := gcc
CFLAGS := -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g

TARGET_EXEC := 3coloring

BUILD_DIR := ./
SRC_DIRS := ./

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
INC_DIRS := $(shell find $(SRC_DIRS) -type d)

# Maybe we need this
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

.PHONY: all
all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: run
run: all
	$(BUILD_DIR)/$(TARGET_EXEC)

.PHONY: clean
clean:
	rm -rf $(TARGET_EXEC)
	rm -rf $(BUILD_DIR)/*.o
