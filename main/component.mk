COMPONENT_SRCDIRS := . air bluetooth config input io msp output p2p platform protocols rc rmp ui util
COMPONENT_PRIV_INCLUDEDIRS := .

# Sanity check
ifndef CONFIG_RAVEN_TX_SUPPORT
ifndef CONFIG_RAVEN_RX_SUPPORT
    $(error At least TX or RX support must be enabled)
endif
endif

RC_MODE :=

ifdef CONFIG_RAVEN_TX_SUPPORT
    CPPFLAGS += -DUSE_TX_SUPPORT
    RC_MODE := $(RC_MODE)TX
endif

ifdef CONFIG_RAVEN_FAKE_TX_INPUT
    CPPFLAGS += -DUSE_TX_FAKE_INPUT
endif

ifdef CONFIG_RAVEN_RX_SUPPORT
    CPPFLAGS += -DUSE_RX_SUPPORT
    RC_MODE := $(RC_MODE)RX
endif