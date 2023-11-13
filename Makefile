TARGET = neo

SRC_DIR = src/
INC_DIR = include/
BUILD_DIR = build/

SRCS = $(wildcard $(SRC_DIR)*.c)
OBJS = $(patsubst $(SRC_DIR)%.c,$(BUILD_DIR)%.o,$(SRCS))
DEPS = $(patsubst $(BUILD_DIR)%.o,$(BUILD_DIR)%.d,$(OBJS))

CFLAGS  = -Wall -Werror -MMD -std=c99 -I$(INC_DIR)
LDFLAGS =

all: $(TARGET)

debug: CFLAGS += -DDEBUG -g
debug: CFLAGS := $(filter-out -Werror, $(CFLAGS))
debug: all

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $(BUILD_DIR)$@ $(LDFLAGS)

$(BUILD_DIR)%.o: $(SRC_DIR)%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

-include $(DEP)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
