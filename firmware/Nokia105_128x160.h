#ifndef NOKIA105_128x160_H
#define NOKIA105_128x160_H

#include <Arduino.h>
#include <SPI.h>

class Nokia105_128x160 {
public:
    // Hardware SPI constructor
    Nokia105_128x160(uint8_t pin_reset, uint8_t pin_cs, SPIClass* spi = &SPI);
    
    // Software SPI constructor
    Nokia105_128x160(uint8_t pin_reset, uint8_t pin_cs, uint8_t pin_sda, uint8_t pin_sck);
    
    void begin();
    void setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
    void writeFrameProgmem(const uint16_t* frame_ptr);
    void writeFrame1BitProgmem(const uint8_t* frame_ptr, uint16_t color, uint16_t bg);
    void writeFrameRLEProgmem(const uint8_t* rle_data);
    void writeFrameDeltaRLEProgmem(const uint16_t* palette, const uint8_t* delta_rle_data, uint32_t* offset);
    void writeFrameRAM(const uint16_t* screen_buf);
    void clearEntireRAM();

    void setRotation(uint8_t m);
    void invertDisplay(bool i);

    // Configuration
    static const int FRAME_WIDTH = 160;
    static const int FRAME_HEIGHT = 128;

private:
    uint8_t _pin_reset;
    uint8_t _pin_cs;
    uint8_t _pin_sda;
    uint8_t _pin_sck;
    
    SPIClass* _spi; // Hardware SPI pointer. If null, use Software SPI.
    uint16_t _spi_word_buf[8];
    uint8_t _spi_word_idx;
    bool _in_stream;

    void spiFlush();
    void spiWrite9(uint16_t word);
    void spiBeginStream();
    void spiEndStream();

    // Fast IO masks/pointers
    uint32_t _mask_cs;
    uint32_t _mask_sda;
    uint32_t _mask_sck;

    inline void writeCmd(uint8_t cmd);
    inline void writeData(uint8_t data);

    inline void setSCK(bool val);
    inline void setSDA(bool val);
    inline void setCS(bool val);
};

#endif
