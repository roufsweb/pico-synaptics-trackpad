#include "Nokia105_128x160.h"

#if defined(ARDUINO_ARCH_RP2040)
  #include "hardware/structs/sio.h"
#endif

// ST7735S display commands
#define SPLOUT  0x11
#define COLMOD  0x3A
#define MADCTL  0x36
#define DISPON  0x29
#define CASET   0x2A
#define PASET   0x2B
#define RAMWR   0x2C

#define CLOCK_DELAY() asm volatile("nop\n nop\n nop\n nop\n")

#if defined(ESP32)
  #define DISABLE_INTS() portDISABLE_INTERRUPTS()
  #define ENABLE_INTS()  portENABLE_INTERRUPTS()
#else
  #define DISABLE_INTS() noInterrupts()
  #define ENABLE_INTS()  interrupts()
#endif

Nokia105_128x160::Nokia105_128x160(uint8_t pin_reset, uint8_t pin_cs, SPIClass* spi) {
    _pin_reset = pin_reset;
    _pin_cs = pin_cs;
    _spi = spi;
    _pin_sda = 0; // Not used
    _pin_sck = 0; // Not used
    _mask_cs = (1ul << _pin_cs);
    _spi_word_idx = 0;
    _in_stream = false;
}

Nokia105_128x160::Nokia105_128x160(uint8_t pin_reset, uint8_t pin_cs, uint8_t pin_sda, uint8_t pin_sck) {
    _pin_reset = pin_reset;
    _pin_cs = pin_cs;
    _pin_sda = pin_sda;
    _pin_sck = pin_sck;
    _spi = nullptr;
    
    _mask_cs = (1ul << _pin_cs);
    _mask_sda = (1ul << _pin_sda);
    _mask_sck = (1ul << _pin_sck);
    _spi_word_idx = 0;
    _in_stream = false;
}

inline void Nokia105_128x160::setSCK(bool val) {
#if defined(ESP32)
    if(val) REG_WRITE(GPIO_OUT_W1TS_REG, _mask_sck); else REG_WRITE(GPIO_OUT_W1TC_REG, _mask_sck);
#elif defined(ESP8266)
    if(val) GPOS = _mask_sck; else GPOC = _mask_sck;
#elif defined(ARDUINO_ARCH_RP2040)
    if(val) sio_hw->gpio_set = _mask_sck; else sio_hw->gpio_clr = _mask_sck;
#else
    digitalWrite(_pin_sck, val);
#endif
}

inline void Nokia105_128x160::setSDA(bool val) {
#if defined(ESP32)
    if(val) REG_WRITE(GPIO_OUT_W1TS_REG, _mask_sda); else REG_WRITE(GPIO_OUT_W1TC_REG, _mask_sda);
#elif defined(ESP8266)
    if(val) GPOS = _mask_sda; else GPOC = _mask_sda;
#elif defined(ARDUINO_ARCH_RP2040)
    if(val) sio_hw->gpio_set = _mask_sda; else sio_hw->gpio_clr = _mask_sda;
#else
    digitalWrite(_pin_sda, val);
#endif
}

inline void Nokia105_128x160::setCS(bool val) {
#if defined(ESP32)
    if(val) REG_WRITE(GPIO_OUT_W1TS_REG, _mask_cs); else REG_WRITE(GPIO_OUT_W1TC_REG, _mask_cs);
#elif defined(ESP8266)
    if(val) GPOS = _mask_cs; else GPOC = _mask_cs;
#elif defined(ARDUINO_ARCH_RP2040)
    if(val) sio_hw->gpio_set = _mask_cs; else sio_hw->gpio_clr = _mask_cs;
#else
    digitalWrite(_pin_cs, val);
#endif
}

void Nokia105_128x160::spiFlush() {
    if (!_spi || _spi_word_idx == 0) return;
    
    uint8_t bytes[9];
    memset(bytes, 0, 9);
    
    if (_spi_word_idx > 0) { bytes[0] = _spi_word_buf[0] >> 1; bytes[1] = (_spi_word_buf[0] << 7) & 0xFF; }
    if (_spi_word_idx > 1) { bytes[1] |= _spi_word_buf[1] >> 2; bytes[2] = (_spi_word_buf[1] << 6) & 0xFF; }
    if (_spi_word_idx > 2) { bytes[2] |= _spi_word_buf[2] >> 3; bytes[3] = (_spi_word_buf[2] << 5) & 0xFF; }
    if (_spi_word_idx > 3) { bytes[3] |= _spi_word_buf[3] >> 4; bytes[4] = (_spi_word_buf[3] << 4) & 0xFF; }
    if (_spi_word_idx > 4) { bytes[4] |= _spi_word_buf[4] >> 5; bytes[5] = (_spi_word_buf[4] << 3) & 0xFF; }
    if (_spi_word_idx > 5) { bytes[5] |= _spi_word_buf[5] >> 6; bytes[6] = (_spi_word_buf[5] << 2) & 0xFF; }
    if (_spi_word_idx > 6) { bytes[6] |= _spi_word_buf[6] >> 7; bytes[7] = (_spi_word_buf[6] << 1) & 0xFF; }
    if (_spi_word_idx > 7) { bytes[7] |= _spi_word_buf[7] >> 8; bytes[8] = _spi_word_buf[7] & 0xFF; }
    
    uint8_t num_bytes = (_spi_word_idx * 9 + 7) / 8;
    
#if defined(ESP8266) || defined(ESP32)
    _spi->writeBytes(bytes, num_bytes);
#else
    for (int i = 0; i < num_bytes; i++) _spi->transfer(bytes[i]);
#endif

    _spi_word_idx = 0;
}

void Nokia105_128x160::spiWrite9(uint16_t word) {
    if (_spi) {
        _spi_word_buf[_spi_word_idx++] = word;
        if (_spi_word_idx == 8) {
            spiFlush();
        }
    } else {
        setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x100); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x080); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x040); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x020); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x010); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x008); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x004); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x002); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
        setSDA(word & 0x001); CLOCK_DELAY(); setSCK(1); CLOCK_DELAY(); setSCK(0); CLOCK_DELAY();
    }
}

void Nokia105_128x160::spiBeginStream() {
    spiFlush();
    if (_spi) _spi->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    setCS(0); CLOCK_DELAY();
    _in_stream = true;
}

void Nokia105_128x160::spiEndStream() {
    spiFlush();
    setCS(1); CLOCK_DELAY();
    if (_spi) _spi->endTransaction();
    _in_stream = false;
}

inline void Nokia105_128x160::writeCmd(uint8_t cmd) {
    bool local_cs = !_in_stream;
    if (local_cs) {
        if (_spi) _spi->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
        setCS(0); CLOCK_DELAY();
    }
    spiWrite9(0x000 | cmd);
    if (local_cs) {
        spiFlush(); setCS(1); CLOCK_DELAY();
        if (_spi) _spi->endTransaction();
    }
}

inline void Nokia105_128x160::writeData(uint8_t data) {
    bool local_cs = !_in_stream;
    if (local_cs) {
        if (_spi) _spi->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
        setCS(0); CLOCK_DELAY();
    }
    spiWrite9(0x100 | data);
    if (local_cs) {
        spiFlush(); setCS(1); CLOCK_DELAY();
        if (_spi) _spi->endTransaction();
    }
}

void Nokia105_128x160::setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    spiBeginStream();
    writeCmd(CASET); writeData(0x00); writeData(x0); writeData(0x00); writeData(x1);
    writeCmd(PASET); writeData(0x00); writeData(y0); writeData(0x00); writeData(y1);
    writeCmd(RAMWR);
    spiEndStream();
}

void Nokia105_128x160::begin() {
    pinMode(_pin_reset, OUTPUT); 
    pinMode(_pin_cs, OUTPUT); 
    pinMode(_pin_sda, OUTPUT); 
    pinMode(_pin_sck, OUTPUT);

    setCS(1); setSCK(0); setSDA(0);

    digitalWrite(_pin_reset, LOW); delay(50); digitalWrite(_pin_reset, HIGH); delay(120);
    writeCmd(0x01); delay(120);
    writeCmd(0x11); delay(120);
    
    writeCmd(0xB1); writeData(0x01); writeData(0x2C); writeData(0x2D);
    writeCmd(0xB2); writeData(0x01); writeData(0x2C); writeData(0x2D);
    writeCmd(0xB3); writeData(0x01); writeData(0x2C); writeData(0x2D); writeData(0x01); writeData(0x2C); writeData(0x2D);
    writeCmd(0xB4); writeData(0x07);
    writeCmd(0xC0); writeData(0xA2); writeData(0x02); writeData(0x84);
    writeCmd(0xC1); writeData(0xC5);
    writeCmd(0xC2); writeData(0x0A); writeData(0x00);
    writeCmd(0xC3); writeData(0x8A); writeData(0x2A);
    writeCmd(0xC4); writeData(0x8A); writeData(0xEE);
    writeCmd(0xC5); writeData(0x0E);
    
    // LANDSCAPE MODE ROTATION
    writeCmd(0x36);
    writeData(0xA8); // MV=1, MY=1, BGR=1
    
    writeCmd(0x3A); writeData(0x05); // 16-bit color
    writeCmd(0xE0); writeData(0x02); writeData(0x1C); writeData(0x07); writeData(0x12); writeData(0x37); writeData(0x32); writeData(0x29); writeData(0x2D); writeData(0x29); writeData(0x25); writeData(0x2B); writeData(0x39); writeData(0x00); writeData(0x01); writeData(0x03); writeData(0x10);
    writeCmd(0xE1); writeData(0x03); writeData(0x1D); writeData(0x07); writeData(0x06); writeData(0x2E); writeData(0x2C); writeData(0x29); writeData(0x2D); writeData(0x2E); writeData(0x2E); writeData(0x37); writeData(0x3F); writeData(0x00); writeData(0x00); writeData(0x02); writeData(0x10);
    
    clearEntireRAM();
    writeCmd(0x29); delay(100);
}

void Nokia105_128x160::clearEntireRAM() {
    setWindow(0, 0, FRAME_WIDTH - 1, FRAME_HEIGHT - 1);
    
    DISABLE_INTS();
    spiBeginStream();
    for (int i = 0; i < 20480; i++) {
        spiWrite9(0x100);
        spiWrite9(0x100);
    }
    spiEndStream();
    ENABLE_INTS();
}

void Nokia105_128x160::writeFrameProgmem(const uint16_t* frame_ptr) {
    setWindow(0, 0, FRAME_WIDTH - 1, FRAME_HEIGHT - 1);
    DISABLE_INTS();
    spiBeginStream();
    
    for (int i = 0; i < 20480; i++) {
        uint16_t pixel = pgm_read_word(&frame_ptr[i]);
        spiWrite9(0x100 | (pixel >> 8));
        spiWrite9(0x100 | (pixel & 0xFF));
    }
    spiEndStream();
    ENABLE_INTS();
}

void Nokia105_128x160::writeFrame1BitProgmem(const uint8_t* frame_ptr, uint16_t color, uint16_t bg) {
    setWindow(0, 0, FRAME_WIDTH - 1, FRAME_HEIGHT - 1);
    DISABLE_INTS();
    spiBeginStream();
    
    uint8_t color_hi = color >> 8;
    uint8_t color_lo = color & 0xFF;
    uint8_t bg_hi = bg >> 8;
    uint8_t bg_lo = bg & 0xFF;
    
    for (int i = 0; i < (FRAME_WIDTH * FRAME_HEIGHT) / 8; i++) {
        uint8_t b = pgm_read_byte(&frame_ptr[i]);
        
        for (int bit = 7; bit >= 0; bit--) {
            if (b & (1 << bit)) {
                spiWrite9(0x100 | color_hi);
                spiWrite9(0x100 | color_lo);
            } else {
                spiWrite9(0x100 | bg_hi);
                spiWrite9(0x100 | bg_lo);
            }
        }
    }
    spiEndStream();
    ENABLE_INTS();
}

void Nokia105_128x160::writeFrameDeltaRLEProgmem(const uint16_t* palette, const uint8_t* delta_rle_data, uint32_t* offset) {
    DISABLE_INTS();
    
    uint32_t idx = *offset;
    uint32_t pixel_ptr = 0; // 0 to 20479
    
    while (pixel_ptr < 20480) {
        uint8_t opcode = pgm_read_byte(&delta_rle_data[idx++]);
        if (opcode == 0x00) {
            // End of frame marker
            break;
        }
        
        if ((opcode & 0x80) == 0) {
            // Skip N pixels
            pixel_ptr += opcode;
        } else {
            // Write N pixels
            uint8_t count = opcode & 0x7F;
            
            // Set window to current pixel_ptr
            uint8_t x = pixel_ptr % FRAME_WIDTH;
            uint8_t y = pixel_ptr / FRAME_WIDTH;
            
            // We set the end window to the bottom-right of the screen so auto-increment works
            spiBeginStream();
            writeCmd(0x2A); writeData(0x00); writeData(x); writeData(0x00); writeData(FRAME_WIDTH - 1); // CASET
            writeCmd(0x2B); writeData(0x00); writeData(y); writeData(0x00); writeData(FRAME_HEIGHT - 1); // PASET
            writeCmd(0x2C); // RAMWR
            
            uint8_t written = 0;
            while (written < count) {
                uint8_t packed = pgm_read_byte(&delta_rle_data[idx++]);
                
                // First pixel (high nibble)
                uint8_t p1_idx = packed >> 4;
                uint16_t color1 = pgm_read_word(&palette[p1_idx]);
                spiWrite9(0x100 | (color1 >> 8));
                spiWrite9(0x100 | (color1 & 0xFF));
                written++;
                pixel_ptr++;
                
                if (written < count) {
                    // Second pixel (low nibble)
                    uint8_t p2_idx = packed & 0x0F;
                    uint16_t color2 = pgm_read_word(&palette[p2_idx]);
                    spiWrite9(0x100 | (color2 >> 8));
                    spiWrite9(0x100 | (color2 & 0xFF));
                    written++;
                    pixel_ptr++;
                }
            }
            spiEndStream();
        }
    }
    
    *offset = idx;
    ENABLE_INTS();
}

void Nokia105_128x160::writeFrameRAM(const uint16_t* screen_buf) {
    setWindow(0, 0, FRAME_WIDTH - 1, FRAME_HEIGHT - 1);
    DISABLE_INTS();
    spiBeginStream();
    
    for (int i = 0; i < 20480; i++) {
        uint16_t pixel = screen_buf[i];
        spiWrite9(0x100 | (pixel >> 8));
        spiWrite9(0x100 | (pixel & 0xFF));
    }
    
    spiEndStream();
    ENABLE_INTS();
}

void Nokia105_128x160::setRotation(uint8_t m) {
    writeCmd(0x36); // MADCTL
    switch (m % 4) {
        case 0: // Portrait
            writeData(0xC8); // MY=1, MX=1, BGR=1
            break;
        case 1: // Landscape
            writeData(0xA8); // MV=1, MY=1, BGR=1
            break;
        case 2: // Inverted Portrait
            writeData(0x08); // BGR=1
            break;
        case 3: // Inverted Landscape
            writeData(0x68); // MV=1, MX=1, BGR=1
            break;
    }
}

void Nokia105_128x160::invertDisplay(bool i) {
    writeCmd(i ? 0x21 : 0x20); // INVON : INVOFF
}

void Nokia105_128x160::writeFrameRLEProgmem(const uint8_t* rle_data) {
    setWindow(0, 0, FRAME_WIDTH - 1, FRAME_HEIGHT - 1);
    DISABLE_INTS();
    spiBeginStream();
    
    int index = 0;
    while (true) {
        uint8_t count = pgm_read_byte(&rle_data[index++]);
        uint8_t hi = pgm_read_byte(&rle_data[index++]);
        uint8_t lo = pgm_read_byte(&rle_data[index++]);
        
        if (count == 0 && hi == 0 && lo == 0) break; // End of frame marker
        
        while (count--) {
            spiWrite9(0x100 | hi);
            spiWrite9(0x100 | lo);
        }
    }
    
    spiEndStream();
    ENABLE_INTS();
}
