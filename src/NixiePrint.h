#pragma once
#include "Arduino.h"
#ifdef ESP32
  #include "esp_system.h" // esp_random()
  #include "freertos/FreeRTOS.h"
  #include "freertos/portmacro.h"
#endif

class NixiePrint {
  public:
    // ---- ハード依存（配列の実体は NixiePrint.cpp に定義）----
    static constexpr bool DIGIT_ACTIVE_HIGH = true; // ←実機により true/false 調整
    static const uint8_t BCD_PINS[4];
    static const uint8_t DIGIT_PINS[6];

    // タイミング（µs）
    struct Timings {
      uint16_t BLANK_US  = 220; // ←強めのブランキング（必要なら 300〜400 も）
      uint16_t SETTLE_US = 8;   // BCD安定待ち
      uint16_t ON_US     = 1800;// 明るさ（総周波数が 70〜120Hz になる範囲で調整）
    };

    void setup() {
      for (uint8_t i = 0; i < 4; i++) pinMode(BCD_PINS[i], OUTPUT);
      for (uint8_t i = 0; i < 6; i++) pinMode(DIGIT_PINS[i], OUTPUT);
      disableAllDigits();
      blankBCD();

      #ifdef ESP32
        randomSeed(esp_random());
      #else
        randomSeed(analogRead(A0));
      #endif
    }

    // 表示値セット（例: HHMMSS）
    void setNumber(uint32_t n) {
      for (uint8_t i = 0; i < 6; i++) { buf_[i] = n % 10; n /= 10; }
    }

    // 高頻度で呼ぶ・非ブロッキング走査
    void refresh() {
      const uint32_t now = micros();
      switch (stage_) {
        case STAGE_BLANK: {
        if (!didBlank_) {
            CRIT_ENTER();
            disableAllDigits();
            // まず必ず空白に（0xF）
            blankBCD();
            CRIT_EXIT();
            didBlank_ = true;
            stageStartUs_ = now;
        } else if (elapsedUs(now, stageStartUs_) >= t_.BLANK_US) {
            // 空白のまま一瞬置いてから目的BCDへ
            // （BLANK_USを短め、SETTLE_USを長めに）
            writeBCD(buf_[cur_]);
            stage_ = STAGE_SETTLE;
            stageStartUs_ = now;
        }
        } break;

        case STAGE_SETTLE: {
          if (elapsedUs(now, stageStartUs_) >= t_.SETTLE_US) {
            // 目的BCDが安定したら桁ON（ここも原子的に）
            CRIT_ENTER();
            enableDigit(cur_);
            CRIT_EXIT();
            stage_ = STAGE_ON;
            stageStartUs_ = now;
          }
        } break;

        case STAGE_ON: {
          if (elapsedUs(now, stageStartUs_) >= t_.ON_US) {
            // 次桁へ：まず確実にOFF（原子的に）
            CRIT_ENTER();
            disableAllDigits();
            CRIT_EXIT();
            cur_   = (cur_ + 1) % 6;
            stage_ = STAGE_BLANK;
            didBlank_ = false;
            stageStartUs_ = now;
          }
        } break;
      }
    }

    void setTimings(const Timings& tt) { t_ = tt; }
    Timings timings() const { return t_; }

    // 起動演出（走査型）
    void splashRandom(uint32_t duration_ms = 1500, uint16_t frame_ms = 40) {
      uint32_t until = millis() + duration_ms;
      uint32_t nextFrame = 0;
      while ((int32_t)(millis() - (int32_t)until) < 0) {
        if ((int32_t)(millis() - (int32_t)nextFrame) >= 0) {
          nextFrame = millis() + frame_ms;
          setNumber(random6Digits_());
        }
        refresh();
      }
      CRIT_ENTER();
      disableAllDigits();
      blankBCD();
      CRIT_EXIT();
    }

    // 1周ブロッキング描画（デバッグ用）
    void printBlocking(uint32_t n) {
      uint8_t digits[6];
      for (uint8_t i = 0; i < 6; i++) { digits[i] = n % 10; n /= 10; }

      for (uint8_t i = 0; i < 6; i++) {
        CRIT_ENTER();
        disableAllDigits();
        blankBCD();
        CRIT_EXIT();
        delayMicroseconds(t_.BLANK_US);

        writeBCD(digits[i]);
        delayMicroseconds(t_.SETTLE_US);

        CRIT_ENTER();
        enableDigit(i);
        CRIT_EXIT();
        delayMicroseconds(t_.ON_US);
      }
      CRIT_ENTER();
      disableAllDigits();
      blankBCD();
      CRIT_EXIT();
    }

    // 全桁同時点灯（短時間テスト）
    void testAllDigits(uint8_t bcd_value = 8, uint16_t hold_ms = 300) {
      CRIT_ENTER();
      disableAllDigits();
      writeBCD(bcd_value);
      for (uint8_t i = 0; i < 6; ++i) enableDigit(i);
      CRIT_EXIT();
      delay(hold_ms);
      CRIT_ENTER();
      disableAllDigits();
      blankBCD();
      CRIT_EXIT();
    }

    // ★極性チェック：1桁ずつ順番にON。期待通り1本ずつ光かない場合は DIGIT_ACTIVE_HIGH を反転
    void polaritySelfTest(uint16_t hold_ms = 250) {
      writeBCD(8);
      for (uint8_t i = 0; i < 6; ++i) {
        CRIT_ENTER();
        disableAllDigits();
        enableDigit(i);
        CRIT_EXIT();
        delay(hold_ms);
      }
      CRIT_ENTER();
      disableAllDigits();
      blankBCD();
      CRIT_EXIT();
    }

  private:
    enum Stage : uint8_t { STAGE_BLANK, STAGE_SETTLE, STAGE_ON };
    volatile Stage    stage_        = STAGE_BLANK;
    volatile uint8_t  cur_          = 0;
    volatile bool     didBlank_     = false;
    volatile uint32_t stageStartUs_ = 0;
    uint8_t           buf_[6]       = {0};
    Timings           t_;

    // クリティカルセクション（ESP32はミューテックス、他は割り込み禁止）
    #ifdef ESP32
      portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
      inline void CRIT_ENTER() { portENTER_CRITICAL(&mux_); }
      inline void CRIT_EXIT()  { portEXIT_CRITICAL(&mux_);  }
    #else
      inline void CRIT_ENTER() { noInterrupts(); }
      inline void CRIT_EXIT()  { interrupts();   }
    #endif

    static inline uint32_t elapsedUs(uint32_t now, uint32_t since) {
      return (uint32_t)((int32_t)(now - since));
    }

    inline void writeDigitRaw(uint8_t idx, bool on) {
      if (idx >= 6) return;
      if (DIGIT_ACTIVE_HIGH) digitalWrite(DIGIT_PINS[idx], on ? HIGH : LOW);
      else                   digitalWrite(DIGIT_PINS[idx], on ? LOW  : HIGH);
    }
    void enableDigit(uint8_t idx) { writeDigitRaw(idx, true); }
    void disableDigit(uint8_t idx){ writeDigitRaw(idx, false); }
    void disableAllDigits() { for (uint8_t i = 0; i < 6; i++) writeDigitRaw(i, false); }

    // アイドルは必ずブランク(0xF=1111)
    void blankBCD() { for (uint8_t i = 0; i < 4; i++) digitalWrite(BCD_PINS[i], HIGH); }

    // 0〜9をBCDで出力（桁はOFF/ブランク中に呼ぶこと）
    void writeBCD(uint8_t n) {
      if (n > 9) return;
      for (uint8_t i = 0; i < 4; i++) {
        const uint8_t bitVal = (n >> i) & 0x1;
        digitalWrite(BCD_PINS[i], bitVal ? HIGH : LOW);
      }
    }

    uint32_t random6Digits_() {
      uint32_t v = 0;
      for (uint8_t i = 0; i < 6; ++i) v = v * 10 + (uint32_t)random(0, 10);
      return v;
    }
};
