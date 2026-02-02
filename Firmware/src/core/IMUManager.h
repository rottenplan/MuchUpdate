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

private:
  MPU6050 _mpu;
  bool _isConnected;
  float _angleX, _angleY, _angleZ;
  float _accX, _accY, _accZ;
  unsigned long _lastUpdate;
};

extern IMUManager imuManager;

#endif
