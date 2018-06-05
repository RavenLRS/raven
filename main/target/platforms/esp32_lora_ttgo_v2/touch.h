#if !defined(PIN_BUTTON_TOUCH)
// Don't use GPIO0, since that's wired to the UART<->USB chip
// and can't work as touch pad
#define PIN_BUTTON_TOUCH 4
#endif
