CC := gcc
DEBUG ?= 0
VERBOSE ?= 0

WL_SHM ?= 0
DMABUF ?= 0

PIPEWIRE_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3)
PIPEWIRE_LIBS   := $(shell pkg-config --libs libpipewire-0.3)

ifeq ($(WL_SHM),1)
	DMABUF := 0
else ifeq ($(DMABUF),1)
	WL_SHM := 0
else
	DMABUF := 1
endif

ifeq ($(VERBOSE), 1)
    Q :=
else
    Q := @
    MAKEFLAGS += --no-print-directory
endif

ifeq ($(DEBUG), 1)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -g -march=native -std=c99 -MMD -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS)
else ifeq ($(DEBUG), 2)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O0 -g -march=native -std=c99 -MMD -DDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS) 
else
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99 -MMD -DNDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2 $(PIPEWIRE_CFLAGS)
endif

CFLAGS += -DWL_SHM=$(WL_SHM)
CFLAGS += -DDMABUF=$(DMABUF)

LDFLAGS := -lvulkan -ldrm -lm -lluajit-5.1 -lxkbcommon -lasound -lpthread -lfreetype $(PIPEWIRE_LIBS)

SRC_DIR := src
BUILD_DIR := build
TARGET := WAR

GLSLC := glslangValidator
SHADER_SRC_DIR := src/shaders
SHADER_BUILD_DIR := $(BUILD_DIR)/shaders
QUAD_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_vertex.glsl
QUAD_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_fragment.glsl
QUAD_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_vertex.spv
QUAD_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_fragment.spv
TEXT_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_text_vertex.glsl
TEXT_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_text_fragment.glsl
TEXT_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_text_vertex.spv
TEXT_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_text_fragment.spv

SRC := $(shell find $(SRC_DIR) -type f -name '*.c')

UNITY_C := $(SRC_DIR)/war_main.c
UNITY_O := $(BUILD_DIR)/war_main.o
DEP := $(UNITY_O:.o=.d)

.PHONY: all clean gcc_check war_keymap_macros # libwar

# LIBWAR := $(BUILD_DIR)/libwar.so
# FFI := $(BUILD_DIR)/fsm_ffi.lua

KEYMAP_MACROS_H := $(BUILD_DIR)/war_keymap_macros.h
GEN_KEYMAP_MACROS_H := $(SRC_DIR)/lua/war_get_keymap_macros.lua
WAR_KEYMAP_FUNCTIONS_H := $(SRC_DIR)/h/war_keymap_functions.h

all: $(KEYMAP_MACROS_H) $(QUAD_VERT_SHADER_SPV) $(QUAD_FRAG_SHADER_SPV) $(TEXT_VERT_SHADER_SPV) $(TEXT_FRAG_SHADER_SPV) $(TARGET)

keymap_macros_h: $(KEYMAP_MACROS_H)

# libwar: $(LIBWAR)

$(SHADER_BUILD_DIR):
	mkdir -p $(SHADER_BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(QUAD_VERT_SHADER_SPV): $(QUAD_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(QUAD_FRAG_SHADER_SPV): $(QUAD_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@

$(TEXT_VERT_SHADER_SPV): $(TEXT_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(TEXT_FRAG_SHADER_SPV): $(TEXT_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@

$(UNITY_O): $(UNITY_C) $(KEYMAP_MACROS_H)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $(UNITY_C) -o $@

# $(LIBWAR) $(FFI): src/lua/get_libwar.lua src/h/war_data.h $(UNITY_C)
#	$(Q)lua src/lua/get_libwar.lua

$(KEYMAP_MACROS_H): $(GEN_KEYMAP_MACROS_H) $(WAR_KEYMAP_FUNCTIONS_H) | $(BUILD_DIR)
	$(Q)lua $(GEN_KEYMAP_MACROS_H)

$(TARGET): $(UNITY_O) $(QUAD_VERT_SHADER_SPV) $(QUAD_FRAG_SHADER_SPV) $(TEXT_VERT_SHADER_SPV) $(TEXT_FRAG_SHADER_SPV)
	$(Q)$(CC) $(CFLAGS) -o $@ $(UNITY_O) $(LDFLAGS)

clean:
	$(Q)rm -rf $(BUILD_DIR) $(TARGET) $(KEYMAP_MACROS_H)

gcc_check:
	$(Q)$(CC) $(CFLAGS) -fsyntax-only $(SRC)

-include $(DEP)

