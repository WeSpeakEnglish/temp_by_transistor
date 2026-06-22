#include <Arduino.h>

#define OW_PIN 2

void    ow_low()  { pinMode(OW_PIN, OUTPUT); digitalWrite(OW_PIN, LOW); }
void    ow_rel()  { pinMode(OW_PIN, INPUT); }
int     ow_rd()   { return digitalRead(OW_PIN); }

bool ow_reset() {
    ow_low(); delayMicroseconds(480);
    ow_rel(); delayMicroseconds(70);
    bool p = !ow_rd();
    delayMicroseconds(410);
    return p;
}

void ow_write_bit(uint8_t b) {
    ow_low(); delayMicroseconds(b ? 6 : 60);
    ow_rel(); delayMicroseconds(b ? 64 : 10);
}

uint8_t ow_read_bit() {
    ow_low(); delayMicroseconds(6);
    ow_rel(); delayMicroseconds(9);
    uint8_t b = ow_rd();
    delayMicroseconds(55);
    return b;
}

void    ow_write(uint8_t v)  { for(int i=0;i<8;i++) ow_write_bit((v>>i)&1); }
uint8_t ow_read_byte()       { uint8_t v=0; for(int i=0;i<8;i++) v|=ow_read_bit()<<i; return v; }

uint8_t crc8(uint8_t *d, uint8_t len) {
    uint8_t crc=0;
    for(uint8_t i=0;i<len;i++) {
        uint8_t b=d[i];
        for(uint8_t j=0;j<8;j++) { uint8_t m=(crc^b)&1; crc>>=1; if(m) crc^=0x8C; b>>=1; }
    }
    return crc;
}

void print_hex(uint8_t *d, uint8_t len) {
    for(int i=0;i<len;i++) { if(d[i]<0x10) Serial.print("0"); Serial.print(d[i],HEX); if(i<len-1) Serial.print(" "); }
}

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(10);
    pinMode(OW_PIN, INPUT);
    Serial.println("DS18B20 — D2");
    Serial.println("============");
}

void loop() {
    uint8_t rom[8], scratch[9];

    Serial.print("\n[RESET] ");
    if(!ow_reset()) { Serial.println("NO ✗"); delay(2000); return; }
    Serial.println("YES ✓");

    ow_write(0x33);
    for(int i=0;i<8;i++) rom[i]=ow_read_byte();
    Serial.print("[ROM]   "); print_hex(rom,8);
    Serial.println(crc8(rom,8)==0 ? (rom[0]==0x28 ? "  DS18B20 ✓" : "  UNKNOWN") : "  CRC FAIL ✗");

    ow_reset();
    ow_write(0xCC); ow_write(0x44);
    pinMode(OW_PIN, OUTPUT); digitalWrite(OW_PIN, HIGH);
    delay(750);
    ow_rel();

    ow_reset();
    ow_write(0xCC); ow_write(0xBE);
    for(int i=0;i<9;i++) scratch[i]=ow_read_byte();
    Serial.print("[PAD]   "); print_hex(scratch,9);

    if(crc8(scratch,9)!=0) { Serial.println("  CRC FAIL ✗"); delay(2000); return; }

    int16_t raw = (scratch[1]<<8)|scratch[0];
    Serial.print("  OK ✓\n[TEMP]  ");
    Serial.print(raw/16.0f, 2);
    Serial.println(" °C");
    Serial.println("------------");
    delay(2000);
}