#include <SPI.h>
#include "Nokia105_128x160.h"
#include "Adafruit_TinyUSB.h"

// Trackpad pins
#define PS2_CLK_PIN 10
#define PS2_DATA_PIN 11
#define BUTTON_PIN 15

// HID Report Descriptor
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_MOUSE()
};

Adafruit_USBD_HID usb_hid;

// Display globals
uint16_t screen_buf[160 * 128];

void drawPixel(int x, int y, uint16_t color) {
    if (x >= 0 && x < 160 && y >= 0 && y < 128) {
        screen_buf[y * 160 + x] = color;
    }
}

void fadeScreen() {
    for (int i = 0; i < 160 * 128; i++) {
        uint16_t c = screen_buf[i];
        if (c != 0) {
            uint8_t r = (c >> 11) & 0x1F;
            uint8_t g = (c >> 5) & 0x3F;
            uint8_t b = c & 0x1F;
            if (r > 0) r--;
            if (g > 1) g -= 2;
            if (b > 0) b--;
            screen_buf[i] = (r << 11) | (g << 5) | b;
        }
    }
}

void fillScreen(uint16_t color) {
    for (int i = 0; i < 160 * 128; i++) {
        screen_buf[i] = color;
    }
}

Nokia105_128x160 display(20, 17, &SPI);

// PS2 Ring Buffer
#define BUF_SIZE 256

class PS2Mouse {
private:
    volatile int head = 0;
    volatile int tail = 0;
    volatile uint8_t buffer[BUF_SIZE];
    
    volatile int bit_count = 0;
    volatile uint8_t current_byte = 0;
    volatile uint8_t parity = 0;
    volatile unsigned long last_time = 0;

    static PS2Mouse* instance;

    static void clk_isr() {
        if (instance == nullptr) return;
        
        unsigned long now = micros();
        if (now - instance->last_time > 2000) {
            instance->bit_count = 0;
            instance->current_byte = 0;
            instance->parity = 0;
        }
        instance->last_time = now;
        
        int val = digitalRead(PS2_DATA_PIN);
        
        if (instance->bit_count == 0) {
            if (val != LOW) {
                instance->bit_count = 0;
                return;
            }
        } else if (instance->bit_count >= 1 && instance->bit_count <= 8) {
            instance->current_byte |= (val << (instance->bit_count - 1));
            instance->parity ^= val;
        } else if (instance->bit_count == 9) {
            if (val != (1 - instance->parity)) {
                instance->bit_count = 0;
                return;
            }
        } else if (instance->bit_count == 10) {
            if (val == HIGH) {
                int next_head = (instance->head + 1) % BUF_SIZE;
                if (next_head != instance->tail) {
                    instance->buffer[instance->head] = instance->current_byte;
                    instance->head = next_head;
                }
            }
            instance->bit_count = -1; 
        }
        
        instance->bit_count++;
    }

public:
    void begin() {
        instance = this;
        pinMode(PS2_CLK_PIN, INPUT_PULLUP);
        pinMode(PS2_DATA_PIN, INPUT_PULLUP);
        
        // This is called in setup1(), so the interrupt fires exclusively on Core 1!
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
// CORE 0: Handles USB PTP Transmission ONLY
// ==========================================
void setup() {
    Serial.begin(115200);
    usb_hid.setPollInterval(2);
    usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    usb_hid.begin();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    // Notice: trackpad.begin() is moved to setup1() to isolate interrupts!
}

void loop() {
    // Read hardware FIFO from Core 1
    if (rp2040.fifo.available() > 0) {
        uint32_t report = rp2040.fifo.pop();
        
        // Unpack the data
        int8_t send_dx = report & 0xFF;
        int8_t send_dy = (report >> 8) & 0xFF;
        int8_t send_scroll = (report >> 16) & 0xFF;
        uint8_t buttons = (report >> 24) & 0xFF;
        
        if (usb_hid.ready()) {
            usb_hid.mouseReport(1, buttons, send_dx, send_dy, send_scroll, 0);
        }
    }
}

// ==========================================
// CORE 1: Handles PS/2 Reading & Display
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
    
    // Crucial fix: Initialize trackpad on Core 1! 
    // This attaches the PS/2 clock interrupt to Core 1, completely isolating it from USB interrupts.
    trackpad.begin();
}

void loop1() {
    static uint8_t packet[6];
    static int pkt_idx = 0;
    static unsigned long last_packet_time = 0;
    
    static bool last_finger_down = false;
    static int last_x = 0;
    static int last_y = 0;
    static uint8_t last_buttons = 0;
    static int last_fingers = -1;

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
            bool physical_left = packet[0] & 0x02; // Bit 1 is Left
            bool physical_right = packet[0] & 0x01; // Bit 0 is Right
            
            int x = packet[4] | ((packet[1] & 0x0F) << 8) | ((packet[3] & 0x10) << 8);
            int y = packet[5] | ((packet[1] & 0xF0) << 4) | ((packet[3] & 0x20) << 7);
            int z = packet[2];
            int w = ((packet[0] & 0x30) >> 2) | ((packet[0] & 0x04) >> 1) | ((packet[3] & 0x04) >> 2);
            
            uint8_t buttons = 0;
            if (physical_left || digitalRead(BUTTON_PIN) == LOW) buttons |= MOUSE_BUTTON_LEFT;
            if (physical_right) buttons |= MOUSE_BUTTON_RIGHT;
            
            bool is_touching = (z > 30) || (last_finger_down && z > 15);
            
            if (is_touching) {
                diag_x = x;
                diag_y = 5924 - y;
                diag_z = z;
                diag_w = w;
                
                int current_fingers = (w == 0) ? 2 : ((w == 1) ? 3 : 1);
                
                if (last_finger_down) {
                    int dx = 0;
                    int dy = 0;
                    static int history_x[2] = {0, 0};
                    static int history_y[2] = {0, 0};
                    
                    if (current_fingers == 1 && last_fingers == 1) {
                        dx = x - last_x;
                        dy = y - last_y; 
                    } else if (current_fingers == 2 && last_fingers == 2) {
                        // 2-Finger Alternating Coordinate Fix
                        dx = x - history_x[0];
                        dy = y - history_y[0];
                        
                        history_x[0] = history_x[1]; history_x[1] = x;
                        history_y[0] = history_y[1]; history_y[1] = y;
                    } else if (current_fingers == 2 && last_fingers != 2) {
                        // Just transitioned to 2 fingers, prime the history buffers
                        history_x[0] = x; history_x[1] = x;
                        history_y[0] = y; history_y[1] = y;
                    }
                    
                    // Native processing, NO glitch filters needed anymore!
                    static float dx_accumulator = 0.0;
                    static float dy_accumulator = 0.0;
                    static int scroll_accumulator = 0;
                    
                    int send_dx = 0;
                    int send_dy = 0;
                    int send_scroll = 0;
                    
                    if (current_fingers == 2 || current_fingers == 3) {
                        scroll_accumulator += (-dy); 
                        
                        int scroll_threshold = 120;
                        if (scroll_accumulator >= scroll_threshold) {
                            send_scroll = 1;
                            scroll_accumulator -= scroll_threshold;
                        } else if (scroll_accumulator <= -scroll_threshold) {
                            send_scroll = -1;
                            scroll_accumulator += scroll_threshold;
                        }
                        
                        dx_accumulator = 0.0;
                        dy_accumulator = 0.0;
                    } else if (current_fingers == 1) {
                        scroll_accumulator = 0; 
                        
                        dx_accumulator += (float)dx / 3.0f;
                        dy_accumulator += (float)(-dy) / 3.0f;
                        
                        send_dx = (int)dx_accumulator;
                        send_dy = (int)dy_accumulator;
                        
                        dx_accumulator -= send_dx;
                        dy_accumulator -= send_dy;
                        
                        send_dx = max(-127, min(127, send_dx));
                        send_dy = max(-127, min(127, send_dy));
                    }
                    
                    if (send_dx != 0 || send_dy != 0 || send_scroll != 0 || buttons != last_buttons) {
                        // Pack data into 32-bit integer for hardware FIFO
                        uint32_t report = (buttons << 24) | ((send_scroll & 0xFF) << 16) | ((send_dy & 0xFF) << 8) | (send_dx & 0xFF);
                        rp2040.fifo.push(report);
                    }
                }
                
                last_x = x;
                last_y = y;
                last_fingers = current_fingers;
                last_finger_down = true;
            } else {
                diag_z = 0;
                last_finger_down = false;
                last_fingers = -1;
                
                if (buttons != last_buttons) {
                    uint32_t report = (buttons << 24); // all zeroes for dx, dy, scroll
                    rp2040.fifo.push(report);
                }
            }
            
            last_buttons = buttons;
            pkt_idx = 0;
        }
    }
    
    // Display update (Running async on Core 1!)
    static unsigned long last_disp = 0;
    if (millis() - last_disp > 10) {
        last_disp = millis();
        fadeScreen();
        
        if (diag_z > 30) {
            int cx = map(diag_x, 1200, 5600, 0, 159);
            int cy = map(diag_y, 0, 5924, 0, 127); 
            
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
    }
}
