#include "wled.h"
#include "sensor_bus.h"
#include <BH1750.h>

/*
 * BH1750 ambient light sensor provider.
 *
 * Reads a Rohm BH1750 over I2C (address 0x23 or 0x5C - both are probed)
 * and pushes the reading into the Sensor Hub (see
 * ../sensor-hub/usermod_sensor_hub.cpp and ../sensor-hub/sensor_bus.h) as
 * "<prefix>_illuminance" (lx). This usermod never talks to MQTT, the JSON
 * API or the Info tab itself - the hub takes care of all of that once a
 * sensor is registered here.
 *
 * Wiring: SDA/SCL go to the I2C pins configured on WLED's own Config > LED
 * Preferences page (the shared "i2c_sda"/"i2c_scl" globals). WLED core
 * already calls Wire.begin() with those pins while loading cfg.json at
 * boot (wled00/cfg.cpp), before any usermod's setup() runs - so this
 * usermod only needs to confirm the pins are set, then use the shared Wire
 * bus. It must NOT call Wire.begin() itself.
 */
class BH1750SensorUsermod : public Usermod {
  private:
    BH1750 lightMeter;
    SensorHub* hub = nullptr;
    uint8_t luxHandle = SENSOR_HANDLE_INVALID;

    bool enabled = true;
    bool sensorFound = false;
    bool initDone = false;

    unsigned long lastRead = 0;
    unsigned long lastBeginAttempt = 0;
    uint8_t consecutiveFailures = 0;

    // config
    uint16_t checkIntervalS = 10; // how often to read the sensor
    String namePrefix = "bh1750"; // sensor name becomes "<prefix>_illuminance"
    uint8_t precision = 0;        // decimal places published
    uint8_t priority = 100;       // getValue() selection priority - lower wins among sensors of the same SensorType (see sensor_bus.h)

    static const char _name[];
    static const char _enabled[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _precision[];
    static const char _priority[];

    bool beginSensor() {
      return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire) ||
             lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);
    }

    void registerSensors() {
      if (!hub || luxHandle != SENSOR_HANDLE_INVALID) return; // already registered
      luxHandle = hub->registerSensor((namePrefix + "_illuminance").c_str(), SensorType::Illuminance, nullptr, nullptr, precision, priority);
    }

  public:
    void setup() override {
      // I2C bus is configured (and Wire.begin() already called) via WLED's
      // own Config > LED Preferences page - nothing to do here if it's unset.
      // Don't persist this into 'enabled' (the user's own on/off switch) -
      // initDone (left false here) is what actually gates loop(), so a
      // later pin fix takes effect on the next boot instead of staying
      // stuck disabled.
      if (i2c_sda < 0 || i2c_scl < 0) return;
      sensorFound = beginSensor();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();

      if (!sensorFound) {
        // sensor missing at boot (or lost) - keep retrying rather than giving up forever
        if (now - lastBeginAttempt < 10000) return;
        lastBeginAttempt = now;
        sensorFound = beginSensor();
        if (!sensorFound) return;
      }

      if (now - lastRead < (unsigned long)checkIntervalS * 1000UL) return;
      lastRead = now;

      if (!lightMeter.measurementReady(true)) return; // not fatal - just not ready yet, try again next loop

      float lux = lightMeter.readLightLevel();
      if (lux < 0) { // library returns -1 (or -2) on read/timing errors
        consecutiveFailures++;
        if (hub && luxHandle != SENSOR_HANDLE_INVALID && consecutiveFailures >= 3) hub->setSensorAvailable(luxHandle, false);
        if (consecutiveFailures >= 10) sensorFound = false; // force a fresh begin() next loop
        return;
      }

      consecutiveFailures = 0;
      if (hub && luxHandle != SENSOR_HANDLE_INVALID) {
        hub->setSensorAvailable(luxHandle, true);
        hub->updateSensor(luxHandle, lux);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_checkInterval)] = checkIntervalS;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_precision)] = precision;
      top[FPSTR(_priority)] = priority;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalS);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_precision)], precision);
      configComplete &= getJsonValue(top[FPSTR(_priority)], priority);
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('BH1750Sensor:checkInterval',1,'seconds between sensor reads');"));
      settingsScript.print(F("addInfo('BH1750Sensor:namePrefix',1,'sensor name becomes &lt;prefix&gt;_illuminance - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('BH1750Sensor:precision',1,'decimal places published');"));
      settingsScript.print(F("addInfo('BH1750Sensor:priority',1,'getValue() selection priority - lower wins if another provider also registers an Illuminance sensor');"));
    }
};

const char BH1750SensorUsermod::_name[]          PROGMEM = "BH1750Sensor";
const char BH1750SensorUsermod::_enabled[]       PROGMEM = "enabled";
const char BH1750SensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char BH1750SensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char BH1750SensorUsermod::_precision[]     PROGMEM = "precision";
const char BH1750SensorUsermod::_priority[]      PROGMEM = "priority";

static BH1750SensorUsermod bh1750_sensor;
REGISTER_USERMOD(bh1750_sensor);
