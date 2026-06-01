##
## ifft-style CK803 project makefile for RRISE
##

ProjectName := %{APPNAMELC}
IntermediateDirectory := Obj
ListDirectory := Lst
OutputFile := $(ProjectName)
ObjectsFileList := %{APPNAMELC}.txt

ifndef RRISE_TOOLKIT_ROOT
$(error RRISE_TOOLKIT_ROOT is not set)
endif

SUPPORT_ROOT := $(RRISE_TOOLKIT_ROOT)/verify/ck803_sample
IFFT_SUPPORT_ROOT := $(RRISE_TOOLKIT_ROOT)/verify/ifft_project

CC := csky-elfabiv2-gcc
AS := csky-elfabiv2-gcc
AR := csky-elfabiv2-ar
OBJDUMP := csky-elfabiv2-objdump
OBJCOPY := csky-elfabiv2-objcopy
SIZE := csky-elfabiv2-size

CFLAGS := -mcpu=e803 -O0 -g -ffunction-sections -fdata-sections
ASFLAGS := -mcpu=e803 -Wa,--gdwarf2
LDFLAGS := -nostartfiles -Wl,--gc-sections -Tlnk/linker.ld -Wl,-zmax-page-size=1024
INCLUDES := -I. -Iinclude -Ilnk -Isrc \
	-I$(SUPPORT_ROOT)/board/smartl_803_evb/include \
	-I$(SUPPORT_ROOT)/csi_core/include \
	-I$(SUPPORT_ROOT)/csi_driver/include \
	-I$(SUPPORT_ROOT)/csi_driver/smartl/include \
	-I$(SUPPORT_ROOT)/libs/include \
	-I$(IFFT_SUPPORT_ROOT)/include
LIBPATHS := -L$(IFFT_SUPPORT_ROOT)
LIBS := -lth01A

OBJECTS := \
	$(IntermediateDirectory)/lnk_crt0.o \
	$(IntermediateDirectory)/__rt_entry.o \
	$(IntermediateDirectory)/%{APPNAMELC}_main.o

.PHONY: all clean

all: $(IntermediateDirectory)/$(OutputFile).elf $(IntermediateDirectory)/$(OutputFile).ihex $(ListDirectory)/$(OutputFile).asm
	@echo size of target:
	@$(SIZE) $(IntermediateDirectory)/$(OutputFile).elf
	@echo -n checksum value of target:
	@cmd /c "%{APPNAMELC}.modify.bat $(IntermediateDirectory) $(OutputFile).elf"

$(IntermediateDirectory)/$(OutputFile).elf: $(OBJECTS) %{APPNAMELC}.txt
	$(CC) -o $@ $(LDFLAGS) @$(ObjectsFileList) $(LIBPATHS) $(LIBS)

$(IntermediateDirectory)/$(OutputFile).ihex: $(IntermediateDirectory)/$(OutputFile).elf
	$(OBJCOPY) -O ihex $< $@

$(ListDirectory)/$(OutputFile).asm: $(IntermediateDirectory)/$(OutputFile).elf
	$(OBJDUMP) -S $< > $@

$(IntermediateDirectory)/lnk_crt0.o: lnk/crt0.S | $(IntermediateDirectory)
	$(AS) -c $< $(ASFLAGS) $(INCLUDES) -o $@

$(IntermediateDirectory)/__rt_entry.o: __rt_entry.S | $(IntermediateDirectory)
	$(AS) -c $< $(ASFLAGS) $(INCLUDES) -o $@

$(IntermediateDirectory)/%{APPNAMELC}_main.o: src/TC_adda_%{APPNAMELC}/c/main.c | $(IntermediateDirectory)
	$(CC) -c $< $(CFLAGS) $(INCLUDES) -o $@

$(IntermediateDirectory):
	mkdir -p $(IntermediateDirectory)

$(ListDirectory):
	mkdir -p $(ListDirectory)

clean:
	rm -rf $(IntermediateDirectory) $(ListDirectory)
