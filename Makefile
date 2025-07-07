CC := gcc
DEBUG ?= 0
VERBOSE ?= 0

WL_SHM ?= 0
DMABUF ?= 0

ifeq ($(DMABUF),0)
ifeq ($(WL_SHM),0)
	DMABUF := 1
endif
endif

ifeq ($(VERBOSE), 1)
    Q :=
else
    Q := @
    MAKEFLAGS += --no-print-directory
endif

ifeq ($(DEBUG), 1)
	# Debug build: asserts, debug symbols, and no DEBUG preprocessor
    CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -g -march=native -std=c99 -MMD -I src -I include
else ifeq ($(DEBUG), 2)
	# Debug verbose build: asserts, debug symbols and DEBUG preprocessor
    CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -g -march=native -std=c99 -MMD -DDEBUG -I src -I include
else
	# Release build: no asserts, no debug symbols
    CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99 -MMD -DNDEBUG -I src -I include
endif

CFLAGS += -DWL_SHM=$(WL_SHM)
CFLAGS += -DDMABUF=$(DMABUF)

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

.PHONY: all headers clean guard empty_headers gcc_check

all: empty_headers headers $(TARGET)

# Create empty header placeholders if they don't exist
empty_headers:
ifeq ($(VERBOSE),1)
	$(Q)echo "Ensuring header placeholders exist..."
endif
	$(Q)$(foreach hdr,$(HDRS), \
		if [ ! -f $(hdr) ]; then \
			mkdir -p $$(dirname $(hdr)); \
			echo "/* Placeholder for $${hdr} */" > $(hdr); \
		fi; \
	)

# Compile unity build main.c
$(UNITY_O): headers
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $(UNITY_C) -o $@

# Link final binary
$(TARGET): $(UNITY_O)
	$(Q)$(CC) $(CFLAGS) -o $@ $^

# Generate headers from all .c files using cproto
headers:
ifeq ($(VERBOSE),1)
	$(Q)echo "Generating headers in $(INCLUDE_DIR)..."
endif
	$(Q)$(foreach f,$(SRC), \
		mkdir -p $(dir $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h))); \
		mkdir -p $(dir $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f))); \
		$(CC) -E -w $(f) -I $(SRC_DIR) -I $(INCLUDE_DIR) -o $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)) 2>/dev/null; \
		cproto -a $(patsubst $(SRC_DIR)/%,$(PRE_DIR)/%,$(f:.c=.i)) > $(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h)) 2>/dev/null; \
		$(MAKE) guard HEADER=$(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h)) VERBOSE=$(VERBOSE); \
		$(MAKE) prepend_std_headers HEADER=$(INCLUDE_DIR)/$(patsubst $(SRC_DIR)/%,%,$(f:.c=.h)); \
	)

# Add include guards with format VIMDAW_FILENAME_H
guard:
	$(Q)header=$(HEADER); \
	filename=$$(basename "$${header}"); \
	filename_no_ext=$${filename%.*}; \
	guard_macro=$$(echo "VIMDAW_$${filename_no_ext}_H" | tr 'a-z' 'A-Z'); \
	tmpfile="$${header}.tmp"; \
	echo "#ifndef $${guard_macro}" > "$${tmpfile}"; \
	echo "#define $${guard_macro}" >> "$${tmpfile}"; \
	cat "$${header}" >> "$${tmpfile}"; \
	echo "#endif /* $${guard_macro} */" >> "$${tmpfile}"; \
	mv "$${tmpfile}" "$${header}"

# Prepend standard includes to generated headers
prepend_std_headers:
	$(Q)header=$(HEADER); \
	tmpfile="$${header}.tmp2"; \
	echo "#include <stdint.h>" > "$${tmpfile}"; \
	echo "#include <stddef.h>" >> "$${tmpfile}"; \
	echo "#include <stdbool.h>" >> "$${tmpfile}"; \
	echo "#include <stdio.h>" >> "$${tmpfile}"; \
	cat "$${header}" >> "$${tmpfile}"; \
	mv "$${tmpfile}" "$${header}"

# Clean everything
clean:
	$(Q)rm -rf $(BUILD_DIR) $(TARGET) $(HDRS)

# Syntax check only
gcc_check:
	$(Q)$(CC) $(CFLAGS) -fsyntax-only $(SRC)

-include $(DEP)

