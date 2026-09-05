#----------------------------------------------------------------------------------------
# 3DS Homebrew Makefile Template
#----------------------------------------------------------------------------------------

# 1. Project Configuration
TARGET      := $(notdir $(CURDIR))
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include

# 2. Application Info (Shown in the Homebrew Launcher)
APP_TITLE   := My 3DS Homebrew Application
APP_AUTHOR  := GitHub Actions Developer
APP_VERSION := 1.0.0
APP_DESC    := Compiled automatically via CI/CD.
APP_ICON    := $(DEVKITPRO)/libctru/default_icon.png

#----------------------------------------------------------------------------------------
# Environment Validation
#----------------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

#----------------------------------------------------------------------------------------
# Compiler and Linker Flags
#----------------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-fomit-frame-pointer -ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++20

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $basename $@).map

# 3. Libraries (Order matters! Dependent libraries must come first)
LIBS	:=	-lcitro3d -lctru -lm

#----------------------------------------------------------------------------------------
# Directory Setup & File Tracking
#----------------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES	:=	$(BINFILES:.bin=.o) $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD) \
					-I$(DEVKITPRO)/libctru/include \
					-I$(DEVKITPRO)/citro3d/include

.PHONY: clean all

#----------------------------------------------------------------------------------------
# Build Rules
#----------------------------------------------------------------------------------------
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

#----------------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh

$(OUTPUT).elf	:	$(OFILES)

$(OUTPUT).smdh	:	$(APP_ICON)

%.o	:	%.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@ $(strifq)

%.o	:	%.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(strifq)

-include $(DEPENDS)

endif
#----------------------------------------------------------------------------------------
