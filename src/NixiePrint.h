#include "Arduino.h"

class NixiePrint {
  public:
    void setup() {
        // BCDピン初期化（必ずLOWにクリア）
        for (uint8_t i = 0; i < 4; i++) {
            pinMode(BCD_PINS[i], OUTPUT);
            digitalWrite(BCD_PINS[i], LOW);
        }
        // 桁選択ピン初期化（全桁OFF）
        for (uint8_t i = 0; i < 6; i++) {
            pinMode(DIGIT_PINS[i], OUTPUT);
            writeDigitRaw(i, /*on=*/false);
        }
    }

    void print(uint32_t n) {
        // nを6桁に分割（LSB→MSB）
        uint8_t digits[6];
        for (uint8_t i = 0; i < 6; i++) {
            digits[i] = n % 10;  // 0も許容
            n /= 10;
        }

        for (uint8_t i = 0; i < 6; i++) {
            // ①全桁OFF → ②BCD更新 → ③対象桁ON
            disableAllDigits();
            writeBCD(digits[i]);
            Serial.printf("enablePin: %d\n", DIGIT_PINS[i]);
            enableDigit(i);

            delayMicroseconds(1500);

            // 次の桁に行く前にOFFしておく（残像/ゴースト抑制）
            disableAllDigits();
        }

        // 表示終了後はBCDもクリア
        clearBCD();
    }

  private:
    // ---- ハード依存の定義 ----
    const bool DIGIT_ACTIVE_HIGH = true;               // 桁選択がActive-Highならtrue, Active-Lowならfalse
    const uint8_t BCD_PINS[4]   = {19, 21, 22, 23};    // BCD A,B,C,D (LSB→MSB)
    const uint8_t DIGIT_PINS[6] = {0, 4, 16, 17, 5, 18};

    // ---- ユーティリティ ----
    inline void writeDigitRaw(uint8_t idx, bool on) {
        digitalWrite(DIGIT_PINS[idx], (on ^ !DIGIT_ACTIVE_HIGH) ? HIGH : LOW);
    }

    void enableDigit(uint8_t idx) {
        if (idx >= 6) return;
        writeDigitRaw(idx, true);
    }

    void disableDigit(uint8_t idx) {
        if (idx >= 6) return;
        writeDigitRaw(idx, false);
    }

    void disableAllDigits() {
        for (uint8_t i = 0; i < 6; i++) writeDigitRaw(i, false);
    }

    void clearBCD() {
        for (uint8_t i = 0; i < 4; i++) digitalWrite(BCD_PINS[i], LOW);
    }

    // 0〜9をBCDで出力（常に全bitを書き換える）
    void writeBCD(uint8_t n) {
        if (n > 9) return;  // 0〜9のみ
        // 先にクリアしてから書くと“0にならない”問題を防ぎやすい
        // （配線やドライバによっては瞬間的な混線を防ぐのに有効）
        clearBCD();

        for (uint8_t i = 0; i < 4; i++) {
            uint8_t bitVal = (n >> i) & 0x1;   // LSBから
            digitalWrite(BCD_PINS[i], bitVal ? HIGH : LOW);
        }
    }
};
