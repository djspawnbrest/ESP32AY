// See SetupX_Template.h for all options available
#define USER_SETUP_ID 42

#define USE_HSPI_PORT
// #define USE_FSPI_PORT //won't work

//#define ST7735_DRIVER
//#define ST7735_GREENTAB2

#define ST7789_DRIVER
//#define ST7789_2_DRIVER

//#define CGRAM_OFFSET      // Library will add offsets required

//#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red
//#define TFT_RGB_ORDER TFT_GBR  // Colour order Blue-Green-Red
//#define TFT_RGB_ORDER TFT_GRB  // Colour order Blue-Green-Red
//#define TFT_RGB_ORDER TFT_BRG  // Colour order Blue-Green-Red

//#define TFT_INVERSION_ON
#define TFT_INVERSION_OFF

// #define TFT_BL           -1    // LED back-light control pin//
#define TFT_BACKLIGHT_ON LOW  // Level to turn ON back-light (HIGH or LOW)

//#define TFT_MISO 19
#define TFT_MOSI 16	 //23 16
#define TFT_SCLK 17  //18 17
#define TFT_CS   13  // Chip select control pin
#define TFT_DC   12  // Data Command control pin
#define TFT_RST  -1  // Reset pin (could connect to RST pin)

// For ST7789, ST7735, ILI9163 and GC9A01 ONLY, define the pixel width and height in portrait orientation
// #define TFT_WIDTH  80
// #define TFT_WIDTH  128
// #define TFT_WIDTH  172 // ST7789 172 x 320
// #define TFT_WIDTH  170 // ST7789 170 x 320
#define TFT_WIDTH  240 // ST7789 240 x 240 and 240 x 320
// #define TFT_HEIGHT 160
// #define TFT_HEIGHT 128
// #define TFT_HEIGHT 240 // ST7789 240 x 240
#define TFT_HEIGHT 320 // ST7789 240 x 320
// #define TFT_HEIGHT 240 // GC9A01 240 x 240

// Optional touch screen chip select
//#define TOUCH_CS 5 // Chip select pin (T_CS) of touch screen

// #define LOAD_GLCD    // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
// #define LOAD_FONT2   // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
// #define LOAD_FONT4   // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
// #define LOAD_FONT6   // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
// #define LOAD_FONT7   // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
// #define LOAD_FONT8   // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF   // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// #define SMOOTH_FONT

// TFT SPI clock frequency
// #define SPI_FREQUENCY  20000000
// #define SPI_FREQUENCY  27000000
// #define SPI_FREQUENCY  30000000
// #define SPI_FREQUENCY  40000000
// #define SPI_FREQUENCY  60000000
// #define SPI_FREQUENCY  62500000 // <- worekd
// #define SPI_FREQUENCY  65000000
// #define SPI_FREQUENCY  70000000
// #define SPI_FREQUENCY  75000000
// #define SPI_FREQUENCY  80000000
// #define SPI_FREQUENCY  100000000
#define SPI_FREQUENCY  120000000

// Optional reduced SPI frequency for reading TFT
//#define SPI_READ_FREQUENCY  60000000

// SPI clock frequency for touch controller
//#define SPI_TOUCH_FREQUENCY  2500000
