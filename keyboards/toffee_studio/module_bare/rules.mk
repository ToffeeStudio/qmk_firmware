ALLOW_WARNINGS = yes
VIA_ENABLE = yes
CONSOLE_ENABLE = yes
PRINT_ENABLE = yes

RGB_MATRIX_ENABLE = yes
RGBLIGHT_ENABLE = no
BACKLIGHT_ENABLE = no
WS2812_DRIVER = vendor

VIRTSER_ENABLE = yes
CDC_ENABLE = yes
EXTRA_USB_INTERFACES = yes

RAW_ENABLE = yes

SRC += animations/manager.c \
	   animations/anim_breathing.c \
       animations/anim_cycle_left_right.c \
       animations/anim_band_sat_left_right.c \
	   animations/anim_hue_breathing.c
