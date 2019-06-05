
#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// #define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
// #define TFT_RGB_ORDER TFT_BGR // Colour order Blue-Green-Red

#define TFT_MISO            -1  
#define TFT_MOSI            19  
#define TFT_SCLK            18  
#define TFT_CS              5   
#define TFT_DC              27  
#define TFT_RST             -1  

// #define SPI_FREQUENCY   1000000
// #define SPI_FREQUENCY   5000000
// #define SPI_FREQUENCY  10000000
// #define SPI_FREQUENCY  20000000
// #define SPI_FREQUENCY  27000000 // Actually sets it to 26.67MHz = 80/3
#define SPI_FREQUENCY  40000000 // Maximum to use SPIFFS
// #define SPI_FREQUENCY  80000000

