#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include "ntp.h"
#include "secret.h"
#include "NixiePrint.h"

NixiePrint NixiePrint;

NTP ntp(WIFI_SSID, WIFI_PASSWORD);

uint32_t makeHHMMSS(const struct tm& t) {
    uint32_t hh = t.tm_hour;  // 時
    uint32_t mm = t.tm_min;   // 分
    uint32_t ss = t.tm_sec;   // 秒
    return hh * 10000 + mm * 100 + ss; // hhmmss
}

void setup() {
  Serial.begin(115200);
  NixiePrint.setup();
  ntp.setSyncIntervalSeconds(120);
  NixiePrint.splashRandom(1500 /*ms*/, 4 /*周回*/, 8 /*ms*/);

  // 起動直後に一度同期
  if (ntp.begin()) {
    Serial.println("Initial NTP sync OK");
  } else {
    Serial.println("Initial NTP sync FAILED");
  }
}

void loop() {
  // 毎ループで呼んでOK。必要なときだけWi-Fiを上げて再同期する
  ntp.update();

  // 時刻の利用例
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    struct tm now{};
    if (ntp.getTime(&now)) {
      uint32_t hhmmss = makeHHMMSS(now);
      Serial.printf("HHMMSS: %06d\n", hhmmss);
      NixiePrint.print(hhmmss);
    }
  }

  delayMicroseconds(1500);
}
