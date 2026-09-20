#ifndef USER_SETUP_H
#define USER_SETUP_H

#define USER_SETUP_INFO "User_Setup"

// --- Display Driver ---
#define ILI9341_2_DRIVER

// --- Panel Metrics ---
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// --- Backlight Control ---
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// --- Display SPI Pins ---
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// --- Touch Controller Pins ---
#define TOUCH_CS 33

// --- Fonts ---
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// --- Clock Speeds ---
#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

#define USE_HSPI_PORT

#endif // USER_SETUP_H
