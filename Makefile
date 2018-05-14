#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

VERSION_MAJOR := 0
VERSION_MINOR := 9
VERSION_PATCH := 0
VERSION_SUFFIX :=

VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)$(VERSION_SUFFIX)

GIT_REVISION := $(shell git rev-parse --short HEAD)

CPPFLAGS += -DGIT_REVISION=\"$(GIT_REVISION)\" -DVERSION=\"$(VERSION)\"
USE_AIR_MODE_1 ?=
ifneq ($(USE_AIR_MODE_1),)
  CPPFLAGS += -DUSE_AIR_MODE_1
endif
PROJECT_NAME := raven

ifdef VARIANT
SDKCONFIG = $(PROJECT_PATH)/sdkconfig.$(VARIANT)
PROJECT_NAME := $(PROJECT_NAME)_$(VARIANT)
BUILD_DIR_BASE = $(PROJECT_PATH)/build-$(VARIANT)
endif


include $(IDF_PATH)/make/project.mk
