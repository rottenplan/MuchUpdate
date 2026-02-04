#include "IMUManager.h"
#include <Preferences.h>

IMUManager::IMUManager() : _mpu(Wire), _isConnected(false), _lastUpdate(0) {
  _angleX = _angleY = _angleZ = 0;
  _accX = _accY = _accZ = 0;
  _rollOffset = _pitchOffset = 0;
  _accXOffset = _accYOffset = 0;
  _isEnabled = true;
}

void IMUManager::begin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  byte status = _mpu.begin();
  if (status == 0) {
    _isConnected = true;
    Serial.println("MPU6050: Connected!");
    _mpu.calcOffsets(true,
                     false); // Calibrate only Gyro at startup to prevent drift
                             // while preserving manual Accel level calibration.

    // Load user offsets and enabled state
    Preferences prefs;
    prefs.begin("laptimer", true);
    _rollOffset = prefs.getFloat("imu_roll_off", 0.0);
    _pitchOffset = prefs.getFloat("imu_pitch_off", 0.0);
    _accXOffset = prefs.getFloat("imu_acc_x_off", 0.0);
    _accYOffset = prefs.getFloat("imu_acc_y_off", 0.0);
    _isEnabled = prefs.getBool("imu_enabled", true);
    prefs.end();
  } else {
    _isConnected = false;
    Serial.print("MPU6050: Connection failed with status ");
    Serial.println(status);
  }
}

void IMUManager::update() {
  if (!_isConnected || !_isEnabled)
    return;

  if (millis() - _lastUpdate > 10) { // 100Hz update rate
    _mpu.update();

    // Apply manual offsets
    _angleX = _mpu.getAngleX() - _rollOffset;
    _angleY = _mpu.getAngleY() - _pitchOffset;
    _angleZ = _mpu.getAngleZ();

    _accX = _mpu.getAccX() - _accXOffset;
    _accY = _mpu.getAccY() - _accYOffset;
    _accZ = _mpu.getAccZ();

    _lastUpdate = millis();
  }
}

void IMUManager::calibrate() {
  if (_isConnected) {
    Serial.println("MPU6050: Calibrating Sensor Bias...");
    _mpu.calcOffsets();
    Serial.println("MPU6050: Sensor Calibrated.");
  }
}

void IMUManager::calibrateLevel() {
  if (_isConnected) {
    _mpu.update();
    // Zero out angles
    _rollOffset = _mpu.getAngleX();
    _pitchOffset = _mpu.getAngleY();
    // Zero out G-Force (assuming level surface)
    _accXOffset = _mpu.getAccX();
    _accYOffset = _mpu.getAccY();

    saveSettings();
    Serial.printf("IMU: Level Calibrated. AccXOff: %.3f, AccYOff: %.3f\n",
                  _accXOffset, _accYOffset);
  }
}

void IMUManager::saveSettings() {
  Preferences prefs;
  prefs.begin("laptimer", false);
  prefs.putFloat("imu_roll_off", _rollOffset);
  prefs.putFloat("imu_pitch_off", _pitchOffset);
  prefs.putFloat("imu_acc_x_off", _accXOffset);
  prefs.putFloat("imu_acc_y_off", _accYOffset);
  prefs.putBool("imu_enabled", _isEnabled);
  prefs.end();
}

IMUManager imuManager;
