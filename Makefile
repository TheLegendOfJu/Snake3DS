TARGET      := HomebrewApp

BUILD       := build
SOURCES     := source
INCLUDES    := include

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM ist nicht gesetzt. Bitte lade die devkitPro-Umgebung.")
endif
include $(DEVKITARM)/3ds_rules

ARCH        := -mfloat-abi=hard -mfpu=vfp -mtune=mpcore -mword-relocations
CFLAGS      := -g -Wall -O2 -mword-relocations -fomit-frame-pointer -ffunction-sections $(ARCH) -D__3DS__
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS        := -lcitro2d -lcitro3d -lctru -lm

INCLUDE     := -I$(CURDIR)/$(INCLUDES) -I$(CTRULIB)/include
LIBPATHS    := -L$(CTRULIB)/lib

CPPFILES    := $(wildcard $(SOURCES)/*.cpp)
OFILES      := $(patsubst $(SOURCES)/%.cpp,$(BUILD)/%.o,$(CPPFILES))

.PHONY: all clean

all: $(BUILD) $(TARGET).3dsx

$(BUILD):
	@mkdir -p $(BUILD)

$(TARGET).3dsx: $(TARGET).elf
	@3dsxtool $(TARGET).elf $(TARGET).3dsx

$(TARGET).elf: $(OFILES)
	@$(CXX) -o $@ $^ $(LDFLAGS) $(LIBPATHS) $(LIBS)

$(BUILD)/%.o: $(SOURCES)/%.cpp
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf *.map
