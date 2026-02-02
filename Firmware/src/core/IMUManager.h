#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#include "../config.h"
#include <MPU6050_light.h>
#include <Wire.h>

class IMUManager {
public:
  IMUManager();
  void begin();
  void update();
  void calibrate();

  float getAngleX() { return _angleX; }
  float getAngleY() { return _angleY; }
  float getAngleZ() { return _angleZ; }

  float getAccX() { return _accX; }
  float getAccY() { return _accY; }
  float getAccZ() { return _accZ; }

  bool isConnected() { return _isConnected; }

  bool isEnabled() { return _isEnabled; }
  void setEnabled(bool enabled) { _isEnabled = enabled; }

  void setRollOffset(float offset) { _rollOffset = offset; }
  float getRollOffset() { return _rollOffset; }
  void setPitchOffset(float offset) { _pitchOffset = offset; }
  float getPitchOffset() { return _pitchOffset; }

  void calibrateLevel(); // Set current orientation as 0,0
  void saveSettings();

private:
  MPU6050 _mpu;
  bool _isConnected;
  bool _isEnabled;
  float _angleX, _angleY, _angleZ;
  float _accX, _accY, _accZ;
  float _rollOffset, _pitchOffset;
  unsigned long _lastUpdate;
};

extern IMUManager imuManager;

#endif
