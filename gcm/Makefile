#   SCE CONFIDENTIAL                                      
#   PlayStation(R)3 Programmer Tool Runtime Library 475.001
#   Copyright (C) 2006 Sony Computer Entertainment Inc.   
#   All Rights Reserved.                                  

CELL_SDK ?= /usr/local/cell
CELL_MK_DIR ?= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

PPU_SRCS		=	main.cpp $(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS)
PPU_TARGET		=	gcm-triangle.elf

GCC_PPU_CXXFLAGS += -fno-exceptions -fno-rtti
PPU_LIBS		+=	$(CELL_TARGET_PATH)/ppu/lib/libgcm_cmd.a \
					$(CELL_TARGET_PATH)/ppu/lib/libgcm_sys_stub.a 
PPU_LDLIBS		+= 	-lsysmodule_stub  -lm -lsysutil_stub
PPU_LDFLAGS		+=	-Wl,--strip-unused

VPSHADER_SRCS	=	vpshader.cg
FPSHADER_SRCS	=	fpshader.cg

VPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(VPSHADER_SRCS))
FPSHADER_PPU_OBJS = $(patsubst %.cg, $(OBJS_DIR)/%.ppu.o, $(FPSHADER_SRCS))

include $(CELL_MK_DIR)/sdk.target.mk

PPU_OBJS += $(VPSHADER_PPU_OBJS) $(FPSHADER_PPU_OBJS)

$(VPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.vpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

$(FPSHADER_PPU_OBJS): $(OBJS_DIR)/%.ppu.o : %.fpo
	@mkdir -p $(dir $(@))
	$(PPU_OBJCOPY)  -I binary -O elf64-powerpc-celloslv2 -B powerpc $< $@

clean-local: ppu-clean ppu-prx-clean spu-clean cg-clean $(MK_TARGET_CLEAN) user-clean clean-objs
