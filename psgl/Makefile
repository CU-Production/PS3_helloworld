#!/usr/bin/make

include MakeDefs

GFX_COMMON=./Common

PPU_INCDIRS		+= -Iinclude 
PPU_CPPFLAGS		+= -DPSGL
PPU_CXXSTDFLAGS		+= -fno-exceptions
PPU_SRCS		:= main.cpp \
			$(GFX_COMMON)/gfxObject.cpp \
			$(GFX_COMMON)/gfxCommon.cpp
			
PPU_TARGET		:= psgl-cube.elf 
PPU_LDLIBS		:= $(GRAPHICS_SAMPLE_CORE_LIBS)

VPSHADER_SOURCES        := shaders/vs_basicSample.cg
FPSHADER_SOURCES        := shaders/fs_basicSample.cg


include MakeRules
