#include "SessionManager.h"
#include <SPI.h>

// Queue item structure (implicitly just char* for now)

void SessionManager::begin() {
  _logging = false;

  Serial.printf("SD Init: SCK=%d, MISO=%d, MOSI=%d, CS=%d\n", PIN_SD_SCLK,
                PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  // Gunakan instans SPI khusus untuk Kartu SD (VSPI)
  // Ini menghindari konflik dengan Tampilan/Sentuh (biasanya pada HSPI)
  SPIClass *sdSpi = new SPIClass(VSPI);
  sdSpi->begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  delay(10); // Tunggu SPI stabil

  // Teruskan SPI khusus ke SD.begin
  if (!SD.begin(PIN_SD_CS, *sdSpi, 4000000)) { // Kecepatan aman 4MHz
    Serial.println("SD Card Init Failed!");
  } else {
    Serial.println("SD Card Ready");
    if (!SD.exists("/sessions")) {
      SD.mkdir("/sessions");
    }
  }

  // Initialize Logging Queue (Holds 50 pointers)
  _logQueue = xQueueCreate(50, sizeof(char *));

  // Start Logging Task (Pinned to Core 0 to leave Core 1 for UI/Arduino)
  xTaskCreatePinnedToCore(loggingTask, "LoggingTask", 4096, this, 1,
                          &_loggingTaskHandle, 0);
}

bool SessionManager::startSession() {
  if (_logging)
    return true;

  String filename = createFilename();
  _logFile = SD.open(filename, FILE_WRITE);

  if (_logFile) {
    _logging = true;
    _currentFilename = filename;
    // _logFile.println("Time,Lat,Lon,Speed,Sats,Alt,Heading"); // Header - Send
    // via Queue instead
    logData("Time,Lat,Lon,Speed,Sats,Alt,Heading");

    Serial.println("Started logging to: " + filename);
    return true;
  }
  return false;
}

void SessionManager::stopSession() {
  if (_logging) {
    // Wait a bit for queue to flush?
    // We can't strictly wait essentially, but let's give it a moment
    unsigned long startWait = millis();
    while (uxQueueMessagesWaiting(_logQueue) > 0 &&
           millis() - startWait < 500) {
      delay(10);
    }

    _logging = false;

    // Slight delay to ensure task sees _logging=false or finishes last write
    delay(50);

    if (_logFile) {
      _logFile.close();
      Serial.println("Session Stopped");
    }
  }
}

void SessionManager::logData(String dataLine) {
  if (_logging) {
    // Alloc string on heap
    char *msg = strdup(dataLine.c_str());
    if (msg) {
      // Send pointer to queue. Do not block if full (drop packet instead of
      // freeze)
      if (xQueueSend(_logQueue, &msg, 0) != pdTRUE) {
        free(msg); // Queue full, discard to prevent leak
        Serial.println("Log Queue Full!");
      }
    }
  }
}

void SessionManager::loggingTask(void *parameter) {
  SessionManager *self = (SessionManager *)parameter;
  char *msg;

  while (true) {
    if (xQueueReceive(self->_logQueue, &msg, portMAX_DELAY) == pdTRUE) {
      if (self->_logging && self->_logFile) {
        self->_logFile.println(msg);
        // Optional: Flush every N lines or rely on close()
      }
      free(msg); // Important: Free the memory allocated in logData
    }
  }
}

String SessionManager::createFilename() {
  int i = 0;
  String fn;
  do {
    fn = "/sessions/run_" + String(i) + ".csv";
    i++;
  } while (SD.exists(fn));
  return fn;
}

void SessionManager::appendToHistoryIndex(String filename, String date,
                                          int laps, unsigned long bestLap,
                                          String type) {
  File indexFile = SD.open("/history.csv", FILE_APPEND);
  if (!indexFile) {
    indexFile = SD.open("/history.csv", FILE_WRITE);
  }

  if (indexFile) {
    // Format: NamaFile,Tanggal,Lap,LapTerbaik,Tipe
    String line = filename + "," + date + "," + String(laps) + "," +
                  String(bestLap) + "," + type;
    indexFile.println(line);
    indexFile.close();
    Serial.println("Added to history index: " + line);
  } else {
    Serial.println("Failed to open history index");
  }
}

String SessionManager::loadHistoryIndex() {
  if (!SD.exists("/history.csv"))
    return "";

  File indexFile = SD.open("/history.csv", FILE_READ);
  if (!indexFile)
    return "";

  String content = "";
  content.reserve(indexFile.size());
  while (indexFile.available()) {
    content += (char)indexFile.read();
  }
  indexFile.close();
  return content;
}

bool SessionManager::deleteSession(String filename) {
  // 1. Remove the actual log file
  if (SD.exists(filename)) {
    SD.remove(filename);
    Serial.println("Deleted log file: " + filename);
  } else {
    Serial.println("Log file not found: " + filename);
    // Proceed to clean index anyway
  }

  // 2. Rewrite History Index
  if (!SD.exists("/history.csv"))
    return false;

  File inFile = SD.open("/history.csv", FILE_READ);
  if (!inFile)
    return false;

  String tempPath = "/history.tmp";
  File outFile = SD.open(tempPath, FILE_WRITE);
  if (!outFile) {
    inFile.close();
    return false;
  }

  bool found = false;
  while (inFile.available()) {
    String line = inFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    // Check if this line contains the filename
    // Format: /sessions/run_X.csv,Date,...
    int c1 = line.indexOf(',');
    if (c1 > 0) {
      String fName = line.substring(0, c1);
      if (fName == filename) {
        found = true;
        Serial.println("Skipping deleted entry in index: " + fName);
        continue; // Skip this line
      }
    }
    outFile.println(line);
  }

  inFile.close();
  outFile.close();

  SD.remove("/history.csv");
  SD.rename(tempPath, "/history.csv");

  return true;
}

bool SessionManager::getSDStatus(uint64_t &total, uint64_t &used) {
  if (!SD.totalBytes())
    return false; // Periksa apakah terpasang/valid
  total = SD.totalBytes();
  used = SD.usedBytes();
  return true;
}

SessionManager::SDTestResult
SessionManager::runFullTest(void (*progressCallback)(int, String)) {
  SDTestResult res;
  res.success = false;
  res.readSpeedKBps = 0;
  res.writeSpeedKBps = 0;

  if (!SD.totalBytes()) {
    res.cardType = "NO CARD";
    return res;
  }

  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();

  // Format Size Label
  float totalGB = total / (1024.0 * 1024.0 * 1024.0);
  res.sizeLabel = String(totalGB, 1) + " GB";

  float usedMB = used / (1024.0 * 1024.0);
  res.usedLabel = String(usedMB, 0) + " MB";

  sdcard_type_t t = SD.cardType();
  if (t == CARD_MMC)
    res.cardType = "MMC";
  else if (t == CARD_SD)
    res.cardType = "SDSC";
  else if (t == CARD_SDHC)
    res.cardType = "SDHC";
  else
    res.cardType = "UNKNOWN";

  // Benchmark Write
  if (progressCallback)
    progressCallback(0, "Writing...");

  uint8_t buf[512]; // Small buffer
  memset(buf, 0xAA, 512);
  String testFile = "/test_bench.bin";

  unsigned long start = millis();
  File f = SD.open(testFile, FILE_WRITE);
  if (f) {
    // Write 1MB (2048 * 512)
    int chunks = 2048;
    for (int i = 0; i < chunks; i++) {
      f.write(buf, 512);
      if (i % 200 == 0 && progressCallback) { // Update every ~10% of phase
        int p = (i * 50) / chunks;            // 0-50%
        progressCallback(p, "Writing...");
      }
    }
    f.close();
    unsigned long duration = millis() - start;
    if (duration > 0) {
      res.writeSpeedKBps = 1024.0 / (duration / 1000.0);
    }
  } else {
    return res; // Write failed
  }

  // Benchmark Read
  if (progressCallback)
    progressCallback(50, "Reading...");

  start = millis();
  f = SD.open(testFile, FILE_READ);
  if (f) {
    long len = f.size();
    long pos = 0;
    int chunks = 0;

    while (f.available()) {
      f.read(buf, 512);
      pos += 512;
      chunks++;

      if (chunks % 200 == 0 && progressCallback) {
        int p = 50 + (pos * 50) / len; // 50-100%
        progressCallback(p, "Reading...");
      }
    }
    f.close();
    unsigned long duration = millis() - start;
    if (duration > 0) {
      res.readSpeedKBps = 1024.0 / (duration / 1000.0);
    }
  } else {
    return res; // Read failed
  }

  // Cleanup
  if (progressCallback)
    progressCallback(100, "Done!");
  SD.remove(testFile);

  res.success = true;
  return res;
}

SessionManager::SessionAnalysis
SessionManager::analyzeSession(String filename) {
  SessionAnalysis result;
  result.totalTime = 0;
  result.totalDistance = 0;
  result.maxSpeed = 0;
  result.avgSpeed = 0;
  result.validLaps = 0;
  result.bestLap = 0;

  File f = SD.open(filename, FILE_READ);
  if (!f)
    return result;

  unsigned long firstTime = 0;
  unsigned long lastTime = 0;
  double prevLat = 0;
  double prevLon = 0;
  bool firstPoint = true;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    if (line.startsWith("LAP,")) {
      // LAP,Count,Time
      int lastComma = line.lastIndexOf(',');
      if (lastComma > 0) {
        unsigned long t = line.substring(lastComma + 1).toInt();
        result.lapTimes.push_back(t);
        result.validLaps++;
        if (result.bestLap == 0 || t < result.bestLap)
          result.bestLap = t;
      }
    } else if (line.startsWith("SECTOR,")) {
      // SECTOR,Lap,Num,Time
      int c1 = line.indexOf(',');
      int c2 = line.indexOf(',', c1 + 1);
      int c3 = line.indexOf(',', c2 + 1);
      if (c3 > 0) {
        int num = line.substring(c2 + 1, c3).toInt();
        unsigned long t = line.substring(c3 + 1).toInt();
        if (num == 1)
          result.sector1.push_back(t);
        else if (num == 2)
          result.sector2.push_back(t);
        else if (num == 3)
          result.sector3.push_back(t);
      }
    } else {
      // Data line
      if (!isdigit(line.charAt(0)) && line.charAt(0) != '-')
        continue;

      // Time,Lat,Lon,Speed...
      int p1 = line.indexOf(',');
      int p2 = line.indexOf(',', p1 + 1); // Lat
      int p3 = line.indexOf(',', p2 + 1); // Lon
      int p4 = line.indexOf(',', p3 + 1); // Speed

      if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0) {
        unsigned long t = line.substring(0, p1).toInt();
        double lat = line.substring(p1 + 1, p2).toDouble();
        double lon = line.substring(p2 + 1, p3).toDouble();
        float speed = line.substring(p3 + 1, p4).toFloat();

        if (firstPoint) {
          firstTime = t;
          prevLat = lat;
          prevLon = lon;
          firstPoint = false;
        } else {
          // Haversine
          float dLat = (lat - prevLat) * DEG_TO_RAD;
          float dLon = (lon - prevLon) * DEG_TO_RAD;
          float a = sin(dLat / 2) * sin(dLat / 2) +
                    cos(prevLat * DEG_TO_RAD) * cos(lat * DEG_TO_RAD) *
                        sin(dLon / 2) * sin(dLon / 2);
          float c = 2 * atan2(sqrt(a), sqrt(1 - a));
          float dist = 6371000 * c; // meters
          if (dist > 0.5) {
            result.totalDistance += (dist / 1000.0); // Add to km
          }
          prevLat = lat;
          prevLon = lon;
        }
        lastTime = t;
        if (speed > result.maxSpeed)
          result.maxSpeed = speed;
      }
    }
  }
  f.close();

  if (lastTime > firstTime) {
    result.totalTime = lastTime - firstTime;
    float hours = result.totalTime / 3600000.0;
    if (hours > 0)
      result.avgSpeed = result.totalDistance / hours;
  }

  // --- Drag Analysis ---
  // Re-read file or process captured logic?
  // Since we processed it sequentially, we need to track drag starts.
  // Assuming the file IS a drag run, it starts around 0 speed.
  // We can re-scan or do it in the first pass.
  // Let's simplified re-scan: Find 0-60, 0-100, etc.
  // Constraint: GPS is 10Hz or 25Hz. Linear interpolation needed for accuracy.

  result.time0to60 = 0;
  result.time0to100 = 0;
  result.time100to200 = 0;
  result.time400m = 0;

  f = SD.open(filename, FILE_READ);
  if (!f)
    return result;

  unsigned long startTime = 0;
  bool started = false;
  double startLat = 0, startLon = 0;
  unsigned long time100 = 0;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || !isdigit(line.charAt(0)))
      continue;

    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);

    if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0) {
      unsigned long t = line.substring(0, p1).toInt();
      double lat = line.substring(p1 + 1, p2).toDouble();
      double lon = line.substring(p2 + 1, p3).toDouble();
      float speed = line.substring(p3 + 1, p4).toFloat();

      if (!started && speed > 1.0) {
        started = true;
        startTime = t;
        startLat = lat;
        startLon = lon;
      }

      if (started) {
        unsigned long runTime = t - startTime;

        // 0-60 km/h
        if (result.time0to60 == 0 && speed >= 60.0) {
          result.time0to60 = runTime;
        }
        // 0-100 km/h
        if (result.time0to100 == 0 && speed >= 100.0) {
          result.time0to100 = runTime;
          time100 = t;
        }
        // 100-200 km/h
        if (time100 > 0 && result.time100to200 == 0 && speed >= 200.0) {
          result.time100to200 = t - time100;
        }

        // Distance (Cheap Calc) - Real implementation should accumulate
        // strictly For simplicity, we assume straight line from start if drag?
        // No, accumulate. We'll trust totalDistance Logic or re-calc distance
        // from start. Let's use Haversine from START point for Drag Distance
        float dLat = (lat - startLat) * DEG_TO_RAD;
        float dLon = (lon - startLon) * DEG_TO_RAD;
        float a = sin(dLat / 2) * sin(dLat / 2) +
                  cos(startLat * DEG_TO_RAD) * cos(lat * DEG_TO_RAD) *
                      sin(dLon / 2) * sin(dLon / 2);
        float c = 2 * atan2(sqrt(a), sqrt(1 - a));
        float distFromStart = 6371000 * c; // meters

        if (result.time400m == 0 && distFromStart >= 402.336) { // 1/4 mile
          result.time400m = runTime;
        }
      }
    }
  }
  f.close();
  return result;
}

// --- Predictive Timing ---
bool SessionManager::loadBestLapAsReference(String filename) {
  referenceLap.clear();
  File f = SD.open(filename, FILE_READ);
  if (!f)
    return false;

  // Pass 1: Find Best Lap Index
  int bestLapIdx = -1;
  unsigned long bestTime = 0;
  int currentLap = 0; // 0-indexed count

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("LAP,")) {
      // LAP,Count,Time
      int lastComma = line.lastIndexOf(',');
      if (lastComma > 0) {
        unsigned long t = line.substring(lastComma + 1).toInt();
        if (bestLapIdx == -1 || t < bestTime) {
          bestTime = t;
          bestLapIdx = currentLap;
        }
      }
      currentLap++;
    }
  }

  if (bestLapIdx == -1) {
    f.close();
    return false; // No laps found
  }

  // Pass 2: Extract Points for Best Lap
  f.seek(0);
  currentLap = 0;
  bool collecting = (currentLap == bestLapIdx);

  double prevLat = 0, prevLon = 0;
  float totalDist = 0;
  unsigned long lapStartTime = 0;
  bool firstPoint = true;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    if (line.startsWith("LAP,")) {
      if (currentLap == bestLapIdx) {
        break;
      }
      currentLap++;
      collecting = (currentLap == bestLapIdx);
      if (collecting) {
        totalDist = 0;
        firstPoint = true;
        lapStartTime = 0;
      }
      continue;
    }

    if (collecting && isdigit(line.charAt(0))) {
      int p1 = line.indexOf(',');
      int p2 = line.indexOf(',', p1 + 1);
      int p3 = line.indexOf(',', p2 + 1);
      if (p1 > 0 && p2 > 0 && p3 > 0) {
        unsigned long t = line.substring(0, p1).toInt();
        double lat = line.substring(p1 + 1, p2).toDouble();
        double lon = line.substring(p2 + 1, p3).toDouble();

        if (firstPoint) {
          lapStartTime = t;
          prevLat = lat;
          prevLon = lon;
          totalDist = 0;
          firstPoint = false;
          referenceLap.push_back({0, 0});
        } else {
          float dLat = (lat - prevLat) * DEG_TO_RAD;
          float dLon = (lon - prevLon) * DEG_TO_RAD;
          float a = sin(dLat / 2) * sin(dLat / 2) +
                    cos(prevLat * DEG_TO_RAD) * cos(lat * DEG_TO_RAD) *
                        sin(dLon / 2) * sin(dLon / 2);
          float c = 2 * atan2(sqrt(a), sqrt(1 - a));
          float dist = 6371000 * c;

          if (dist > 0.5) {
            totalDist += dist;
            unsigned long relTime = t - lapStartTime;
            referenceLap.push_back({totalDist, (uint32_t)relTime});
            prevLat = lat;
            prevLon = lon;
          }
        }
      }
    }
  }

  f.close();
  return !referenceLap.empty();
}

float SessionManager::getReferenceTime(float distance) {
  if (referenceLap.empty())
    return -1.0;

  auto it = std::lower_bound(
      referenceLap.begin(), referenceLap.end(), distance,
      [](const ReferencePoint &a, float val) { return a.distance < val; });

  if (it == referenceLap.begin())
    return referenceLap[0].time;
  if (it == referenceLap.end())
    return referenceLap.back().time;

  const ReferencePoint &p2 = *it;
  const ReferencePoint &p1 = *(it - 1);

  float distDelta = p2.distance - p1.distance;
  if (distDelta < 0.1)
    return p1.time;

  float ratio = (distance - p1.distance) / distDelta;
  return p1.time + (p2.time - p1.time) * ratio;
}

void SessionManager::wipeSDData() {
  if (!SD.totalBytes())
    return;

  auto clearFolder = [](String path) {
    File root = SD.open(path);
    if (!root || !root.isDirectory())
      return;

    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      // Ensure path is correct for removal
      String fullPath = path;
      if (!fullPath.endsWith("/"))
        fullPath += "/";
      if (fileName.startsWith("/")) {
        SD.remove(fileName);
      } else {
        SD.remove(fullPath + fileName);
      }
      file = root.openNextFile();
    }
  };

  // 1. Clear contents of known data folders
  clearFolder("/sessions");
  clearFolder("/tracks");
  clearFolder("/uploaded_tracks");

  // 2. Remove history index & wifi details
  if (SD.exists("/history.csv")) {
    SD.remove("/history.csv");
  }
  if (SD.exists("/wifi.txt")) {
    SD.remove("/wifi.txt");
  }

  Serial.println("SD Data Wipe Complete");
}
