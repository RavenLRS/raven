COMPONENT_SRCDIRS := . air bluetooth config input io msp output ota p2p platform protocols rc rmp ui util
# Must be a relative dir, so we can't use $PLATFORMS_DIR
COMPONENT_SRCDIRS += $(addprefix target/platforms/,$(PLATFORM_SOURCES))
COMPONENT_PRIV_INCLUDEDIRS := .

# Sanity check
ifndef CONFIG_RAVEN_TX_SUPPORT
ifndef CONFIG_RAVEN_RX_SUPPORT
    $(error At least TX or RX support must be enabled)
endif
endif

ifdef CONFIG_RAVEN_TX_SUPPORT
    CPPFLAGS += -DUSE_TX_SUPPORT
endif

ifdef CONFIG_RAVEN_RX_SUPPORT
    CPPFLAGS += -DUSE_RX_SUPPORT
endif
