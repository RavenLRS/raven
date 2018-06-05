PROJECT_NAME := raven
export PROJECT_NAME

PORT ?=

# Version number and git revision of the build
VERSION_MAJOR := 0
VERSION_MINOR := 9
VERSION_PATCH := 0
VERSION_SUFFIX :=

VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)$(VERSION_SUFFIX)

GIT_REVISION := $(shell git rev-parse --short HEAD)

CPPFLAGS += -DGIT_REVISION=\"$(GIT_REVISION)\" -DVERSION=\"$(VERSION)\"

# Platform/targets setup
ROOT           			:= $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
export ROOT
TARGET_DIR				:= $(ROOT)/main/target
PLATFORMS_DIR 			= $(TARGET_DIR)/platforms
export PLATFORMS_DIR
VALID_PLATFORMS 		= $(dir $(wildcard $(PLATFORMS_DIR)/*/sdkconfig))
VALID_PLATFORMS			:= $(subst /,,$(subst $(PLATFORMS_DIR)/,,$(VALID_PLATFORMS)))
VALID_PLATFORMS			:= $(sort $(VALID_PLATFORMS))

VARIANTS_DIR 			= $(TARGET_DIR)/variants
export VARIANTS_DIR
VALID_VARIANTS	 		= $(dir $(wildcard $(VARIANTS_DIR)/*/sdkconfig))
VALID_VARIANTS			:= $(subst /,,$(subst $(VARIANTS_DIR)/,,$(VALID_VARIANTS)))
VALID_VARIANTS			:= $(sort $(VALID_VARIANTS))

make_target				= $(platform)_$(variant)
VALID_TARGETS			:= $(foreach platform,$(VALID_PLATFORMS),$(foreach variant,$(VALID_VARIANTS),$(make_target)))

# Features that we accept via environment

USE_AIR_MODE_1 ?=
ifneq ($(USE_AIR_MODE_1),)
	CPPFLAGS += -DUSE_AIR_MODE_1
endif

PIN_BUTTON_TOUCH ?=
ifneq ($(PIN_BUTTON_TOUCH),)
	CPPFLAGS += -DPIN_BUTTON_TOUCH=$(PIN_BUTTON_TOUCH)
endif

CPPFLAGS += -I$(TARGET_DIR)

.PHONY: raven-help $(TARGET)

ifneq ($(TARGET),)
TARGET_COMPONENTS	:= $(subst _, ,$(TARGET))
BASE_PLATFORM 		:= $(firstword $(TARGET_COMPONENTS))
VARIANT				:= $(lastword $(TARGET_COMPONENTS))
# No other way to replace ' ' in subst than using this
space				:=
space				+=
PLATFORM			:= $(subst $(space),_,$(filter-out $(VARIANT),$(TARGET_COMPONENTS)))
CPPFLAGS			+= -I$(PLATFORMS_DIR)/$(PLATFORM) -I$(VARIANTS_DIR)/$(VARIANT)
.DEFAULT_GOAL 		:= $(TARGET)
else
BASE_PLATFORM	:=
PLATFORM		:=
VARIANT			:=
.DEFAULT_GOAL 	:= help
endif

export TARGET
export PLATFORM
export VARIANT
export CPPFLAGS
export PORT

help:
	@echo "No target specified"
	@echo "Valid targets are $(VALID_TARGETS)"
	@echo "Use TARGET=<target> make to compile a target"
	@echo "To flash a target, use TARGET=<target> PORT=<port> make flash"
	@echo "On macOS, Linux and Unix-like systems port must be the full port path e.g. /dev/tty.SLAB_USBtoUART"
	@echo "On Windows port must be specified by its number, e.g. COM10"

help-esp32:
	@ $(MAKE) -f Makefile.esp32 help

$(TARGET):
	@ PORT=$(PORT) PLATFORM=$(PLATFORM) VARIANT=$(VARIANT) TARGET=$(TARGET) $(MAKE) -f Makefile.$(BASE_PLATFORM)

erase: $(TARGET)
	@ PORT=$(PORT) PLATFORM=$(PLATFORM) VARIANT=$(VARIANT) TARGET=$(TARGET) $(MAKE) -f Makefile.$(BASE_PLATFORM) erase

flash: $(TARGET)
	@ PORT=$(PORT) PLATFORM=$(PLATFORM) VARIANT=$(VARIANT) TARGET=$(TARGET) $(MAKE) -f Makefile.$(BASE_PLATFORM) flash

monitor: $(TARGET)
	@ PORT=$(PORT) PLATFORM=$(PLATFORM) VARIANT=$(VARIANT) TARGET=$(TARGET) $(MAKE) -f Makefile.$(BASE_PLATFORM) monitor

menuconfig:
	@ PORT=$(PORT) PLATFORM=$(PLATFORM) VARIANT=$(VARIANT) TARGET=$(TARGET) $(MAKE) -f Makefile.$(BASE_PLATFORM) menuconfig

all:
	@ for target in $(VALID_TARGETS); do \
		TARGET=$$target $(MAKE); \
	done