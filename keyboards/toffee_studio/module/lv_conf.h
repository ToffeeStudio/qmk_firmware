#pragma once

#include_next <lv_conf.h>

// Force 16-bit color
#undef LV_COLOR_DEPTH
#define LV_COLOR_DEPTH 16

// Swap red/blue channels (typical for GC9xxx-based panels)
#undef LV_COLOR_16_SWAP
#define LV_COLOR_16_SWAP 1

#undef LV_USE_IMG
#define LV_USE_IMG 1

#undef LV_USE_GIF
#define LV_USE_GIF 1

#undef LV_USE_FS_LITTLEFS
#define LV_USE_FS_LITTLEFS 1

#undef LV_FS_LITTLEFS_LETTER
#define LV_FS_LITTLEFS_LETTER 'L'

#undef LV_FS_LITTLEFS_CACHE_SIZE
#define LV_FS_LITTLEFS_CACHE_SIZE 512

#undef LV_MEM_SIZE
#define LV_MEM_SIZE (32U * 1024U)

#undef LV_USE_USER_DATA
#define LV_USE_USER_DATA 1

