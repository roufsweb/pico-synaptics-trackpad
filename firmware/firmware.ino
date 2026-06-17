#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include "Nokia105_128x160.h"

#define PIN_RESET 16
#define PIN_CS    17
#define BUTTON_PIN 24

// Hardware SPI Pins for RP2040: SDA(TX) = 19, SCK = 18, RX = 4
Nokia105_128x160 display(PIN_RESET, PIN_CS, &SPI);
uint16_t screen_buf[20480];

void fillScreen(uint16_t color) {
    for (int i = 0; i < 20480; i++) {
        screen_buf[i] = color;
    }
}

void drawPixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < 160 && y >= 0 && y < 128) {
        screen_buf[y * 160 + x] = color;
    }
}

void fadeScreen() {
    for (int i = 0; i < 20480; i++) {
        uint16_t c = screen_buf[i];
        if (c != 0) {
            uint16_t r = (c >> 11) & 0x1F;
            uint16_t g = (c >> 5) & 0x3F;
            uint16_t b = c & 0x1F;
            
            if (r > 0) r--;
            if (g > 1) g -= 2; else if (g > 0) g--;
            if (b > 0) b--;
            
            screen_buf[i] = (r << 11) | (g << 5) | b;
        }
    }
}

// ==========================================
// HID DESCRIPTOR: STANDARD MOUSE
// ==========================================
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(1))
};



Adafruit_USBD_HID usb_hid;

// Standard HID callbacks
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  return 0;
}

void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // We can ignore SET_REPORT for now.
}

// ==========================================
// PS/2 HARWARE & PINS
// ==========================================
#define PS2_DATA_PIN 11
#define PS2_CLK_PIN 10

class PS2Mouse {
private:
    volatile uint8_t bit_count = 0;
    volatile uint8_t incoming_byte = 0;
    volatile bool in_transmission = false;
    
    // Volatile circular buffer
    static const int BUF_SIZE = 64;
    volatile uint8_t buffer[BUF_SIZE];
    volatile int head = 0;
    volatile int tail = 0;

    static PS2Mouse* instance;

    static void clk_isr() {
        if (instance) instance->handle_clk();
    }

    void handle_clk() {
        int data_bit = digitalRead(PS2_DATA_PIN);
        
        if (!in_transmission) {
            if (data_bit == 0) {
                in_transmission = true;
                bit_count = 0;
                incoming_byte = 0;
            }
        } else {
            if (bit_count < 8) {
                if (data_bit) {
                    incoming_byte |= (1 << bit_count);
                }
                bit_count++;
            } else if (bit_count == 8) {
                bit_count++;
            } else if (bit_count == 9) {
                int next_head = (head + 1) % BUF_SIZE;
                if (next_head != tail) {
                    buffer[head] = incoming_byte;
                    head = next_head;
                }
                in_transmission = false;
            }
        }
    }

public:
    void begin() {
        instance = this;
        pinMode(PS2_CLK_PIN, INPUT_PULLUP);
        pinMode(PS2_DATA_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PS2_CLK_PIN), clk_isr, FALLING);
        
        delay(10);
        write(0xFF); // Reset
        delay(500);
        head = 0; tail = 0;

        // Synaptics Magic Sequence for Absolute W-Mode (Mode Byte 0xC1)
        write(0xE8); write(0x03); 
        write(0xE8); write(0x00); 
        write(0xE8); write(0x00); 
        write(0xE8); write(0x01); 
        write(0xF3); write(0x14); 
        write(0xF4); // Enable
        delay(10);
        head = 0; tail = 0;
    }

    void write(uint8_t data) {
        detachInterrupt(digitalPinToInterrupt(PS2_CLK_PIN));
        pinMode(PS2_CLK_PIN, OUTPUT);
        pinMode(PS2_DATA_PIN, OUTPUT);
        
        digitalWrite(PS2_CLK_PIN, LOW);
        delayMicroseconds(100);
        digitalWrite(PS2_DATA_PIN, LOW);
        digitalWrite(PS2_CLK_PIN, HIGH);
        pinMode(PS2_CLK_PIN, INPUT_PULLUP);
        
        while (digitalRead(PS2_CLK_PIN) == HIGH);
        
        bool parity = true;
        for (int i = 0; i < 8; i++) {
            digitalWrite(PS2_DATA_PIN, (data >> i) & 1);
            parity ^= ((data >> i) & 1);
            while (digitalRead(PS2_CLK_PIN) == LOW);
            while (digitalRead(PS2_CLK_PIN) == HIGH);
        }
        
        digitalWrite(PS2_DATA_PIN, parity);
        while (digitalRead(PS2_CLK_PIN) == LOW);
        while (digitalRead(PS2_CLK_PIN) == HIGH);
        
        pinMode(PS2_DATA_PIN, INPUT_PULLUP);
        while (digitalRead(PS2_DATA_PIN) == HIGH);
        while (digitalRead(PS2_CLK_PIN) == LOW);
        while (digitalRead(PS2_CLK_PIN) == HIGH);
        
        attachInterrupt(digitalPinToInterrupt(PS2_CLK_PIN), clk_isr, FALLING);
    }

    int available() {
        if (head >= tail) return head - tail;
        return BUF_SIZE - tail + head;
    }

    uint8_t read() {
        if (head == tail) return 0;
        uint8_t val = buffer[tail];
        tail = (tail + 1) % BUF_SIZE;
        return val;
    }
};

PS2Mouse* PS2Mouse::instance = nullptr;
PS2Mouse trackpad;

volatile int diag_x = 0;
volatile int diag_y = 0;
volatile int diag_z = 0;
volatile int diag_w = 0;

// ==========================================
// CORE 0: Handles PS/2 & USB PTP
// ==========================================
void setup() {
    Serial.begin(115200);
    
    // Setup TinyUSB HID
    usb_hid.setPollInterval(2);
    usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    usb_hid.setReportCallback(get_report_callback, set_report_callback);
    usb_hid.begin();

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    trackpad.begin();
}

void loop() {
    static uint8_t packet[6];
    static int pkt_idx = 0;
    static unsigned long last_packet_time = 0;
    
    static bool last_finger_down = false;
    static int last_x = 0;
    static int last_y = 0;
    static uint8_t last_buttons = 0;

    // Timeout resync
    if (millis() - last_packet_time > 50) {
        pkt_idx = 0;
    }

    while (trackpad.available() > 0) {
        uint8_t b = trackpad.read();
        last_packet_time = millis();
        packet[pkt_idx] = b;
        
        // Strict Synaptics 6-byte Absolute W-Mode Sync
        if (pkt_idx == 0 && (b & 0xC8) != 0x80) continue;
        if (pkt_idx == 3 && (b & 0xC8) != 0xC0) { pkt_idx = 0; continue; }
        
        pkt_idx++;
        
        if (pkt_idx == 6) {
            bool physical_left = packet[0] & 0x01;
            int x = packet[4] | ((packet[1] & 0x0F) << 8) | ((packet[3] & 0x10) << 8);
            int y = packet[5] | ((packet[1] & 0xF0) << 4) | ((packet[3] & 0x20) << 7);
            int z = packet[2];
            int w = ((packet[0] & 0x30) >> 2) | ((packet[0] & 0x04) >> 1) | ((packet[3] & 0x04) >> 2);
            
            uint8_t buttons = 0;
            if (physical_left || digitalRead(BUTTON_PIN) == LOW) {
                buttons |= MOUSE_BUTTON_LEFT;
            }
            
            // Update global vars for LCD Core 1
            if (z > 30) {
                diag_x = x;
                diag_y = 5924 - y;
                diag_z = z;
                diag_w = w;
                
                if (last_finger_down) {
                    int dx = x - last_x;
                    int dy = y - last_y; // Y is bottom-to-top natively from trackpad
                    
                    static float dx_accumulator = 0.0;
                    static float dy_accumulator = 0.0;
                    static int scroll_accumulator = 0;
                    
                    if (usb_hid.ready()) {
                        if (w == 0 || w == 1) {
                            // 2 or 3 fingers -> Scroll
                            scroll_accumulator += (-dy); 
                            
                            int scroll = 0;
                            int scroll_threshold = 120;
                            
                            if (scroll_accumulator >= scroll_threshold) {
                                scroll = 1;
                                scroll_accumulator -= scroll_threshold;
                            } else if (scroll_accumulator <= -scroll_threshold) {
                                scroll = -1;
                                scroll_accumulator += scroll_threshold;
                            }
                            
                            if (scroll != 0) {
                                usb_hid.mouseReport(1, buttons, 0, 0, scroll, 0);
                            }
                            
                            // Reset movement accumulators when scrolling
                            dx_accumulator = 0.0;
                            dy_accumulator = 0.0;
                        } else {
                            // 1 finger -> Move
                            scroll_accumulator = 0; // Reset scroll when moving
                            
                            // Accumulate fractional movement for extremely high precision
                            // Dividing by 3.0 gives the DPI we want, but saves the sub-pixel data!
                            dx_accumulator += (float)dx / 3.0f;
                            dy_accumulator += (float)(-dy) / 3.0f;
                            
                            int send_dx = (int)dx_accumulator;
                            int send_dy = (int)dy_accumulator;
                            
                            // Remove the integer part we are about to send, keep the fraction
                            dx_accumulator -= send_dx;
                            dy_accumulator -= send_dy;
                            
                            send_dx = max(-127, min(127, send_dx));
                            send_dy = max(-127, min(127, send_dy));
                            
                            if (send_dx != 0 || send_dy != 0 || buttons != last_buttons) {
                                usb_hid.mouseReport(1, buttons, send_dx, send_dy, 0, 0);
                            }
                        }
                    }
                }
                
                last_x = x;
                last_y = y;
                last_finger_down = true;
            } else {
                diag_z = 0;
                last_finger_down = false;
                
                // If button state changed and no touch, send click
                if (buttons != last_buttons && usb_hid.ready()) {
                    usb_hid.mouseReport(1, buttons, 0, 0, 0, 0);
                }
            }
            
            last_buttons = buttons;
            pkt_idx = 0;
        }
    }
}

// ==========================================
// CORE 1: Handles SPI Display Output
// ==========================================
void setup1() {
    SPI.setRX(4);
    SPI.setSCK(18);
    SPI.setTX(19);
    SPI.begin();
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    display.begin();
    display.setRotation(1);

    fillScreen(0x0000);
    display.writeFrameRAM(screen_buf);
}

void loop1() {
    fadeScreen();
    
    if (diag_z > 30) {
        int cx = map(diag_x, 1200, 5600, 0, 159);
        int cy = map(diag_y, 0, 5924, 0, 127); // mapped with inverted Y
        
        if (cx < 0) cx = 0; if (cx > 159) cx = 159;
        if (cy < 0) cy = 0; if (cy > 127) cy = 127;
        
        uint16_t color;
        if (diag_w >= 4) color = 0xFFFF;      
        else if (diag_w == 0) color = 0x07E0; 
        else if (diag_w == 1) color = 0x001F; 
        else color = 0xF800;                  
        
        for(int dx=-1; dx<=1; dx++) {
            for(int dy=-1; dy<=1; dy++) {
                drawPixel(cx+dx, cy+dy, color);
            }
        }
    }
    
    display.writeFrameRAM(screen_buf);
    delay(10);
}
