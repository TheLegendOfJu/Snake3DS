#---------------------------------------------------------------------------------
3DS Homebrew Makefile (Snake3DS)
#---------------------------------------------------------------------------------
ifeq ($(strip ￼(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitpro")
endif
include $(DEVKITPRO)/3ds_rules
TARGET      := Snake3DS
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include
ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 -mword-relocations 
-fomit-frame-pointer -ffunction-sections 
$(ARCH) -DARM11 -D_3DS
CXXFLAGS    := $(CFLAGS) -std=gnu++17
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g ￼(notdir ￼@)).map
LIBS        := -lctru -lm
ifneq (￼(notdir$(CURDIR)))
export OUTPUT   := ￼(TARGET)
export VPATH    := ￼(SOURCES),￼(dir))
export DEPSDIR  := ￼(BUILD)
CFILES      := ￼(SOURCES),￼
￼(notdir ￼(dir)/.cpp)))
sFILES      := ￼(SOURCES),$(notdir ￼(dir)/.s)))
SFILES      := ￼(SOURCES),$(notdir ￼(dir)/*.S)))
export OFILES := ￼(CFILES:.c=.o) ￼(SFILES:.S=.o)
export INCLUDE := ￼(INCLUDES),-I$(CURDIR)/(dir)) \
￼(CURDIR)/(dir)) \
￼(CURDIR)/$(BUILD)
.PHONY: clean all
all: ￼
￼(MAKE) --no-print-directory -C ￼(CURDIR)/Makefile
$(BUILD):
@mkdir -p $@
clean:
@echo clean ...
@rm -rf ￼(TARGET).3dsx $(TARGET).elf $(TARGET).smdh
else
DEPENDS := $(OFILES:.o=.d)
￼(OUTPUT).elf
￼(OFILES)
%.o: %.cpp
@echo ￼<)
@￼(DEPSDIR)/$*.d $(CXXFLAGS) -c ￼@
%.o: %.c
@echo ￼<)
@￼(DEPSDIR)/$*.d $(CFLAGS) -c ￼@
-include $(DEPENDS)
endif
