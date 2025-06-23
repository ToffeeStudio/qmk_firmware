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
       animations/anim_cycle_up_down.c \
       animations/anim_band_sat_left_right.c \
       animations/anim_band_sat_up_down.c \
	   animations/anim_hue_breathing.c \
       animations/anim_solid.c \
       animations/anim_rainbow_vortex.c \
       animations/anim_vortex.c \
       animations/anim_comet_tail.c
