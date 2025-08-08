CC := gcc
DEBUG ?= 0
VERBOSE ?= 0

WL_SHM ?= 0
DMABUF ?= 0

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
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -g -march=native -std=c99 -MMD -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2
else ifeq ($(DEBUG), 2)
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O0 -g -march=native -std=c99 -MMD -DDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2
else
	CFLAGS := -D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99 -MMD -DNDEBUG -I src -I include -I /usr/include/libdrm -I /usr/include/freetype2
endif

CFLAGS += -DWL_SHM=$(WL_SHM)
CFLAGS += -DDMABUF=$(DMABUF)

LDFLAGS := -lvulkan -ldrm -lm -lluajit-5.1 -lxkbcommon -lasound -lpthread -lfreetype

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
QUAD_NDC_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_ndc_vertex.glsl
QUAD_NDC_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_quad_ndc_fragment.glsl
QUAD_NDC_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_ndc_vertex.spv
QUAD_NDC_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_quad_ndc_fragment.spv
SDF_VERT_SHADER_SRC := $(SHADER_SRC_DIR)/war_sdf_vertex.glsl
SDF_FRAG_SHADER_SRC := $(SHADER_SRC_DIR)/war_sdf_fragment.glsl
SDF_VERT_SHADER_SPV := $(SHADER_BUILD_DIR)/war_sdf_vertex.spv
SDF_FRAG_SHADER_SPV := $(SHADER_BUILD_DIR)/war_sdf_fragment.spv

SRC := $(shell find $(SRC_DIR) -type f -name '*.c')

UNITY_C := $(SRC_DIR)/war_main.c
UNITY_O := $(BUILD_DIR)/war_main.o
DEP := $(UNITY_O:.o=.d)

.PHONY: all clean gcc_check

all: $(QUAD_VERT_SHADER_SPV) $(QUAD_FRAG_SHADER_SPV) $(QUAD_NDC_VERT_SHADER_SPV) $(QUAD_NDC_FRAG_SHADER_SPV) $(SDF_VERT_SHADER_SPV) $(SDF_FRAG_SHADER_SPV) $(TARGET)

$(SHADER_BUILD_DIR):
	mkdir -p $(SHADER_BUILD_DIR)

$(QUAD_VERT_SHADER_SPV): $(QUAD_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(QUAD_FRAG_SHADER_SPV): $(QUAD_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@

$(QUAD_NDC_VERT_SHADER_SPV): $(QUAD_NDC_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(QUAD_NDC_FRAG_SHADER_SPV): $(QUAD_NDC_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@

$(SDF_VERT_SHADER_SPV): $(SDF_VERT_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S vert $< -o $@

$(SDF_FRAG_SHADER_SPV): $(SDF_FRAG_SHADER_SRC) | $(SHADER_BUILD_DIR)
	$(Q)$(GLSLC) -V -S frag $< -o $@

$(UNITY_O): $(UNITY_C)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $(UNITY_C) -o $@

$(TARGET): $(UNITY_O) $(QUAD_VERT_SHADER_SPV) $(QUAD_FRAG_SHADER_SPV) $(QUAD_NDC_VERT_SHADER_SPV) $(QUAD_NDC_FRAG_SHADER_SPV) $(SDF_FRAG_SHADER_SPV) $(SDF_VERT_SHADER_SPV)
	$(Q)$(CC) $(CFLAGS) -o $@ $(UNITY_O) $(LDFLAGS)

clean:
	$(Q)rm -rf $(BUILD_DIR) $(TARGET)

gcc_check:
	$(Q)$(CC) $(CFLAGS) -fsyntax-only $(SRC)

-include $(DEP)

