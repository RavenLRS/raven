PROJECT_NAME := raven
export PROJECT_NAME

PORT ?=

# Version number and git revision of the build
VERSION_MAJOR := 0
VERSION_MINOR := 9
VERSION_PATCH := 0
VERSION_SUFFIX :=

VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)$(VERSION_SUFFIX)
export VERSION

GIT_REVISION := $(shell git rev-parse --short HEAD)

CPPFLAGS += -DGIT_REVISION=\"$(GIT_REVISION)\" -DVERSION=\"$(VERSION)\"

RELEASE ?=

ifeq ($(RELEASE),1)
CPPFLAGS += -DRAVEN_RELEASE
endif

WERROR ?=
ifeq ($(WERROR),1)
CPPFLAGS += -Werror
endif

V ?=

ifeq ($(V),1)
STDOUT_REDIR :=
else
STDOUT_REDIR := 1> /dev/null
endif

PLATFORM_VARIANT_SEPARATOR 	:= _
INVALID_TARGET 				:= <invalid>

# Platform/targets setup
ROOT           			:= $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
export ROOT
TARGET_DIR				:= $(ROOT)/main/target
PLATFORMS_DIR 			= $(TARGET_DIR)/platforms
export PLATFORMS_DIR
VALID_PLATFORMS 		= $(dir $(wildcard $(PLATFORMS_DIR)/*/platform.h))
VALID_PLATFORMS			:= $(subst /,,$(subst $(PLATFORMS_DIR)/,,$(VALID_PLATFORMS)))
VALID_PLATFORMS			:= $(sort $(VALID_PLATFORMS))

VARIANTS_DIR 			= $(TARGET_DIR)/variants
export VARIANTS_DIR
VALID_VARIANTS	 		= $(dir $(wildcard $(VARIANTS_DIR)/*/sdkconfig))
VALID_VARIANTS			:= $(subst /,,$(subst $(VARIANTS_DIR)/,,$(VALID_VARIANTS)))
VALID_VARIANTS			:= $(sort $(VALID_VARIANTS))

# Disable txrx targets for releases
RELEASE_VARIANTS		:= $(filter-out txrx,$(VALID_VARIANTS))

# By default, all platforms support all variants. Platforms that support only some variants
# can include a variants file with one supported variant per line.
variants_file			= $(PLATFORMS_DIR)/$(platform)/variants
make_target				= $(if $(shell grep ^$(variant)$$ $(variants_file) 2> /dev/null || (test ! -f $(variants_file) && echo $(variant))),$(platform)$(PLATFORM_VARIANT_SEPARATOR)$(variant),$(INVALID_TARGET))
VALID_TARGETS			:= $(foreach platform,$(VALID_PLATFORMS),$(foreach variant,$(VALID_VARIANTS),$(make_target)))
VALID_TARGETS			:= $(filter-out $(INVALID_TARGET),$(VALID_TARGETS))

RELEASE_TARGETS			:= $(foreach platform,$(VALID_PLATFORMS),$(foreach variant,$(RELEASE_VARIANTS),$(make_target)))
RELEASE_TARGETS			:= $(filter-out $(INVALID_TARGET),$(RELEASE_TARGETS))

TARGETS_FILTER ?=

ifneq ($(TARGETS_FILTER),)
VALID_TARGETS := $(filter %$(TARGETS_FILTER),$(VALID_TARGETS))
endif

CPPFLAGS += -I$(TARGET_DIR)

.PHONY: raven-help $(TARGET)

ifneq ($(TARGET),)
ifeq ($(filter $(TARGET),$(VALID_TARGETS)),)
$(error $(TARGET) is not a valid target. See make help.)
endif
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
export BASE_PLATFORM
export PLATFORM
export VARIANT
export CPPFLAGS
export PORT

RELEASE_BASENAME		:= $(PROJECT_NAME)_$(VERSION)_$(PLATFORM)_$(VARIANT)
RELEASES_DIR			:= $(ROOT)/releases
export RELEASE_BASENAME
export RELEASES_DIR

PLATFORM_MAKEFILE              := Makefile.$(BASE_PLATFORM)

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
	@ $(MAKE) -f $(PLATFORM_MAKEFILE)

clean:
ifneq (,$(wildcard $(PLATFORM_MAKEFILE)))
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) clean
endif
	@ $(RM) -r build-*

erase: $(TARGET)
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) erase

flash: $(TARGET)
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) flash

monitor: $(TARGET)
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) monitor

menuconfig:
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) menuconfig

size:
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) size

release:
ifeq ($(TARGET),)
	@ for target in $(RELEASE_TARGETS); do \
		echo "Building $$target"; \
		RELEASE=1 TARGET=$$target $(MAKE) release $(STDOUT_REDIR) || exit 1; \
	done
else
	@ mkdir -p $(RELEASES_DIR)
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) release
endif

release-clean:
	$(RM) -r $(RELEASES_DIR)

all:
	@ for target in $(VALID_TARGETS); do \
		TARGET=$$target $(MAKE); \
	done

# Build all targets, but run make clean after building each one
# to avoid using too much disk space.
ci-build:
	@ for target in $(VALID_TARGETS); do \
		echo "Building $$target"; \
		TARGET=$$target $(MAKE) 1> /dev/null || exit 1; \
		TARGET=$$target $(MAKE) clean 1> /dev/null; \
	done

base-platform-%:
	@ $(MAKE) -f $(PLATFORM_MAKEFILE) $*

show-targets:
	@echo "Valid targets are $(VALID_TARGETS)"

show-platforms:
	@echo "Valid platforms are $(VALID_PLATFORMS)"

format:
	./format.sh format

format-check:
	./format.sh check
