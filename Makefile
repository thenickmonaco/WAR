CC := gcc

DEBUG ?= 0

ifeq ($(DEBUG), 1)
	CFLAGS := -Wall -Wextra -O0 -g -std=c99 -I src -I include
else
	CFLAGS := -Wall -Wextra -O3 -march=native -std=c99 -MMD -DNDEBUG -I src -I include
endif

SRC_DIR := src
BUILD_DIR := build
PRE_DIR := $(BUILD_DIR)/pre
INCLUDE_DIR := include

SRC := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

# Preprocessed files (.i) in build/pre/
IPRE := $(patsubst $(SRC_DIR)/%.c,$(PRE_DIR)/%.i,$(SRC))
# Header files under include/
HDRS := $(patsubst $(SRC_DIR)/%.c,$(INCLUDE_DIR)/%.h,$(SRC))

TARGET := vimDAW

.PHONY: all headers clean

# All target depends on headers and target
all: headers $(TARGET)

# Link object files to create the final target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile each source file into an object file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Generate headers for each source file
headers:
	@echo "Generating individual headers under $(INCLUDE_DIR)"
	$(foreach f,$(SRC), \
		mkdir -p $(dir $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h))); \
		mkdir -p $(dir $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f))); \
		$(CC) -E $(f) -I $(SRC_DIR) -I $(INCLUDE_DIR) -o $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)); \
		cproto -a $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)) > $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h));)
	@echo "Cleaning up .i files"
	@rm -f $(IPRE)  # Remove all the .i files after generating the headers

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(HDRS)

-include $(DEP)

