#---------------------------------------------------------------------------------
Makefile fuer 3DS Homebrew (Snake3DS)
#---------------------------------------------------------------------------------
ifeq ($(strip (DEVKITPRO)),)(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitpro")
endif
include $(DEVKITPRO)/3ds_rules
TARGET      := Snake3DS
OBJS        := main.o
CFLAGS      := -Wall -O2 -mword-relocations -fomit-frame-pointer -ffast-math -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -DARM11 -D_3DS
CXXFLAGS    := $(CFLAGS) -std=gnu++17
LDFLAGS     := -specs=3dsx.specs -g -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
LIBS        := -lctru -lm
all: $(TARGET).3dsx
(TARGET).3dsx:(TARGET).elf
(TARGET).elf:(OBJS)
clean:
rm -f $(OBJS) $(TARGET).elf $(TARGET).3dsx
