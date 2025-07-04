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
TARGET := vimDAW

SRC := $(shell find $(SRC_DIR) -type f -name '*.c')
HDRS := $(patsubst $(SRC_DIR)/%.c,$(INCLUDE_DIR)/%.h,$(SRC))

UNITY_C := $(SRC_DIR)/main.c
UNITY_O := $(BUILD_DIR)/main.o
DEP := $(UNITY_O:.o=.d)

.PHONY: all headers clean guard empty_headers

all: empty_headers headers $(TARGET)

# Create empty header placeholders if they don't exist
empty_headers:
	@echo "Ensuring header placeholders exist..."
	@$(foreach hdr,$(HDRS), \
		if [ ! -f $(hdr) ]; then \
			mkdir -p $$(dirname $(hdr)); \
			echo "/* Placeholder for $${hdr} */" > $(hdr); \
		fi; \
	)

# Compile unity build main.c (after headers generated)
$(UNITY_O): headers
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(UNITY_C) -o $@

# Link final executable from unity.o only
$(TARGET): $(UNITY_O)
	$(CC) $(CFLAGS) -o $@ $^

# Generate headers from all .c files using cproto
headers:
	@echo "Generating headers in $(INCLUDE_DIR)..."
	$(foreach f,$(SRC), \
		mkdir -p $(dir $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h))); \
		mkdir -p $(dir $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f))); \
		$(CC) -E $(f) -I $(SRC_DIR) -I $(INCLUDE_DIR) -o $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)); \
		cproto -a $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)) > $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h)); \
		$(MAKE) guard HEADER=$(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h)); \
	)

# Add include guards with format VIMDAW_FILENAME_H
guard:
	@header=$(HEADER); \
	filename=$$(basename "$${header}"); \
	filename_no_ext=$${filename%.*}; \
	guard_macro=$$(echo "VIMDAW_$${filename_no_ext}_H" | tr 'a-z' 'A-Z'); \
	tmpfile="$${header}.tmp"; \
	echo "#ifndef $${guard_macro}" > "$${tmpfile}"; \
	echo "#define $${guard_macro}" >> "$${tmpfile}"; \
	cat "$${header}" >> "$${tmpfile}"; \
	echo "#endif /* $${guard_macro} */" >> "$${tmpfile}"; \
	mv "$${tmpfile}" "$${header}"

# Clean all build artifacts and generated headers
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(HDRS)

-include $(DEP)

