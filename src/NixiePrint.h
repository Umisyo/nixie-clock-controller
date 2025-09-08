#include "Arduino.h"
#ifdef ESP32
  #include "esp_system.h" // esp_random()
#endif

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

        // 乱数の種
        #ifdef ESP32
          uint32_t seed = esp_random();
          randomSeed(seed);
        #else
          // 浮遊ピンが使えるならA0など読むと良い
          randomSeed(analogRead(A0));
        #endif
    }

    // ★ 起動演出：ランダム6桁をバーッと表示
    //   duration_ms: 演出時間
    //   cycles_per_frame: 1つの6桁値を何回printするか（残光/明るさのため）
    //   frame_delay_ms: フレーム間の待ち
    void splashRandom(uint32_t duration_ms = 1500,
                      uint16_t cycles_per_frame = 4,
                      uint16_t frame_delay_ms = 8) {
        uint32_t start = millis();
        while (millis() - start < duration_ms) {
            uint32_t val = random6Digits_();  // 000000〜999999 を作る
            // 同じ値を数回走査して明るさ＆視認性を確保
            for (uint16_t c = 0; c < cycles_per_frame; ++c) {
                print(val);
            }
            delay(frame_delay_ms);
        }
        // 終了時に一瞬真っ黒にしたいなら下記を有効化
        // disableAllDigits(); clearBCD();
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
            enableDigit(i);

            delayMicroseconds(1500);  // 点灯時間（必要なら調整）

            // 次の桁に行く前にOFF（残像/ゴースト抑制）
            disableAllDigits();
        }

        // 表示終了後はBCDもクリア（不要なら消してOK）
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
        clearBCD();
        for (uint8_t i = 0; i < 4; i++) {
            uint8_t bitVal = (n >> i) & 0x1;   // LSBから
            digitalWrite(BCD_PINS[i], bitVal ? HIGH : LOW);
        }
    }

    // 0〜999999 の6桁ランダム値（先頭0あり得る）
    uint32_t random6Digits_() {
        // 6桁それぞれ 0-9 をランダムに生成して結合
        uint32_t v = 0;
        for (uint8_t i = 0; i < 6; ++i) {
            v = v * 10 + (uint32_t)random(0, 10);
        }
        return v;
    }
};
