#include "SyncManager.h"
#include "SessionManager.h"
#include <base64.h>
#include <time.h>
extern SessionManager sessionManager;

SyncManager::SyncManager() {}

bool SyncManager::isFirstSyncDone() {
  _prefs.begin("sync", false);
  bool done = _prefs.getBool("first_sync_done", false);
  _prefs.end();
  return done;
}

String SyncManager::getLastSyncTime() {
  _prefs.begin("sync", false);
  String lastSync = _prefs.getString("last_sync", "Never");
  _prefs.end();
  return lastSync;
}

String SyncManager::makeBasicAuthHeader(const char *username,
                                        const char *password) {
  String credentials = String(username) + ":" + String(password);
  String encoded = base64::encode(credentials);
  return "Basic " + encoded;
}

bool SyncManager::performFirstSync(const char *apiUrl, const char *username,
                                   const char *password) {
  // Check if first sync already done
  if (isFirstSyncDone()) {
    Serial.println("Sync: First sync already completed.");
    return true;
  }

  Serial.println("Sync: Performing first sync...");

  // Perform sync
  bool success = syncSettings(apiUrl, username, password);

  if (success) {
    markSyncComplete();
    Serial.println("Sync: First sync completed successfully!");
  } else {
    Serial.println("Sync: First sync failed.");
  }

  return success;
}

bool SyncManager::syncSettings(const char *apiUrl, const char *username,
                               const char *password) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sync: WiFi not connected.");
    return false;
  }
  Serial.print("Sync: WiFi Connected. Device IP: ");
  Serial.println(WiFi.localIP());

  String authHeader = makeBasicAuthHeader(username, password);
  bool success = downloadAndApplySettings(apiUrl, authHeader.c_str());

  if (success) {
    // Also sync tracks
    downloadTracks(apiUrl, authHeader.c_str());
    saveLastSyncTime();
  }

  return success;
}

bool SyncManager::downloadAndApplySettings(const char *apiUrl,
                                           const char *authHeader) {
  HTTPClient http;

  // Append Status Query Params
  uint64_t totalBytes = SD.totalBytes();
  uint64_t usedBytes = SD.usedBytes();
  // Convert to MB
  int totalMB = (int)(totalBytes / (1024 * 1024));
  int usedMB = (int)(usedBytes / (1024 * 1024));

  String url = String(apiUrl) + "?storage_used=" + String(usedMB) +
               "&storage_total=" + String(totalMB);

  Serial.print("Sync: Connecting to ");
  Serial.println(url);

  http.begin(url);
  http.addHeader("Authorization", authHeader);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Sync: Response received");

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("Sync: JSON parse error: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    // Check if response has success flag
    if (!doc["success"].as<bool>()) {
      Serial.println("Sync: API returned success=false");
      http.end();
      return false;
    }

    // Apply settings
    applySettings(doc);
    applyTrackSelection(doc);
    applyEngineSettings(doc);

    http.end();
    return true;

  } else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Sync: Authentication failed (401)");
  } else {
    Serial.print("Sync: HTTP error: ");
    Serial.println(httpCode);
  }

  http.end();
  return false;
}

void SyncManager::applySettings(JsonDocument &doc) {
  if (!doc["data"]["settings"].isNull()) {
    JsonObject settings = doc["data"]["settings"];

    _prefs.begin("laptimer", false);

    // Units (0=Metric, 1=Imperial)
    String units = settings["units"].as<String>();
    _prefs.putInt("units", units == "kmh" ? 0 : 1);

    // Brightness (0-9 for 10%-100%)
    int brightness = settings["brightness"].as<int>();
    int brightnessIdx = map(brightness, 10, 100, 0, 9);
    _prefs.putInt("brightness", brightnessIdx);

    // Power Save (0=1min, 1=5min, 2=10min, 3=30min, 4=Never)
    int powerSave = settings["powerSave"].as<int>();
    int powerSaveIdx = 1; // Default 5 min
    switch (powerSave) {
    case 1:
      powerSaveIdx = 0;
      break;
    case 5:
      powerSaveIdx = 1;
      break;
    case 10:
      powerSaveIdx = 2;
      break;
    case 30:
      powerSaveIdx = 3;
      break;
    case 0:
      powerSaveIdx = 4;
      break;
    }
    _prefs.putInt("power_save", powerSaveIdx);

    // Contrast (store directly)
    int contrast = settings["contrast"].as<int>();
    _prefs.putInt("contrast", contrast);

    _prefs.end();

    Serial.println("Sync: Device settings applied");
  }
}

void SyncManager::applyTrackSelection(JsonDocument &doc) {
  if (!doc["data"]["tracks"].isNull()) {
    JsonObject tracks = doc["data"]["tracks"];

    // Store track country selection
    _prefs.begin("tracks", false);

    JsonArray countries = tracks["countries"];
    String countryList = "";
    for (JsonVariant country : countries) {
      if (countryList.length() > 0)
        countryList += ",";
      countryList += country.as<String>();
    }

    _prefs.putString("countries", countryList);
    _prefs.putInt("track_count", tracks["trackCount"].as<int>());

    _prefs.end();

    Serial.print("Sync: Track selection applied - ");
    Serial.print(tracks["trackCount"].as<int>());
    Serial.println(" tracks");
  }
}

void SyncManager::applyEngineSettings(JsonDocument &doc) {
  if (!doc["data"]["engines"].isNull()) {
    JsonArray engines = doc["data"]["engines"];

    _prefs.begin("laptimer", false);

    // Store active engine
    int activeEngine = doc["data"]["activeEngine"].as<int>();
    _prefs.putInt("active_engine", activeEngine);

    // Store engine hours for active engine
    for (JsonVariant engine : engines) {
      if (engine["id"].as<int>() == activeEngine) {
        float hours = engine["hours"].as<float>();
        _prefs.putFloat("engine_hours", hours);
        Serial.print("Sync: Engine hours set to ");
        Serial.println(hours);
        break;
      }
    }

    _prefs.end();

    Serial.println("Sync: Engine settings applied");
  }
}

void SyncManager::markSyncComplete() {
  _prefs.begin("sync", false);
  _prefs.putBool("first_sync_done", true);
  _prefs.end();
}

void SyncManager::saveLastSyncTime() {
  // Get current time as ISO string
  time_t now = time(nullptr);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  char timeStr[25];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  _prefs.begin("sync", false);
  _prefs.putString("last_sync", String(timeStr));
  _prefs.end();

  Serial.print("Sync: Last sync time saved: ");
  Serial.println(timeStr);
}

void SyncManager::triggerManualSync() {
  // This would be called from Settings menu
  // For now, just a placeholder
  Serial.println("Sync: Manual sync triggered");
}

bool SyncManager::uploadSessions(const char *apiUrl, const char *username,
                                 const char *password) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  String authHeader = makeBasicAuthHeader(username, password);
  String history = sessionManager.loadHistoryIndex();

  // Simple line parsing
  int lastIndex = 0;
  while (true) {
    int nextNewLine = history.indexOf('\n', lastIndex);
    if (nextNewLine == -1)
      break;

    String line = history.substring(lastIndex, nextNewLine);
    line.trim();
    lastIndex = nextNewLine + 1;

    if (line.length() == 0)
      continue;

    // CSV: filename,date,laps...
    int commaIndex = line.indexOf(',');
    if (commaIndex == -1)
      continue;

    String filename = line.substring(0, commaIndex);

    // Upload logic
    if (SD.exists(filename)) {
      File f = SD.open(filename, FILE_READ);
      if (!f)
        continue;

      String content = f.readString(); // Much faster than byte-by-byte
      f.close();

      HTTPClient http;
      http.begin(apiUrl);
      http.addHeader("Authorization", authHeader);
      http.addHeader("Content-Type", "application/json");

      JsonDocument doc;
      doc["type"] = "upload_session";
      doc["filename"] = filename;
      doc["csv_data"] = content;

      String jsonPayload;
      serializeJson(doc, jsonPayload);

      int code = http.POST(jsonPayload);
      http.end();
      if (code != 200) {
        Serial.println("Failed to upload: " + filename);
      } else {
        Serial.println("Uploaded: " + filename);
      }
    }
  }
  return true;
}

bool SyncManager::downloadTracks(const char *apiUrl, const char *authHeader) {
  // Construct URL: Replace "api/device/sync" with "api/tracks/list"
  String url = String(apiUrl);
  int idx = url.indexOf("/api/device/sync");
  if (idx > 0) {
    url = url.substring(0, idx) + "/api/tracks/list";
  } else {
    Serial.println("Sync: Invalid API URL format for tracks");
    return false;
  }

  HTTPClient http;
  Serial.print("Sync: Downloading tracks from ");
  Serial.println(url);

  http.begin(url);
  http.addHeader("Authorization", authHeader);

  int httpCode = http.GET();
  bool success = false;

  if (httpCode == HTTP_CODE_OK) {
    // Stream directly to SD card to save RAM
    File file = SD.open("/tracks.json", FILE_WRITE);
    if (file) {
      http.writeToStream(&file);
      file.close();
      Serial.println("Sync: Tracks saved to /tracks.json");
      success = true;
    } else {
      Serial.println("Sync: Failed to open /tracks.json for writing");
    }
  } else {
    Serial.print("Sync: HTTP error downloading tracks: ");
    Serial.println(httpCode);
  }

  http.end();
  return success;
}
bool SyncManager::uploadGPXTracks(const char *apiUrl, const char *username,
                                  const char *password) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  String authHeader = makeBasicAuthHeader(username, password);

  // Construct Upload URL (Assumption: /api/tracks/upload)
  String url = String(apiUrl);
  int idx = url.indexOf("/api/device/sync");
  if (idx > 0) {
    url = url.substring(0, idx) + "/api/tracks/upload";
  } else {
    // Fallback or just append if base url is different
    // Assuming apiUrl was the sync endpoint, usually
    // "https://domain.com/api/device/sync" We want
    // "https://domain.com/api/tracks/upload"
    Serial.println("Sync: Invalid API URL format for track upload");
    return false;
  }

  Serial.println("Sync: Scanning for GPX tracks in /tracks/...");

  if (!SD.exists("/tracks")) {
    Serial.println("Sync: /tracks directory not found.");
    return true; // Nothing to upload, technically success
  }

  File root = SD.open("/tracks");
  if (!root || !root.isDirectory()) {
    Serial.println("Sync: Failed to open /tracks directory.");
    return false;
  }

  bool allSuccess = true;
  File file = root.openNextFile();

  while (file) {
    String filename = String(file.name());

    // Only process .gpx files (skip hidden files or others)
    if (!file.isDirectory() && filename.endsWith(".gpx")) {
      Serial.print("Sync: Found track: ");
      Serial.println(filename);

      // Read File Content
      String gpxData = "";
      while (file.available()) {
        gpxData += (char)file.read();
      }

      // Prepare Upload
      HTTPClient http;
      http.begin(url);
      http.addHeader("Authorization", authHeader);
      http.addHeader("Content-Type", "application/json");

      JsonDocument doc;
      doc["filename"] = filename;
      doc["gpx_data"] = gpxData; // Sending raw XML in JSON string

      String jsonPayload;
      serializeJson(doc, jsonPayload);

      Serial.println("Sync: Uploading...");
      int httpCode = http.POST(jsonPayload);
      http.end();

      if (httpCode == HTTP_CODE_OK || httpCode == 201) {
        Serial.println("Sync: Upload success!");

        // Move to /uploaded_tracks/
        if (!SD.exists("/uploaded_tracks")) {
          SD.mkdir("/uploaded_tracks");
        }

        // Rename (Move)
        // Note: SD.rename requires full paths
        String oldPath = "/tracks/" + filename;
        String newPath = "/uploaded_tracks/" + filename;

        // Close file before renaming? (It was read fully, but object persists)
        // file object is invalid after openNextFile loop usually, but here we
        // still have it Need to close current file handle? 'file' is a wrapper
        // around the directory entry iterator in some SD libs. But we read from
        // it. Let's make sure we don't hold a lock. file.close() is implicit or
        // we can just ignore since we read to end? Actually, standard SD lib:
        // file needs to be closed? But the loop uses 'file =
        // root.openNextFile()'. We should probably rely on the fact we read it
        // all. Wait! We can't rename an open file. But 'file' is the handle. We
        // must close it first. BUT openNextFile returns an open file object. We
        // need to close 'file' before renaming. However, the loop depends on
        // 'file'. Actually typical pattern: File f = root.openNextFile();
        // ... work ...
        // f.close();

        // The issue: openNextFile updates the internal pointer of 'root'.
        // So closing 'file' is fine.
      } else {
        Serial.print("Sync: Upload failed. Code: ");
        Serial.println(httpCode);
        allSuccess = false;
      }

      // We must close the file to release the handle, especially before
      // renaming
      file.close();

      if (httpCode == HTTP_CODE_OK || httpCode == 201) {
        // Rename now that it's closed
        String oldPath = "/tracks/" + filename;
        String newPath = "/uploaded_tracks/" + filename;
        if (SD.rename(oldPath, newPath)) {
          Serial.println("Sync: Moved to " + newPath);
        } else {
          Serial.println("Sync: Failed to move file!");
        }
      }

    } else {
      file.close(); // Not a gpx, close and move on
    }

    file = root.openNextFile();
  }

  root.close();
  return allSuccess;
}
