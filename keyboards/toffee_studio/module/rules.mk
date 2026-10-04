LITTLEFS_ENABLE = yes
ALLOW_WARNINGS = yes
VIA_ENABLE = yes
CONSOLE_ENABLE = yes
PRINT_ENABLE = yes
EEPROM_RESET = yes

QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += gc9107_spi
QUANTUM_PAINTER_LVGL_INTEGRATION = yes

SRC += lighting/groups.c lighting/storage.c lighting/power.c lighting/power_profile.c lighting/power_driver.c \
       rawhid/module_raw_hid.c \
	   persistence.c \
	   display/wpm_indicator.c \
       display/animation.c \
       display/ui.c \
       display/cdc_handler.c \
       lighting/lighting.c \
       animations/manager.c \
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

VPATH += keyboards/toffee_studio/module/rawhid
VPATH += keyboards/toffee_studio/module/display
VPATH += keyboards/toffee_studio/module/animations

PICO_FLASH_SIZE_BYTES = 16*1024*1024
FLASH_RESERVATION_KB = 1024

RGB_MATRIX_ENABLE = yes
RGBLIGHT_ENABLE = no
BACKLIGHT_ENABLE = no
WS2812_DRIVER = vendor
WS2812_DRIVER_REQUIRED = yes

VIRTSER_ENABLE = yes
CDC_ENABLE = yes
EXTRA_USB_INTERFACES = yes
WPM_ENABLE = yes

# Explicit opt-in only; the normal product build keeps saved-startup behaviour.
ifeq ($(MODULE_EXTENSIVE_TEST),yes)
    ifndef MODULE_TEST_BUILD_ID
        $(error MODULE_TEST_BUILD_ID is required for MODULE_EXTENSIVE_TEST=yes)
    endif
    OPT_DEFS += -DMODULE_EXTENSIVE_TEST -DMODULE_TEST_BUILD_ID=$(MODULE_TEST_BUILD_ID)
    SRC += lighting/test_mode.c
endif
