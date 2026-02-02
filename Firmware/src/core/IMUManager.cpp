#include "IMUManager.h"

IMUManager::IMUManager() : _mpu(Wire), _isConnected(false), _lastUpdate(0) {
  _angleX = _angleY = _angleZ = 0;
  _accX = _accY = _accZ = 0;
}

void IMUManager::begin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  byte status = _mpu.begin();
  if (status == 0) {
    _isConnected = true;
    Serial.println("MPU6050: Connected!");
    _mpu.calcOffsets(); // Initial auto-calibration
  } else {
    _isConnected = false;
    Serial.print("MPU6050: Connection failed with status ");
    Serial.println(status);
  }
}

void IMUManager::update() {
  if (!_isConnected)
    return;

  if (millis() - _lastUpdate > 10) { // 100Hz update rate
    _mpu.update();

    _angleX = _mpu.getAngleX();
    _angleY = _mpu.getAngleY();
    _angleZ = _mpu.getAngleZ();

    _accX = _mpu.getAccX();
    _accY = _mpu.getAccY();
    _accZ = _mpu.getAccZ();

    _lastUpdate = millis();
  }
}

void IMUManager::calibrate() {
  if (_isConnected) {
    Serial.println("MPU6050: Calibrating...");
    _mpu.calcOffsets();
    Serial.println("MPU6050: Calibrated.");
  }
}

IMUManager imuManager;
