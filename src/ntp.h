#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "secret.h"

class NTP {
  private:
    const char*    SSID;
    const char*    PASS;
    const char*    NTP_SERVER_1    = "ntp.nict.jp";
    const char*    NTP_SERVER_2    = "time.google.com";
    const char*    NTP_SERVER_3    = "ntp.jst.mfeed.ad.jp";
    const uint32_t GMT_OFFSET      = 3600 * 9;  // JST
    const uint8_t  DAYLIGHT_OFFSET = 0;

    const uint8_t  CONN_TRY_TIMES  = 10;   // WiFi接続の試行回数
    const uint16_t CONN_TRY_INTVL  = 500;  // 試行間隔[ms]

    uint32_t       SYNC_INTERVAL_SEC = 3600; // 再同期間隔（秒）
    time_t         last_sync_epoch_ = 0;     // 直近同期のepoch秒
    bool           is_configured    = false; // ひとまず最初の同期が完了したか

    bool connectWiFi_() {
      WiFi.mode(WIFI_STA);
      WiFi.begin(SSID, PASS);
      for (uint8_t i = 0; i < CONN_TRY_TIMES; ++i) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(CONN_TRY_INTVL);
      }
      return false;
    }

    void disconnectWiFi_() {
      WiFi.disconnect(true, true);  // APから切断 + ステーション停止
      WiFi.mode(WIFI_OFF);          // 省電力
    }

    bool syncTimeOnce_() {
      // NTP設定（これでSNTPクライアントが起動し時刻を取得）
      configTime(GMT_OFFSET, DAYLIGHT_OFFSET, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

      // 取得完了待ち（最大 ~3秒程度で十分なことが多い）
      tm timeinfo{};
      const uint32_t deadline = millis() + 3000;
      while (millis() < deadline) {
        if (getLocalTime(&timeinfo, 200 /*ms*/)) {
          last_sync_epoch_ = mktime(&timeinfo);
          is_configured = true;
          return true;
        }
      }
      return false;
    }

    bool connectAndSync_() {
      if (!connectWiFi_()) return false;
      Serial.println("Syncing time...");
      bool ok = syncTimeOnce_();
      disconnectWiFi_();
      return ok;
    }

  public:
    NTP(const char* _SSID = WIFI_SSID, const char* _PASS = WIFI_PASSWORD)
      : SSID(_SSID), PASS(_PASS) {}

    // 最初の同期（起動時などに1回呼ぶ）
    bool begin() {
      return connectAndSync_();
    }

    // ループ内から定期的に呼ぶと、必要に応じて1時間おきに再同期
    void update() {
      if (!is_configured) {
        // まだ一度も同期できていなければ試す
        connectAndSync_();
        return;
      }

      // RTCカウント中：必要時間経過したら再同期
      time_t now_epoch = time(nullptr);
      if (now_epoch == 0) {
        // 稀に未初期化に見える場合は再同期を試みる
        connectAndSync_();
        return;
      }

      if ((uint32_t)(now_epoch - last_sync_epoch_) >= SYNC_INTERVAL_SEC) {
        connectAndSync_();
      }
    }

    // 任意タイミングで現在時刻を取り出し（秒境界に合わせる版）
    bool getTimeOnSecond(struct tm* tm_out) {
      if (!tm_out) return false;
      if (!getLocalTime(tm_out)) return false;

      uint8_t prev = tm_out->tm_sec;
      // 次の秒に変わるのを待つ（最大1.2秒程度で抜ける）
      uint32_t deadline = millis() + 1200;
      do {
        if (!getLocalTime(tm_out, 50)) return false;
      } while (tm_out->tm_sec == prev && millis() < deadline);

      return true;
    }

    // 秒境界にこだわらない現在時刻
    bool getTime(struct tm* tm_out) {
      if (!tm_out) return false;
      return getLocalTime(tm_out);
    }

    void setSyncIntervalSeconds(uint32_t sec) { SYNC_INTERVAL_SEC = sec; }
    uint32_t getSyncIntervalSeconds() const { return SYNC_INTERVAL_SEC; }

    bool getIsConfigured() const { return is_configured; }
    time_t getLastSyncEpoch() const { return last_sync_epoch_; }
};
