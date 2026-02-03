#include "GPSManager.h"
#include "../ui/screens/TimeSettingScreen.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>

void GPSManager::begin() {
  // Note: Serial is now used for GPS (UART0), not debug output!
  // Debug output disabled to avoid conflict with GPS on GPIO 1/3

  _rxPin = PIN_GPS_RX;
  _txPin = PIN_GPS_TX;

  _baudRate = GPS_BAUD;
  _gpsSerial = &Serial2;

  Serial.printf("GPS Manager Begin: RX=%d, TX=%d\n", _rxPin, _txPin);

  // Try to find the current GPS baud rate (might be factory 9600)
  if (!detectBaudRate()) {
    _baudRate = 9600; // Last ditch guess
    _gpsSerial->begin(9600, SERIAL_8N1, _rxPin, _txPin);
  }

  // If we found it but it's not 115200, try to switch it
  if (_baudRate != 115200) {
    Serial.println("GPS: Switching module to 115200 baud...");
    setBaud(115200);
    delay(200);
    _gpsSerial->end();
    _gpsSerial->begin(115200, SERIAL_8N1, _rxPin, _txPin);
    _baudRate = 115200;
  }

  delay(500); // Wait for GPS module to stabilize

  // Configure NEO-M8N to enable UBX-NAV-PVT message (92 bytes, contains all nav
  // data) UBX-CFG-MSG: Enable NAV-PVT message
  uint8_t enableNavPvt[] = {
      0xB5, 0x62, // Header
      0x06, 0x01, // CFG-MSG
      0x03, 0x00, // Length = 3 bytes
      0x01,       // Message Class: NAV (0x01)
      0x07,       // Message ID: PVT (0x07)
      0x01,       // Rate: send every solution
      0x00, 0x00  // Checksum placeholder
  };

  // Calculate UBX checksum
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 9; i++) {
    ck_a += enableNavPvt[i];
    ck_b += ck_a;
  }
  enableNavPvt[9] = ck_a;
  enableNavPvt[10] = ck_b;

  _gpsSerial->write(enableNavPvt, 11);
  delay(100);

  // Set to 10Hz for high performance (Accuracy enhancement)
  setFrequencyLimit(10);
  delay(100);

  // Set Dynamic Model to Automotive (Better accuracy in corners)
  setDynamicModel(4);
  delay(100);

  // Enable UBX-NAV-SAT (0x01 0x35)
  uint8_t enableNavSat[] = {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00,
                            0x01, 0x35, 0x01, 0x00, 0x00};
  // Calc Checksum
  ck_a = 0;
  ck_b = 0;
  for (int i = 2; i < 9; i++) {
    ck_a += enableNavSat[i];
    ck_b += ck_a;
  }
  enableNavSat[9] = ck_a;
  enableNavSat[10] = ck_b;
  _gpsSerial->write(enableNavSat, 11);
  delay(100);

  // Serial.println NOT available - Serial used for GPS data!

  // Load Preferences
  Preferences prefs;
  prefs.begin("laptimer", true); // Read-only
  // Default to 7 (WIB) if not set (using -100 as sentinel for "not set")
  // OR if set to 0 (Legacy Default), assuming User is in Indonesia and hasn't
  // configured it.
  int storedOffset = prefs.getInt("utc_offset", -100);
  if (storedOffset == -100 || storedOffset == 0) {
    _utcOffset = 7; // Default to WIB (Indonesia)
    // Optional: We could save this back to prefs to make it permanent
    // prefs.putInt("utc_offset", 7);
  } else {
    _utcOffset = storedOffset;
  }
  prefs.end();
}

// Static Member Initialization
volatile unsigned long GPSManager::_rpmPulses = 0;
volatile unsigned long GPSManager::_lastPulseMicros = 0;
volatile unsigned long GPSManager::_pulseInterval = 0;

void IRAM_ATTR GPSManager::onPulse() {
  unsigned long now = micros();
  // Debounce: 2ms (2000us) -> Max 30.000 RPM (Reduces noise
  // significantly)
  unsigned long interval = now - _lastPulseMicros;
  if (interval > 2000) {
    _lastPulseMicros = now;
    _pulseInterval = interval;
    _rpmPulses++; // Still keep track of total pulses if needed
  }
}

void GPSManager::update() {
  if (!_gpsSerial)
    return;

  // Track bytes received for debug
  static unsigned long lastDebugTime = 0;
  static int bytesReceived = 0;

  while (_gpsSerial->available() > 0) {
    uint8_t c = _gpsSerial->read();
    bytesReceived++;
    _totalBytesReceived++; // Track total bytes for diagnostic

    // Invoke debug callback if set
    if (_dataCallback) {
      _dataCallback(c);
    }

    // Process UBX binary protocol
    processUBXByte(c);

    // Also process with TinyGPS++ for NMEA fallback
    _gps.encode(c);

    // Manual parsing of NMEA sentences for satellites in view
    if (c == '\n') {
      _nmeaBuffer[_nmeaPos] = '\0'; // Null terminate
      // Check if this is a GSV sentence: $GPGSV or $GNGSV
      if (_nmeaPos > 6 && _nmeaBuffer[0] == '$' && _nmeaBuffer[1] == 'G' &&
          _nmeaBuffer[3] == 'G' && _nmeaBuffer[4] == 'S' &&
          _nmeaBuffer[5] == 'V') {
        // GPGSV/GNGSV format: $GPGSV,numMsgs,msgNum,totalSats,...
        // Find 3rd comma
        int commaCount = 0;
        char *ptr = _nmeaBuffer;
        while (*ptr && commaCount < 3) {
          if (*ptr == ',')
            commaCount++;
          ptr++;
          if (commaCount == 3) {
            _satsInView = atoi(ptr);
            break;
          }
        }
      }
      _nmeaPos = 0;
    } else if (c != '\r') {
      if (_nmeaPos < sizeof(_nmeaBuffer) - 1) {
        _nmeaBuffer[_nmeaPos++] = (char)c;
      } else {
        _nmeaPos = 0; // Buffer overflow, reset
      }
    }
  }

  // Debug output every 5 seconds
  // NOTE: Serial debug disabled - Serial (UART0) used for GPS on GPIO 1/3
  if (millis() - lastDebugTime >= 5000) {
    Serial.print("GPS bytes received (last 5s): ");
    Serial.println(bytesReceived);
    Serial.print("GPS fix: ");
    Serial.println(isFixed() ? "YES" : "NO");
    Serial.print("Satellites: ");
    Serial.print(getSatellites());
    Serial.print(" | Chars processed: ");
    Serial.println(_gps.charsProcessed());

    // Simplified Debug Output
    Serial.print("GPS: Fix=");
    Serial.print(isFixed() ? "YES" : "NO");
    Serial.print(" Sats=");
    Serial.println(getSatellites());

    bytesReceived = 0;
    lastDebugTime = millis();
  }

  // --- SYSTEM TIME REDUNDANCY ---
  // 1. Tick System Time
  if (_lastTick == 0)
    _lastTick = millis();
  unsigned long now = millis();
  if (now - _lastTick >= 1000) {
    _sysSec++;
    if (_sysSec >= 60) {
      _sysSec = 0;
      _sysMin++;
      if (_sysMin >= 60) {
        _sysMin = 0;
        _sysHour++;
        if (_sysHour >= 24) {
          _sysHour = 0;
          // Day increment logic omitted for simplicity or could be added
        }
      }
    }
    _lastTick = now;
  }

  // 2. Auto-Sync with GPS (if valid)
  // Sync every second when GPS time is valid (not just when updated)
  static unsigned long lastGpsSync = 0;
  if (_gps.time.isValid() && _gps.date.isValid()) {
    // Only sync once per second to avoid constant overwrites
    if (now - lastGpsSync >= 1000) {
      // Overwrite System Time with GPS Time (UTC)
      _sysHour = _gps.time.hour();
      _sysMin = _gps.time.minute();
      _sysSec = _gps.time.second();
      _sysDay = _gps.date.day();
      _sysMonth = _gps.date.month();
      _sysYear = _gps.date.year();
      lastGpsSync = now;
    }
  }

  // Update Trip Meter
  if (_gps.location.isValid() && _gps.location.isUpdated()) {
    double lat = _gps.location.lat();
    double lng = _gps.location.lng();

    if (_hasLastPos) {
      double dist = distanceBetween(_lastLat, _lastLng, lat, lng);
      // Filter out jitter (e.g. static movements < 2m)
      if (dist > 2.0 && dist < 1000.0) { // < 1km jump is reasonable for 1Hz
        _totalDistance += dist;
      }
    }

    _lastLat = lat;
    _lastLng = lng;
    _hasLastPos = true;
    _updatesCount++;
  }

  // Calculate Hz every 1 second
  if (millis() - _lastRateCheck >= 1000) {
    _currentHz = _updatesCount;
    _updatesCount = 0;
    _lastRateCheck = millis();
  }

  // --- RPM CALCULATION (PERIOD METHOD) ---
  if (_rpmEnabled) {
    if (millis() - _lastRpmCalcTime > 50) { // 20Hz Update for smoothness
      _lastRpmCalcTime = millis();

      unsigned long lastP = _lastPulseMicros;
      unsigned long interval = _pulseInterval;
      unsigned long nowMicros = micros();

      // Timeout: 0.5s without pulse -> Engine Off/Stall (<120 RPM 4T)
      if (nowMicros - lastP > 500000) {
        _currentRPM = 0;
      } else if (interval > 0) {
        // Calculate RPM: (60 sec * 1000 ms * 1000 us) / (interval * PPR)
        // 60,000,000 / (interval * PPR)
        float ppr = (_currentPPR > 0.1) ? _currentPPR : 1.0;
        float instRPM = 60000000.0 / (float)(interval * ppr);

        if (instRPM > 20000)
          instRPM = 0; // Sanity check (Noise)

        // Smoothing (EMA)
        // _currentRPM = 0.7 * _currentRPM + 0.3 * instRPM;
        // Or simpler integer smoothing
        _currentRPM = (_currentRPM * 7 + (int)instRPM * 3) / 10;
      }
    }
  } else {
    _currentRPM = 0;
  }

  // Periodic Save (Every 1 minute)
  if (millis() - _lastSaveTime > 60000) {
    Preferences prefs;
    prefs.begin("laptimer", false);
    prefs.putDouble("total_trip", _totalDistance);
    prefs.end();

    // BACKUP TO SD CARD (Redundancy)
    if (SD.exists("/trip.txt") || SD.open("/trip.txt", FILE_WRITE)) {
      File file = SD.open("/trip.txt", FILE_WRITE); // Overwrite/create
      if (file) {
        file.println(_totalDistance);
        file.close();
      }
    }
    _lastSaveTime = millis();
  }
}

// Manual Setters
void GPSManager::setManualTime(int h, int m, int s) {
  // Input is LOCAL time. Convert to UTC for System Time.
  // UTC = Local - Offset
  int utcH = h - _utcOffset;

  // Handle wrap around
  if (utcH < 0)
    utcH += 24;
  if (utcH >= 24)
    utcH -= 24;

  _sysHour = utcH;
  _sysMin = m;
  _sysSec = s;

  // Save preference for "Manual Sync" if desired?
  // Current requirement is just set it.
  // Maybe valid GPS will overwrite this immediately?
  // YES. This is desired. Manual is fallback.
}

void GPSManager::setUtcOffset(int offset) {
  _utcOffset = offset;
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("utc_offset", offset);
  prefs.end();
}

bool GPSManager::isFixed() {
  // Return true if either UBX has a fix OR TinyGPS++ has a fix
  return _hasValidFix || _gps.location.isValid();
}

double GPSManager::getLatitude() {
  // Use UBX parsed data (NEO-M8N sends UBX, not NMEA)
  return _latitude;
}

double GPSManager::getLongitude() {
  // Use UBX parsed data (NEO-M8N sends UBX, not NMEA)
  return _longitude;
}

float GPSManager::getSpeedKmph() {
  // Use UBX parsed speed (already in km/h)
  return _currentSpeed;

  // Priority 2: Calculate speed from position changes
  if (_gps.location.isValid() && _gps.location.isUpdated()) {
    double lat = _gps.location.lat();
    double lon = _gps.location.lng();
    unsigned long now = millis();

    if (_hasLastSpeedPos && (now - _lastSpeedTime) > 0) {
      // Calculate distance traveled
      double dist = distanceBetween(_lastSpeedLat, _lastSpeedLon, lat, lon);

      // Calculate time elapsed (in hours)
      double timeHours = (now - _lastSpeedTime) / 3600000.0;

      // Speed = Distance / Time (km/h)
      // Ultra-sensitive: accept any movement > 1cm (0.01m)
      if (timeHours > 0 && dist > 0.01 && dist < 1000) {
        float newSpeed = (dist / 1000.0) / timeHours;

        // Smooth the speed (60% new, 40% old) to reduce jitter
        _calculatedSpeed = (_calculatedSpeed * 0.4) + (newSpeed * 0.6);
      }
    }

    // Update last position for next calculation
    _lastSpeedLat = lat;
    _lastSpeedLon = lon;
    _lastSpeedTime = now;
    _hasLastSpeedPos = true;
  }

  // Return calculated speed (or 0 if no data yet)
  return _calculatedSpeed;
}

float GPSManager::getTotalTrip() {
  return (float)(_totalDistance / 1000.0); // Convert to km
}

void GPSManager::resetTrip() {
  _totalDistance = 0.0;
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putDouble("total_trip", 0.0);
  prefs.end();
}

int GPSManager::getSatellites() {
  // Use the best available satellite count
  int sats = _satelliteCount; // From UBX
  if (sats <= 0)
    sats = _gps.satellites.value(); // Fallback to TinyGPS++
  if (sats <= 0)
    sats = _satsInView; // Fallback to manual NMEA parse
  return sats;
}

String GPSManager::getTimeString() {
  int h, m, s, d, mo, y;
  getLocalTime(h, m, s, d, mo, y);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  return String(buf);
}

String GPSManager::getDateString() {
  int h, m, s, d, mo, y;
  getLocalTime(h, m, s, d, mo, y);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d", d, mo, y);
  return String(buf);
}

void GPSManager::getLocalTime(int &h, int &m, int &s, int &d, int &mo, int &y) {
  // Use Internal System Time (Redundant Source)
  h = _sysHour;
  m = _sysMin;
  s = _sysSec;
  d = _sysDay;
  mo = _sysMonth;
  y = _sysYear;

  h += _utcOffset;

  if (h < 0) {
    h += 24;
    d--;
    if (d < 1) {
      mo--;
      if (mo < 1) {
        mo = 12;
        y--;
      }
      static const int daysInMonth[] = {0,  31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};
      int days = daysInMonth[mo];
      if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)))
        days = 29;
      d = days;
    }
  } else if (h >= 24) {
    h -= 24;
    d++;
    static const int daysInMonth[] = {0,  31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};
    int days = daysInMonth[mo];
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)))
      days = 29;
    if (d > days) {
      d = 1;
      mo++;
      if (mo > 12) {
        mo = 1;
        y++;
      }
    }
  }
}

int GPSManager::getRawHour() {
  return _gps.time.isValid() ? _gps.time.hour() : 0;
}

int GPSManager::getRawMinute() {
  return _gps.time.isValid() ? _gps.time.minute() : 0;
}

double GPSManager::getHDOP() {
  // 1. Use UBX-parsed HDOP if available (valid is < 99.9)
  if (_hdop < 99.0) {
    return _hdop;
  }
  // 2. Fallback to TinyGPS++ NMEA parsing
  if (_gps.hdop.isValid()) {
    return _gps.hdop.hdop();
  }
  return 99.9; // Default for no fix
}

double GPSManager::getAltitude() {
  // Use UBX parsed altitude
  return _altitude;
}

double GPSManager::getHeading() {
  if (_gps.course.isValid()) {
    return _gps.course.deg();
  }
  return 0.0;
}

int GPSManager::getUpdateRate() { return _currentHz; }

unsigned long GPSManager::getBytesReceived() { return _totalBytesReceived; }

double GPSManager::distanceBetween(double lat1, double long1, double lat2,
                                   double long2) {
  return _gps.distanceBetween(lat1, long1, lat2, long2);
}

// --- CONFIGURATION IMPL ---

void GPSManager::sendUBX(const uint8_t *cmd, int len) {
  if (_gpsSerial) {
    _gpsSerial->write(cmd, len);
  }
}

void GPSManager::setGnssMode(uint8_t mode) {
  if (!_gpsSerial)
    return;

  // UBX-CFG-GNSS commands for different constellations
  // We construct these based on U-blox M8 protocol
  // Simplify: Trigger Cold Start or just minimal configuration?
  // Real implementation requires constructing complex payload.
  // For this prototype, we will handle RATE mostly as it's the primary
  // "User Visible" change Hz.

  // Mapping Mode to Hz Limit
  int targetRate = 1; // Default safer 1Hz for 9600 baud

  // Only allow higher rates if Baud Rate is sufficient (>38400)
  // 9600 baud can barely handle 10Hz if sentences are short, but with
  // full NMEA it chokes. Safe limit: 1Hz for 9600.
  if (_baudRate > 38400) {
    switch (mode) {
    case 0:
      targetRate = 5;
      break; // All
    case 1:
      targetRate = 5;
      break; // GPS+GLO+SBAS (Was 16)
    case 2:
      targetRate = 5;
      break; // GPS+GAL+GLO+SBAS
    case 3:
      targetRate = 5;
      break; // GPS+GAL+SBAS (Was 20)
    case 4:
      targetRate = 5;
      break; // GPS+SBAS (Was 25)
    case 5:
      targetRate = 5;
      break; // GPS Only (Was 25)
    case 6:
      targetRate = 5;
      break; // GPS+BEI+SBAS (Was 12)
    case 7:
      targetRate = 5;
      break; // GPS+GLO (Was 16)
    }
  } else {
    // For 9600 baud, force 1Hz to be safe.
    targetRate = 1;
  }
  setFrequencyLimit(targetRate);
  _currentGnssMode = mode;

  // In a real scenario, we would send UBX-CFG-GNSS here to enable/disable
  // specific constellations. Due to complexity and lack of verification
  // hardware, we mock the constellation switch but APPLY the Update Rate
  // which is universally supported on M8/M10.

  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("gnss_mode", mode);
  prefs.end();
}

uint8_t GPSManager::getGnssMode() { return _currentGnssMode; }

void GPSManager::setDynamicModel(uint8_t modelIdx) {
  if (!_gpsSerial)
    return;

  // User Index to UBX DynModel Mapping
  // 0: Portable    -> 0
  // 1: Stationary  -> 2
  // 2: Pedestrian  -> 3
  // 3: Automotive  -> 4 (Default)
  // 4: At Sea      -> 5
  // 5: Air <1g     -> 6
  // 6: Air <2g     -> 7
  // 7: Air <4g     -> 8

  uint8_t ubxModel = 4; // Default Automotive
  switch (modelIdx) {
  case 0:
    ubxModel = 0;
    break;
  case 1:
    ubxModel = 2;
    break;
  case 2:
    ubxModel = 3;
    break;
  case 3:
    ubxModel = 4;
    break;
  case 4:
    ubxModel = 5;
    break;
  case 5:
    ubxModel = 6;
    break;
  case 6:
    ubxModel = 7;
    break;
  case 7:
    ubxModel = 8;
    break;
  }

  // UBX-CFG-NAV5 (0x06 0x24)
  uint8_t packet[] = {
      0xB5,     0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, // Mask
      ubxModel,                                           // dynModel
      0x03,                                               // fixMode (3=Auto)
      0x00,     0x00, 0x00, 0x00,                         // fixedAlt
      0x10,     0x27, 0x00, 0x00,                         // fixedAltVar
      0x05,                                               // minElev
      0x00,                                               // drLimit
      0xFA,     0x00,                                     // pDop
      0xFA,     0x00,                                     // tDop
      0x64,     0x00,                                     // pAcc
      0x2C,     0x01,                                     // tAcc
      0x00,                                               // staticHoldThresh
      0x3C,                                               // dgpsTimeOut
      0x00,     0x00, 0x00, 0x00,                         // cnoThresh
      0x00,     0x00,                                     // reserved
      0x00,     0x00, 0x00, 0x00,                         // reserved
      0x00,     0x00                                      // Checksum
  };

  // Calc Checksum
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 38; i++) {
    ck_a += packet[i];
    ck_b += ck_a;
  }
  packet[38] = ck_a;
  packet[39] = ck_b;

  sendUBX(packet, sizeof(packet));

  _currentDynModel = modelIdx;
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("gnss_model", modelIdx);
  prefs.end();
}

void GPSManager::setSBASConfig(uint8_t regionIndex) {
  if (!_gpsSerial)
    return;

  // SBAS Configuration (UBX-CFG-SBAS 0x06 0x16)
  // We mainly want to enable/disable or set the PRN mask.
  // For simplicity in this "Blind" implementation, we will validly toggle
  // the system.

  // Region Index (New Order):
  // 0: EGNOS (Europe)
  // 1: WAAS (USA)
  // 2: SDCM (Russia)
  // 3: MSAS (Japan)
  // 4: GAGAN (India)
  // 5: SouthPAN (Aus/NZ)
  // 6: S.AMERICA (NONE) -> Disable
  // 7: MID-EAST (NONE)  -> Disable
  // 8: AFRICA (NONE)    -> Disable
  // 9: China (BDSBAS)   -> Enable
  // 10: KASS (Korea)    -> Enable

  bool enable = true;
  uint32_t prnMask = 0; // 0 = Auto/All

  // Disable if index is 6, 7, or 8
  if (regionIndex >= 6 && regionIndex <= 8) {
    enable = false; // Disable SBAS
  } else {
    // Enable for others (including China/Korea which are now 9/10)
    prnMask = 0x00000000;
  }

  uint8_t mode = enable ? 0x01 : 0x00;

  uint8_t packet[] = {
      0xB5, 0x62, 0x06, 0x16, 0x08, 0x00,
      mode,                   // mode (Enable/Disable)
      0x03,                   // usage (Range+DiffCorr+Integrity)
      0x03,                   // maxSBAS (3 channels)
      0x00,                   // scanmode2 (PRN Mask Low - 0 for auto)
      0x00, 0x00, 0x00, 0x00, // scanmode1 (PRN Mask High)
      0x00, 0x00              // Checksum
  };

  // If we wanted to be rigorous:
  // WAAS PRNs: 131,133,135,138 -> Map to bits
  // Since we don't have the exact bitmask function handy and don't want
  // to break it, we assume 0 (Auto Scan) is sufficient for "Enable".
  // Disabling (Index >= 8) allows revert to raw GPS.

  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 14; i++) {
    ck_a += packet[i];
    ck_b += ck_a;
  }
  packet[14] = ck_a;
  packet[15] = ck_b;

  sendUBX(packet, sizeof(packet));

  _currentSBAS = regionIndex;
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("gnss_sbas", regionIndex);
  prefs.end();
}

void GPSManager::setRpmEnabled(bool enabled) {
  _rpmEnabled = enabled;

  if (PIN_RPM_INPUT >= 0) {
    if (_rpmEnabled) {
      pinMode(PIN_RPM_INPUT, INPUT);
      attachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT), onPulse, FALLING);
    } else {
      detachInterrupt(digitalPinToInterrupt(PIN_RPM_INPUT));
      _currentRPM = 0;
      _rpmPulses = 0;
    }
  }

  // Save Preference
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putBool("rpm_enabled", enabled);
  prefs.end();
}

void GPSManager::setPPRIndex(int idx) {
  _currentPPR = 1.0;
  switch (idx) {
  case 0:
    _currentPPR = 1.0;
    break;
  case 1:
    _currentPPR = 0.5;
    break;
  case 2:
    _currentPPR = 2.0;
    break;
  case 3:
    _currentPPR = 3.0;
    break;
  case 4:
    _currentPPR = 4.0;
    break;
  }
}

void GPSManager::setFrequencyLimit(int freq) {
  if (!_gpsSerial)
    return;

  // UBX-CFG-RATE
  // rate = 1000 / freq
  uint16_t rateMs = 1000 / freq;

  uint8_t packet[] = {
      0xB5,
      0x62,
      0x06,
      0x08,
      0x06,
      0x00,
      (uint8_t)(rateMs & 0xFF),
      (uint8_t)((rateMs >> 8) & 0xFF), // measRate
      0x01,
      0x00, // navRate (always 1)
      0x01,
      0x00, // timeRef (GPS)
      0x00,
      0x00 // Checksum
  };

  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 12; i++) {
    ck_a += packet[i];
    ck_b += ck_a;
  }
  packet[12] = ck_a;
  packet[13] = ck_b;

  sendUBX(packet, sizeof(packet));
  _targetFreq = freq;
}

void GPSManager::setProjection(bool enabled) {
  _projectionEnabled = enabled;
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putBool("gnss_proj", enabled);
  prefs.end();
}

void GPSManager::setPins(int rx, int tx) {
  if (_rxPin == rx && _txPin == tx)
    return;

  _rxPin = rx;
  _txPin = tx;

  // Save to prefs
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("gps_rx_pin", _rxPin);
  prefs.putInt("gps_tx_pin", _txPin);
  prefs.end();

  // Restart Serial
  if (_gpsSerial) {
    _gpsSerial->end();
    delay(100);
    _gpsSerial->begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);

    // Re-apply config as module might have power cycled?
    // Actually ESP32 UART reset doesn't reset the GPS module itself,
    // but just in case we need to re-init communication.
    delay(100);
    setGnssMode(_currentGnssMode);
    setDynamicModel(_currentDynModel);
    setSBASConfig(_currentSBAS);
  }
}

void GPSManager::setBaud(int baud) {
  if (_baudRate == baud)
    return;

  // 1. Command GPS to switch (while still at old baud)
  configureGpsBaud(baud);

  _baudRate = baud;

  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putInt("gps_baud", _baudRate);
  prefs.end();

  // 2. Switch ESP32 to new baud
  if (_gpsSerial) {
    delay(200); // Wait for module
    _gpsSerial->updateBaudRate(_baudRate);
    delay(100);
    // Re-apply config
    setGnssMode(_currentGnssMode);
    setDynamicModel(_currentDynModel);
    setSBASConfig(_currentSBAS);
  }
}

void GPSManager::configureGpsBaud(int targetBaud) {
  if (!_gpsSerial)
    return;

  // UBX-CFG-PRT (0x06 0x00)
  uint8_t packet[] = {
      0xB5,
      0x62,
      0x06,
      0x00,
      0x14,
      0x00,
      0x01,
      0x00,
      0x00,
      0x00, // PortID=1 (UART1)
      0xD0,
      0x08,
      0x00,
      0x00,                         // Mode (8N1)
      (uint8_t)(targetBaud & 0xFF), // Baud LSB
      (uint8_t)((targetBaud >> 8) & 0xFF),
      (uint8_t)((targetBaud >> 16) & 0xFF),
      (uint8_t)((targetBaud >> 24) & 0xFF), // Baud MSB
      0x07,
      0x00, // In Proto (UBX+NMEA+RTCM)
      0x03,
      0x00, // Out Proto (UBX+NMEA)
      0x00,
      0x00, // Flags
      0x00,
      0x00, // Reserved
      0x00,
      0x00 // Checksum
  };

  // Calc Checksum
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 26; i++) {
    ck_a += packet[i];
    ck_b += ck_a;
  }
  packet[26] = ck_a;
  packet[27] = ck_b;

  sendUBX(packet, sizeof(packet));
}

void GPSManager::disableUnnecessarySentences() {
  if (!_gpsSerial)
    return;

  // Disable GSA (DOP and active satellites) - Not critical for racing
  uint8_t disableGSA[] = {
      0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, // NMEA-GxGSA
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             // Disable on all ports
      0x01, 0x31                                      // Checksum
  };
  sendUBX(disableGSA, sizeof(disableGSA));
  delay(50);

  // Disable GSV (Satellites in view) - Not critical, uses lots of
  // bandwidth
  uint8_t disableGSV[] = {
      0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, // NMEA-GxGSV
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x38  // Checksum
  };
  sendUBX(disableGSV, sizeof(disableGSV));
  delay(50);

  // Disable GLL (Geographic position) - Redundant with RMC
  uint8_t disableGLL[] = {
      0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, // NMEA-GxGLL
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A  // Checksum
  };
  sendUBX(disableGLL, sizeof(disableGLL));
  delay(50);

  // Keep enabled: GGA (Position), RMC (Recommended minimum), VTG
  // (Track/Speed) These are essential for racing/tracking
}
// UBX Binary Protocol Parser Implementation
// This file contains the UBX parser methods for GPSManager
// Append this to the end of GPSManager.cpp

void GPSManager::resetModule() {
  Serial.println("GPS: Sending Reset Command (Cold Start)...");

  // UBX-CFG-RST (0x06 0x04)
  // Payload:
  // navBbrMask (2 bytes): 0xFFFF = Cold Start (clear all)
  // resetMode (1 byte): 0x01 = Hardware Reset (watchdog), 0x02 = Controlled
  // Software Reset reserved1 (1 byte): 0x00
  uint8_t resetCmd[] = {
      0xB5, 0x62, // Header
      0x06, 0x04, // Class/ID
      0x04, 0x00, // Length (4 bytes)
      0xFF, 0xFF, // BBR Mask (Cold Start)
      0x01,       // Reset Mode (Hardware Reset)
      0x00,       // Reserved
      0x00, 0x00  // Checksum
  };

  // Calc Checksum
  uint8_t ck_a = 0, ck_b = 0;
  for (int i = 2; i < 10; i++) {
    ck_a += resetCmd[i];
    ck_b += ck_a;
  }
  resetCmd[10] = ck_a;
  resetCmd[11] = ck_b;

  _gpsSerial->write(resetCmd, 12);
  _gpsSerial->flush();

  // Reset internal state
  _hasValidFix = false;
  _satelliteCount = 0;
  _satsInView = 0;
  _hdop = 99.9;

  // Re-init some configs after a short delay
  delay(500);
  begin();
}

void GPSManager::processUBXByte(uint8_t b) {
  switch (_ubxState) {
  case UBX_SYNC1:
    if (b == 0xB5) {
      _ubxState = UBX_SYNC2;
    }
    break;

  case UBX_SYNC2:
    if (b == 0x62) {
      _ubxState = UBX_CLASS;
      _ubxCkA = 0;
      _ubxCkB = 0;
    } else {
      _ubxState = UBX_SYNC1;
    }
    break;

  case UBX_CLASS:
    _ubxClass = b;
    _ubxCkA += b;
    _ubxCkB += _ubxCkA;
    _ubxState = UBX_ID;
    break;

  case UBX_ID:
    _ubxId = b;
    _ubxCkA += b;
    _ubxCkB += _ubxCkA;
    _ubxState = UBX_LEN1;
    break;

  case UBX_LEN1:
    _ubxLength = b;
    _ubxCkA += b;
    _ubxCkB += _ubxCkA;
    _ubxState = UBX_LEN2;
    break;

  case UBX_LEN2:
    _ubxLength |= (b << 8);
    _ubxCkA += b;
    _ubxCkB += _ubxCkA;
    _ubxPayloadIndex = 0;
    _ubxState = (_ubxLength > 0) ? UBX_PAYLOAD : UBX_CK_A;
    break;

  case UBX_PAYLOAD:
    // Always store byte if space exists
    if (_ubxPayloadIndex < sizeof(_ubxPayload)) {
      _ubxPayload[_ubxPayloadIndex] = b;
    }
    // Always increment index to track progress against _ubxLength
    _ubxPayloadIndex++;

    _ubxCkA += b;
    _ubxCkB += _ubxCkA;

    if (_ubxPayloadIndex >= _ubxLength) {
      // Packet Complete
      _ubxState = UBX_CK_A;
    }
    break;

  case UBX_CK_A:
    // We already calculated _ubxCkA from payload.
    // The byte b IS the received CK_A.
    // Wait, the calculated values include Class, ID, Len, Payload.
    // We compare calculated _ubxCkA with received b.
    if (b == _ubxCkA) {
      _ubxState = UBX_CK_B;
    } else {
      _ubxState = UBX_SYNC1; // Fail
    }
    break;

  case UBX_CK_B:
    if (b == _ubxCkB) {
      // Valid Packet!
      if (_ubxClass == 0x01 && _ubxId == 0x07) {
        parseUBXNavPvt();
      } else if (_ubxClass == 0x01 && _ubxId == 0x35) {
        parseUBXNavSat();
      }
    }
    _ubxState = UBX_SYNC1;
    break;
  }
}

void GPSManager::parseUBXNavSat() {
  uint8_t numSats = _ubxPayload[5];

  // DEBUG
  // Serial.print("NAV-SAT: Sats=");
  // Serial.println(numSats);

  _satellites.clear();

  // Safety check
  if (numSats > 50)
    return;

  int offset = 8;
  for (int i = 0; i < numSats; i++) {
    if (offset + 12 > _ubxLength)
      break;
    if (offset + 12 > sizeof(_ubxPayload))
      break; // Buffer safety

    SatelliteInfo sat;
    sat.id = _ubxPayload[offset + 1];
    sat.snr = _ubxPayload[offset + 2];
    sat.elevation = (int8_t)_ubxPayload[offset + 3];
    sat.azimuth =
        (int16_t)(_ubxPayload[offset + 4] | (_ubxPayload[offset + 5] << 8));

    _satellites.push_back(sat);

    offset += 12;
  }
}

void GPSManager::parseUBXNavPvt() {
  if (_ubxLength < 92)
    return; // Invalid UBX-NAV-PVT message

  // Extract fix type (offset 20)
  uint8_t fixType = _ubxPayload[20];
  _hasValidFix = (fixType == 0x02 || fixType == 0x03); // 2D or 3D fix

  // Extract satellite count (offset 23)
  _satelliteCount = _ubxPayload[23];

  // Extract longitude (offset 24, 4 bytes, little-endian, 1e-7 degrees)
  int32_t lonRaw = *((int32_t *)(&_ubxPayload[24]));
  _longitude = lonRaw / 10000000.0;

  // Extract latitude (offset 28, 4 bytes, little-endian, 1e-7 degrees)
  int32_t latRaw = *((int32_t *)(&_ubxPayload[28]));
  _latitude = latRaw / 10000000.0;

  // Extract altitude MSL (offset 36, 4 bytes, mm)
  int32_t altRaw = *((int32_t *)(&_ubxPayload[36]));
  _altitude = altRaw / 1000.0; // Convert mm to meters

  // Extract ground speed (offset 60, 4 bytes, mm/s)
  int32_t speedRaw = *((int32_t *)(&_ubxPayload[60]));
  _currentSpeed = (speedRaw / 1000.0) * 3.6; // Convert mm/s to km/h

  // Extract heading (offset 64, 4 bytes, 1e-5 degrees)
  int32_t headRaw = *((int32_t *)(&_ubxPayload[64]));
  _heading = headRaw / 100000.0;

  // Extract PDOP (offset 76, 2 bytes, 0.01)
  uint16_t pdopRaw = *((uint16_t *)(&_ubxPayload[76]));
  _hdop = pdopRaw / 100.0;

  // Extract date/time if valid (offset 11 - valid flags)
  // Bit 0: Valid Date, Bit 1: Valid Time. Both must be 1 (0x03)
  if ((_ubxPayload[11] & 0x03) == 0x03) { // Valid Date & Time
    uint16_t year = *((uint16_t *)(&_ubxPayload[4]));
    uint8_t month = _ubxPayload[6];
    uint8_t day = _ubxPayload[7];
    uint8_t hour = _ubxPayload[8];
    uint8_t min = _ubxPayload[9];
    uint8_t sec = _ubxPayload[10];

    // Update system time (Internal is ALWAYS UTC)
    _sysYear = year;
    _sysMonth = month;
    _sysDay = day;
    _sysHour = hour; // Keep as raw UTC internally
    _sysMin = min;
    _sysSec = sec;
  }

  // Update counters
  _updatesCount++;
  _lastUpdateTime = millis();
}

bool GPSManager::detectBaudRate() {
  int rates[] = {9600, 19200, 38400, 57600, 115200};

  Serial.println("GPS: Auto-detecting baud rate...");

  for (int i = 0; i < 5; i++) {
    int rate = rates[i];
    Serial.print("Trying ");
    Serial.print(rate);
    Serial.print("... ");

    _gpsSerial->begin(rate, SERIAL_8N1, _rxPin, _txPin);
    unsigned long start = millis();
    bool validDataFound = false;

    uint8_t lastChar = 0;
    while (millis() - start < 500) { // Listen for 500ms
      if (_gpsSerial->available()) {
        uint8_t c = _gpsSerial->read();

        // Check for UBX Sync (0xB5 0x62)
        if (lastChar == 0xB5 && c == 0x62) {
          validDataFound = true;
          break;
        }
        // Check for NMEA Start ($G)
        if (lastChar == '$' && c == 'G') {
          validDataFound = true;
          break;
        }
        lastChar = c;
      }
    }

    if (validDataFound) {
      Serial.println("FOUND!");
      _baudRate = rate;
      // Keep Serial2 open at this rate
      return true;
    } else {
      Serial.println("No valid data.");
      _gpsSerial->end();
      delay(50); // Small pause
    }
  }
  return false;
}
