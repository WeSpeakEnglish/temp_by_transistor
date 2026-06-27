#include <Arduino.h>

#define OW_PIN 2
#define D0_PIN 0
#define A1_PIN A1

#define SAMPLE_INTERVAL_MS 200
#define PRINT_INTERVAL_MS 10000  // 10 sec

// Bus control
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

// Accumulators
float voltageAccum = 0;
float tempAccum = 0;
int sampleCount = 0;
unsigned long startTime = 0;
unsigned long nextPrintTime = 0;

float readTemperature() {
    uint8_t rom[8], scratch[9];

    if(!ow_reset()) return NAN;

    ow_write(0x33);
    for(int i=0;i<8;i++) rom[i]=ow_read_byte();
    if(crc8(rom,8) != 0 || rom[0] != 0x28) return NAN;

    ow_reset();
    ow_write(0xCC); ow_write(0x44);
    pinMode(OW_PIN, OUTPUT); digitalWrite(OW_PIN, HIGH);
    delay(750);
    ow_rel();

    ow_reset();
    ow_write(0xCC); ow_write(0xBE);
    for(int i=0;i<9;i++) scratch[i]=ow_read_byte();
    if(crc8(scratch,9) != 0) return NAN;

    int16_t raw = (scratch[1]<<8)|scratch[0];
    return raw / 16.0f;
}

void printTime(unsigned long elapsedSec) {
    unsigned int minutes = elapsedSec / 60;
    unsigned int seconds = elapsedSec % 60;
    if (minutes < 10) Serial.print("0");
    Serial.print(minutes);
    Serial.print(":");
    if (seconds < 10) Serial.print("0");
    Serial.print(seconds);
}

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(10);
    pinMode(OW_PIN, INPUT);

    pinMode(D0_PIN, OUTPUT);
    digitalWrite(D0_PIN, LOW);

    // XIAO SAMD21: MUST set 12-bit explicitly — default is 10-bit!
    analogReadResolution(12);
    analogReference(AR_INTERNAL1V0);  // 1.0V internal reference

    startTime = millis();
    nextPrintTime = startTime + PRINT_INTERVAL_MS;
    nextPrintTime = ((nextPrintTime + PRINT_INTERVAL_MS - 1) / PRINT_INTERVAL_MS) * PRINT_INTERVAL_MS;

    Serial.println("Time, Voltage[mV], Temperature[°C], Estimated Temperature [°C]");
}

void loop() {
    unsigned long now = millis();

    // Sample every 200 ms
    static unsigned long lastSampleTime = 0;
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = now;

        // Read voltage: 1.0V ref, 12-bit ADC (0–4095)
        int a1_raw = analogRead(A1_PIN);
        float voltage = a1_raw * (1000.0 / 4095.0);
        voltageAccum += voltage;

        // Read temperature
        float temp = readTemperature();
        if (!isnan(temp)) {
            tempAccum += temp;
        }

        sampleCount++;
    }

    // Print every 10 seconds
    if (now >= nextPrintTime && sampleCount > 0) {

        float avgVoltage = voltageAccum / sampleCount;
        float avgTemp = tempAccum / sampleCount;
        unsigned long elapsedSec = (nextPrintTime - startTime) / 1000;

        printTime(elapsedSec);
        Serial.print(",");
        Serial.print(avgVoltage, 2);
        Serial.print(",");
        Serial.print(avgTemp, 2);
        Serial.print(",");
        Serial.println(-0.4558 * avgVoltage + 288.49, 2);

        voltageAccum = 0;
        tempAccum = 0;
        sampleCount = 0;
        nextPrintTime += PRINT_INTERVAL_MS;
    }
}

