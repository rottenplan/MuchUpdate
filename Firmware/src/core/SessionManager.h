#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include "../config.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <vector>

class SessionManager {
public:
  void begin();

  bool startSession();
  void stopSession();
  void logData(String dataLine); // Simplified logging

  bool isLogging() { return _logging; }

  void appendToHistoryIndex(String filename, String date, int laps,
                            unsigned long bestLap, String type = "TRACK");
  String loadHistoryIndex(); // Returns full content for processing

  bool deleteSession(String filename); // Delete file and update index
  void wipeSDData();                   // Wipe sessions, tracks and history

  bool getSDStatus(uint64_t &total, uint64_t &used);

  struct SDTestResult {
    bool success;
    String cardType;
    String sizeLabel; // e.g. "16 GB"
    String usedLabel; // e.g. "200 MB"
    float readSpeedKBps;
    float writeSpeedKBps;
  };

  SDTestResult runFullTest(void (*progressCallback)(int, String) = NULL);

  struct SessionAnalysis {
    unsigned long totalTime;
    float totalDistance; // km
    float maxSpeed;      // km/h
    float avgSpeed;      // km/h
    int validLaps;
    std::vector<unsigned long> lapTimes;
    unsigned long bestLap;
    // Drag Metrics
    unsigned long time0to60;
    unsigned long time0to100;
    unsigned long time100to200;
    unsigned long time400m;
    std::vector<unsigned long> sector1;
    std::vector<unsigned long> sector2;
    std::vector<unsigned long> sector3;
  };

  SessionAnalysis analyzeSession(String filename);

  struct ReferencePoint {
    float distance; // Meters from start of LAP
    uint32_t time;  // ms from start of LAP
  };
  std::vector<ReferencePoint> referenceLap;
  bool loadBestLapAsReference(String filename); // Loads best lap from session
  float getReferenceTime(float distance);       // Interp logic

private:
  bool _logging;
  File _logFile;
  String createFilename();
  String _currentFilename;

public:
  String getCurrentFilename() { return _currentFilename; }

private:
  QueueHandle_t _logQueue;
  TaskHandle_t _loggingTaskHandle;
  static void loggingTask(void *parameter);
};

#endif
