// **********************************
// * Settings                       *
// **********************************

#define OLED_RESET LED_BUILTIN  //4
#define BAUD_RATE 115200

#define DISPLAY_I2C_ADDR 0x3C
#define LUX_METER_I2C_ADDR 0x23

// * Hostname
#define HOSTNAME "luxmeter1.home"

// * The password used for uploading
#define OTA_PASSWORD "admin"

// * Wifi timeout in milliseconds
#define WIFI_TIMEOUT 30000

// * MQTT network settings
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_MAX_RECONNECT_TRIES 10

// * Watchdog timer
#define OSWATCH_RESET_TIME 300

// * Watchdog: Will be updated each loop
static unsigned long last_loop;

// * MQTT topic settings
char * MQTT_SENSOR_CHANNEL = (char*) "sensors/luxmeter1/lux";

// * To be filled with EEPROM data
char MQTT_HOST[64]       = "";
char MQTT_PORT[6]        = "";
char MQTT_USER[32]       = "";
char MQTT_PASS[32]       = "";

// * MQTT Last reconnection counter
long LAST_RECONNECT_ATTEMPT     = 0;
const long MQTT_RECONNECT_DELAY = 5000;

// * MQTT publish to broker counters
unsigned long MQTT_LAST_UPDATE         = 0;            // * Will store the last time the value was sent to the broker
const long    MQTT_UPDATE_FREQUENCY    = 5000;         // * Send the measured value to the broker every 5 seconds

// * Will be changed when the display should update
bool          DISPLAY_SHOULD_UPDATE    = true;         // * Will be set to true when display needs an update
unsigned long DISPLAY_LAST_UPDATE      = 0;            // * Will store the last time the display was updated
const long    DISPLAY_UPDATE_FREQUENCY = 5000;         // * Update the display every 5 seconds

// * Lux meter counters
unsigned long LUX_METER_LAST_READ      = 0;            // * Will store the last time the lux meter was read
const long    LUX_METER_READ_FREQUENCY = 1000;         // * Read the value evewry second

// * Lux meter last values
uint16_t      LUX_METER_LAST_VALUE;
char          LUX_METER_VALUE_AS_CHAR[12];

