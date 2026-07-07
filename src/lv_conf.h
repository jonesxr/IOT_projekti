/**
 * @file lv_conf.h
 * MINIMAL LVGL CONFIGURATION FOR ESP32 & LOVYANGFX
 */

#pragma once

/*====================
   COLOR SETTINGS
 *====================*/
/*Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH 16

/*Swap the 2 bytes of RGB565 color. Useful if the display has a 8 bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 1

/*=========================
   MEMORY SETTINGS
 *=========================*/
/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()`*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)          /*[bytes]*/
#define LV_MEM_ADR 0                       /*0: allocate in system RAM (or malloc)*/

/*====================
   HAL SETTINGS
 *====================*/
/*Default display refresh period. LVG will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD 30      /*[ms]*/

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD 30     /*[ms]*/

/*Use a custom tick source that tells the elapsed time in milliseconds.
 *It removes the need to manually update the tick with `lv_tick_inc()`*/
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
/*1: Enable GPU (e.g. DMA) support*/
#define LV_USE_GPU_STM32_DMA2D 0

/*Logging*/
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/*==================
 * FONT USAGE
 *===================*/
/*Enable the built-in fonts*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*===================
 * THEME USAGE
 *====================*/
/*Always enable at least on theme*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
