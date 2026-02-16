#include "wifiportal.h"

WifiPortal::WifiPortal(const Config& cfg) : cfg_(cfg) {};

String WifiPortal::saveSSID() const {
    // Preferences is not const friendly; expose a method instead of direct access.
    // We'll reopen read-only if you want, but simplist is: call after begin() and cache externally.
    return "";
}

bool WifiPortal::connectSta_(const String& ssid, const String& pass) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(cfg_.hostname);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);

    Serial.printf("Connecting to Wifi SSID: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < cfg_.staConnectTimeoutMs) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wifi connected!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString());
        
        return true;
    }

    Serial.println("Wifi connect failed!");

    return false;
}

void WifiPortal::startApPortal_() {
    mode_ = Mode::AP;

    Serial.println("Starting fallback AP and captive portal...");
    WiFi.mode(WIFI_AP);

    // Configure AP IP/subnet
    WiFi.softAPConfig(cfg_.apIP, cfg_.apGW, cfg_.apMask);

    bool apOk = WiFi.softAP(cfg_.apSSID, cfg_.apPass);
    Serial.printf("AP: %s. AP IP: %s",
        apOk ? "started" : "failed",
        WiFi.softAPIP().toString().c_str()
    );

    // Captive DNS: resolve all hostnames to AP IP
    dns_.start(53, "*", WiFi.softAPIP());
}

void WifiPortal::registerCommonRoutes_() {
    // Wifi config form
    server_.on("/wifi", HTTP_GET, [this]() {handleWifiForm_(); });

    // Save (POST)
    server_.on("/save", HTTP_POST, [this]() {handleSave_(); });
}

void WifiPortal::registerApOnlyRoutes_() {
    // Portal root : status + link to config
    server_.on("/", HTTP_GET, [this]() {handlePortalRoot_(); });

    // Helpful: redirect unknown routes to /wifi in AP mode
    server_.onNotFound([this]() {
        server_.sendHeader("Location", "/wifi", true);
        server_.send(302, "text/plain", "");
    });
}

void WifiPortal::handlePortalRoot_() {
    String s;
    s += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>ESP32 Setup</title>";
    s += "<style>body{font-family:system-ui;margin:24px} .card{max-width:520px;padding:18px;border:1px solid #ddd;border-radius:14px}";
    s += "a{display:inline-block;margin-top:10px}</style></head><body><div class='card'>";
    s += "<h3>ESP32 Setup</h3>";

    if (mode_ == Mode::AP) {
        s += "<p><b>Mode:</b> AP (setup)</p>";
        s += "<p><b>AP SSID:</b> ";
        s += cfg_.apSSID;
        s += "</p>";
        s += "<p><b>AP IP:</b> ";
        s += WiFi.softAPIP().toString();
        s += "</p>";
        s += "<a href='/wifi'>Configure Wi-Fi</a>";
    } else {
        s += "<p><b>Mode:</b> STA</p>";
        s += "<p><b>IP:</b> ";
        s += WiFi.localIP().toString();
        s += "</p>";
        s += "<a href='/wifi'>Change Wi-Fi</a>";
    }

    s += "</div></body></html>";
    server_.sendHeader("Connection", "close");
    server_.send(200, "text/html", s);
}

void WifiPortal::handleWifiForm_() {
    String saved = prefs_.getString(cfg_.keySSID, "");

    String s;
    s.reserve(1200);
    s += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>Configure Wi-Fi</title>";
    s += "<style>body{font-family:system-ui;margin:24px} .card{max-width:520px;padding:18px;border:1px solid #ddd;border-radius:14px}";
    s += "input{width:100%;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:10px} ";
    s += "button{padding:10px 14px;border:0;border-radius:10px}</style>";
    s += "</head><body><div class='card'>";
    s += "<h3>Configure Wi-Fi</h3>";

    if (saved.length()) {
        s += "<p>Saved SSID: <b>";
        s += saved;
        s += "</b></p>";
    }

    s += "<form method='POST' action='/save'>";
    s += "<label>SSID</label><input name='ssid' required>";
    s += "<label>Password</label><input name='pass' type='password' required>";
    s += "<button type='submit'>Save &amp; Reboot</button>";
    s += "</form>";
    s += "<p style='color:#666'>After reboot, the device will try to join your Wi-Fi.</p>";
    s += "</div></body></html>";

    server_.sendHeader("Connection", "close");
    server_.send(200, "text/html", s);
}

void WifiPortal::handleSave_() {
    if (!server_.hasArg("ssid") || !server_.hasArg("pass")) {
        server_.send(400, "text/plain", "Missing SSID or pass");
        return;
    }

    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    ssid.trim();

    if (ssid.length() == 0) {
        server_.send(400, "text/plain", "SSID empty");
        return;
    }

    if (pass.length() < 8) {
        server_.send(400, "text/plain", "pass must be at least 8 chars");
        return;
    }

    prefs_.putString(cfg_.keySSID, ssid);
    prefs_.putString(cfg_.keyPass, pass);

    server_.sendHeader("Connection", "close");
    server_.send(200, "text/html", "<html><body><p>Saved. Rebooting...</p></body></html>");

    delay(600);
    ESP.restart();
}

WifiPortal::Mode WifiPortal::begin() {
    prefs_.begin(cfg_.nvsNamespace, false);

    // Portal routes always available (even if in STA if you want "change wifi".
    registerCommonRoutes_();

    // Try saving creds.
    String ssid = prefs_.getString(cfg_.keySSID, "");
    String pass = prefs_.getString(cfg_.keyPass, "");

    if (ssid.length() && connectSta_(ssid, pass)) {
        mode_ = Mode::STA;
        server_.begin();
        Serial.println("HTTP Server started (STA)");
        return mode_;
    }

    // AP Provisioning mode
    startApPortal_();
    registerApOnlyRoutes_();
    server_.begin();
    Serial.println("HTTP Server started (AP portal)");
    return mode_;
}

void WifiPortal::loop() {
    server_.handleClient();
    if (mode_ == Mode::AP) {
        dns_.processNextRequest();
    }
}