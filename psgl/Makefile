#!/usr/bin/make

include MakeDefs

GFX_COMMON=./Common

PPU_INCDIRS		+= -Iinclude 
PPU_CPPFLAGS		+= -DPSGL
PPU_CXXSTDFLAGS		+= -fno-exceptions
PPU_SRCS		:= main.cpp
			
PPU_TARGET		:= psgl-triangle.elf 
PPU_LDLIBS		:= $(GRAPHICS_SAMPLE_CORE_LIBS)

VPSHADER_SOURCES        := shaders/triangle_vs.cg
FPSHADER_SOURCES        := shaders/triangle_fs.cg


include MakeRules
