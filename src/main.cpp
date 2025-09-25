#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "ntp.h"
#include "secret.h"
#include "NixiePrint.h"

NixiePrint nix;
NTP ntp(WIFI_SSID, WIFI_PASSWORD);

uint32_t makeHHMMSS(const struct tm& t) {
  uint32_t hh = t.tm_hour;
  uint32_t mm = t.tm_min;
  uint32_t ss = t.tm_sec;
  return hh * 10000 + mm * 100 + ss;
}

void setup() {
  Serial.begin(115200);
  nix.setup();

  // ★まず極性チェック（必要なら一度だけ実行）
  // nix.polaritySelfTest(250); while(true) { nix.refresh(); }

  // 起動演出（任意）
  nix.splashRandom(1500, 40);

  ntp.setSyncIntervalSeconds(3600);
  if (ntp.begin()) Serial.println("Initial NTP sync OK");
  else             Serial.println("Initial NTP sync FAILED");

  nix.setNumber(0);

  // さらにゴーストが残る場合はここを増やす（例：300〜350）
  auto tt = nix.timings();
  tt.BLANK_US  = 800;   // ←強化
  tt.SETTLE_US = 40;
  tt.ON_US     = 1700;  // 必要に応じ 1500〜2500 で調整
  nix.setTimings(tt);
}

void loop() {
  ntp.update();

  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    struct tm now{};
    if (ntp.getTime(&now)) {
      uint32_t hhmmss = makeHHMMSS(now);
      nix.setNumber(hhmmss);
      Serial.printf("HHMMSS: %06u\n", hhmmss);
    }
  }

  // 常時走査
  nix.refresh();
}
