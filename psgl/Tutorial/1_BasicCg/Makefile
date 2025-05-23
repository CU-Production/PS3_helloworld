#!/usr/bin/make
# SCE CONFIDENTIAL
# PlayStation(R)3 Programmer Tool Runtime Library 475.001
#                 Copyright (C) 2009 Sony Computer Entertainment Inc.
#                                                All Rights Reserved.

include ../../MakeDefs

GFX_COMMON=../../Common

PPU_INCDIRS		+= -Iinclude 
PPU_CPPFLAGS		+= -DPSGL
PPU_CXXSTDFLAGS		+= -fno-exceptions
PPU_SRCS		:= main.cpp \
			$(GFX_COMMON)/gfxObject.cpp \
			$(GFX_COMMON)/gfxCommon.cpp \
			$(GFX_COMMON)/gfxPad.cpp
			
PPU_TARGET		:= psglTriangle.elf 
PPU_LDLIBS		:= $(GRAPHICS_SAMPLE_CORE_LIBS)

VPSHADER_SOURCES        := shaders/vs_basicSample.cg
FPSHADER_SOURCES        := shaders/fs_basicSample.cg

include ../../MakeRules
