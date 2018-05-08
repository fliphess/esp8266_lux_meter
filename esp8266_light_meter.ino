#include <FS.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>

// * Include settings
#include "settings.h"

// * Include PubSubClient after setting the correct packet size
#include <PubSubClient.h>

// * Initiate led blinker library
Ticker ticker;

// * Initiate Watchdog
Ticker tickerOSWatch;

// * Initiate WIFI client
WiFiClient espClient;

// * Initiate MQTT client
PubSubClient mqtt_client(espClient);

// * Initiate display
Adafruit_SSD1306 display(OLED_RESET);

// * Initiate lux meter
BH1750 lux_meter(LUX_METER_I2C_ADDR);

// **********************************
// * System Functions               *
// **********************************

// * Watchdog function
void ICACHE_RAM_ATTR osWatch(void)
{
    unsigned long t = millis();
    unsigned long last_run = abs(t - last_loop);
    if (last_run >= (OSWATCH_RESET_TIME * 1000)) {
        ESP.restart();
    }
}

// * Gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println(F("Entered config mode"));
    Serial.println(WiFi.softAPIP());

    // * If you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());

    // * Entered config mode, make led toggle faster
    ticker.attach(0.2, tick);
}

// * Callback notifying us of the need to save config
void save_wifi_config_callback ()
{
    Serial.println("Setting should save flag to true");
    shouldSaveConfig = true;
}

// * Blink on-board Led
void tick()
{
    // * Toggle state
    int state = digitalRead(LED_BUILTIN);    // * Get the current state of GPIO1 pin
    digitalWrite(LED_BUILTIN, !state);       // * Set pin to the opposite state
}

// **********************************
// * Display Functions              *
// **********************************

// * Update the display update timer
void set_display_updated()
{
    DISPLAY_LAST_UPDATE = millis();
    DISPLAY_SHOULD_UPDATE = false;
}

// * Force a display update
void set_display_should_update()
{
    DISPLAY_SHOULD_UPDATE = true;
}

// * Returns true if the display should update, otherwise false
bool display_should_update()
{
    return DISPLAY_SHOULD_UPDATE || (millis() - DISPLAY_LAST_UPDATE >= DISPLAY_UPDATE_FREQUENCY);
}

// * Setup the display: Print rounded square and go to position 0,0
void setup_display()
{
  display.setTextColor(WHITE);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 0);
  display.clearDisplay();
  display.drawRoundRect(2, 2, display.width() - 2 * 2, display.height() - 2 * 2, display.height() / 4, WHITE);
}

// * Print a 2 lines status message to the display
void display_message(const char* message)
{
    setup_display();
    display.setTextSize(1);
    display.println("");
    display.println("     " + String(message));
    display.println("");
    display.display();
    set_display_updated();
}

void update_display()
{
    setup_display();
    display.setTextSize(1);
    display.println("");
    display.println("     " + String(LUX_METER_LAST_VALUE) + " lux");
    display.println("");
    display.display();
    set_display_updated();
}

// **********************************
// * MQTT                           *
// **********************************

// * Reconnect to MQTT server and subscribe to in and out topics
bool mqtt_reconnect()
{
    // * Loop until we're reconnected
    int MQTT_RECONNECT_RETRIES = 0;

    display_message("Connecting.");

    while (!mqtt_client.connected() && MQTT_RECONNECT_RETRIES < MQTT_MAX_RECONNECT_TRIES) {
        MQTT_RECONNECT_RETRIES++;

        Serial.printf("MQTT connection attempt %d / %d ...\n", MQTT_RECONNECT_RETRIES, MQTT_MAX_RECONNECT_TRIES);

        // * Attempt to connect
        if (mqtt_client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
            Serial.println(F("MQTT connected!"));

            // * Once connected, publish an announcement...
            char * message = new char[20 + strlen(HOSTNAME) + 1];
            strcpy(message, "Lux meter alive: ");
            strcat(message, HOSTNAME);
            mqtt_client.publish("hass/status", message);

            Serial.printf("MQTT topic: %s\n", MQTT_SENSOR_CHANNEL);

            // * Set display to "System Online"
            display_message("Online.");
        }
        else {
            // * Set display to "System Offline"
            display_message("Offline.");

            Serial.print(F("MQTT Connection failed: rc="));
            Serial.println(mqtt_client.state());
            Serial.println(F(" Retrying in 5 seconds"));
            Serial.println("");

            // * Wait 5 seconds before retrying
            delay(5000);
        }
    }

    if (MQTT_RECONNECT_RETRIES >= MQTT_MAX_RECONNECT_TRIES) {
        Serial.printf("*** MQTT connection failed, giving up after %d tries ...\n", MQTT_RECONNECT_RETRIES);
        return false;
    }

    return true;
}

// **********************************
// * EEPROM & OTA helpers           *
// **********************************

// * Read mqtt credentials from eeprom
String read_eeprom(int offset, int len)
{
    String res = "";

    for (int i = 0; i < len; ++i) {
        res += char(EEPROM.read(i + offset));
    }
    Serial.print(F("read_eeprom(): "));
    Serial.println(res.c_str());

    return res;
}

// * Write the mqtt credentials to eeprom
void write_eeprom(int offset, int len, String value)
{
    Serial.print(F("write_eeprom(): "));
    Serial.println(value.c_str());

    for (int i = 0; i < len; ++i) {
        if ((unsigned)i < value.length()) {
            EEPROM.write(i + offset, value[i]);
        } else {
            EEPROM.write(i + offset, 0);
        }
    }
}

// * Setup update over the air
void setup_ota()
{
    Serial.println(F("Arduino OTA activated."));

    // * Port defaults to 8266
    ArduinoOTA.setPort(8266);

    // * Set hostname for OTA
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        Serial.println(F("Arduino OTA: Start"));
    });

    ArduinoOTA.onEnd([]() {
        Serial.println(F("Arduino OTA: End (Running reboot)"));
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Arduino OTA Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Arduino OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println(F("Arduino OTA: Auth Failed"));
        else if (error == OTA_BEGIN_ERROR)
            Serial.println(F("Arduino OTA: Begin Failed"));
        else if (error == OTA_CONNECT_ERROR)
            Serial.println(F("Arduino OTA: Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println(F("Arduino OTA: Receive Failed"));
        else if (error == OTA_END_ERROR)
            Serial.println(F("Arduino OTA: End Failed"));
    });
    ArduinoOTA.begin();
    Serial.println(F("Arduino OTA finished"));
}

// **********************************
// * Setup                          *
// **********************************

void setup()
{
    // * Configure Watchdog
    last_loop = millis();
    tickerOSWatch.attach_ms(((OSWATCH_RESET_TIME / 3) * 1000), osWatch);

    // * Configure Serial and EEPROM
    Serial.begin(BAUD_RATE);

    // * Initiate EEPROM
    EEPROM.begin(512);

    // * Initiate Wire library for the bh1750
    Wire.begin();

    // * Initiate Lux Meter
    lux_meter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

    // * Setup display
    display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_I2C_ADDR);
    display.clearDisplay();

    // * Set led pin as output
    pinMode(LED_BUILTIN, OUTPUT);

    // * Start ticker with 0.5 because we start in AP mode and try to connect
    ticker.attach(0.6, tick);

    // * Set display to "System Booting"
    display_message("Booting.");

    // * Get MQTT Server settings
    String settings_available = read_eeprom(134, 1);

    if (settings_available == "1") {
        read_eeprom(0, 64).toCharArray(MQTT_HOST, 64);   // * 0-63
        read_eeprom(64, 6).toCharArray(MQTT_PORT, 6);    // * 64-69
        read_eeprom(70, 32).toCharArray(MQTT_USER, 32);  // * 70-101
        read_eeprom(102, 32).toCharArray(MQTT_PASS, 32); // * 102-133
    }

    WiFiManagerParameter CUSTOM_MQTT_HOST("host", "MQTT hostname", MQTT_HOST, 64);
    WiFiManagerParameter CUSTOM_MQTT_PORT("port", "MQTT port",     MQTT_PORT, 6);
    WiFiManagerParameter CUSTOM_MQTT_USER("user", "MQTT user",     MQTT_USER, 32);
    WiFiManagerParameter CUSTOM_MQTT_PASS("pass", "MQTT pass",     MQTT_PASS, 32);

    // * WiFiManager local initialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // * Reset settings - uncomment for testing
    // wifiManager.resetSettings();

    // * Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);

    // * Set timeout
    wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);

    // * Set save config callback
    wifiManager.setSaveConfigCallback(save_wifi_config_callback);

    // * Add all your parameters here
    wifiManager.addParameter(&CUSTOM_MQTT_HOST);
    wifiManager.addParameter(&CUSTOM_MQTT_PORT);
    wifiManager.addParameter(&CUSTOM_MQTT_USER);
    wifiManager.addParameter(&CUSTOM_MQTT_PASS);

    // * Fetches SSID and pass and tries to connect
    // * Reset when no connection after 10 seconds
    if (!wifiManager.autoConnect()) {
        Serial.println(F("Failed to connect to WIFI and hit timeout"));
        display_message("Reset.");

        // * Reset and try again
        ESP.reset();
        delay(WIFI_TIMEOUT);
    }

    // * Read updated parameters
    strcpy(MQTT_HOST, CUSTOM_MQTT_HOST.getValue());
    strcpy(MQTT_PORT, CUSTOM_MQTT_PORT.getValue());
    strcpy(MQTT_USER, CUSTOM_MQTT_USER.getValue());
    strcpy(MQTT_PASS, CUSTOM_MQTT_PASS.getValue());

    // * Save the custom parameters to FS
    if (shouldSaveConfig) {
        Serial.println(F("Saving WiFiManager config"));
        write_eeprom(0, 64, MQTT_HOST);   // * 0-63
        write_eeprom(64, 6, MQTT_PORT);   // * 64-69
        write_eeprom(70, 32, MQTT_USER);  // * 70-101
        write_eeprom(102, 32, MQTT_PASS); // * 102-133
        write_eeprom(134, 1, "1");        // * 134 --> always "1"
        EEPROM.commit();
    }

    // * If you get here you have connected to the WiFi
    Serial.println(F("Connected to WIFI..."));

    // * Keep LED on
    ticker.detach();
    digitalWrite(LED_BUILTIN, LOW);

    // * Configure OTA
    setup_ota();

    // * Startup MDNS Service
    Serial.println(F("Starting MDNS responder service"));
    MDNS.begin(HOSTNAME);

    // * Setup MQTT
    mqtt_client.setServer(MQTT_HOST, atoi(MQTT_PORT));
    Serial.printf("MQTT connecting to: %s:%s\n", MQTT_HOST, MQTT_PORT);

    // * Set the display to "System Connect"
    display_message("Connected.");
}

// **********************************
// * Loop                           *
// **********************************

void loop()
{
    // * Update last loop watchdog value
    last_loop = millis();

    // * Accept ota requests if offered
    ArduinoOTA.handle();

    // **********************************
    // * Maintain MQTT Connection       *
    // **********************************
    unsigned long NOW = millis();

    if (!mqtt_client.connected()) {

        if (NOW - LAST_RECONNECT_ATTEMPT >= MQTT_RECONNECT_DELAY) {
            LAST_RECONNECT_ATTEMPT = NOW;

            if (mqtt_reconnect()) {
                LAST_RECONNECT_ATTEMPT = 0;
            }
        }
    }
    else {
        mqtt_client.loop();
    }

    // **********************************
    // * Read the meter                 *
    // **********************************
    bool LUX_VALUE_CHANGED = false;

    if (NOW - LUX_METER_LAST_READ >= LUX_METER_READ_FREQUENCY) {
        uint16_t LUX = lux_meter.readLightLevel();
        LUX_METER_LAST_READ = NOW;

        if (LUX_METER_LAST_VALUE != LUX) {
            LUX_METER_LAST_VALUE = LUX;
            LUX_VALUE_CHANGED = true;
            set_display_should_update();

            // * Convert measured value to a char
            itoa((int)LUX_METER_LAST_VALUE, LUX_METER_VALUE_AS_CHAR, 10);
        }
    }

    // **********************************
    // * Send the data to MQTT broker   *
    // **********************************

    if (LUX_VALUE_CHANGED || NOW - MQTT_LAST_UPDATE >= MQTT_UPDATE_FREQUENCY) {

        MQTT_LAST_UPDATE = NOW;
        LUX_VALUE_CHANGED = false;

        Serial.print(F("Sending data to broker: "));
        Serial.println(LUX_METER_VALUE_AS_CHAR);

        bool result = mqtt_client.publish(MQTT_SENSOR_CHANNEL, LUX_METER_VALUE_AS_CHAR);

        if (!result) {
            Serial.printf("MQTT publish to topic %s failed\n", MQTT_SENSOR_CHANNEL);
        }
    }

    // **********************************
    // * Update the display             *
    // **********************************

    if (display_should_update()) {
        update_display();
    }
}
