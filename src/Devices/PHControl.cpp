#include "PHControl.h"

#include "Devices/EEPROM_TC.h"
#include "Devices/Serial_TC.h"
#include "PID_TC.h"
#include "TC_util.h"

const double DEFAULT_PH = 7.125;

//  class instance variables
/**
 * static variable for singleton
 */
PHControl *PHControl::_instance = nullptr;

//  class methods
/**
 * static member function to return singleton
 */
PHControl *PHControl::instance() {
  if (!_instance) {
    _instance = new PHControl();
  }
  return _instance;
}

PHControl::PHControl() {
  window_start_time = millis();
  digitalWrite(PIN, HIGH);
  targetPh = EEPROM_TC::instance()->getPH();
  if (isnan(targetPh)) {
    targetPh = DEFAULT_PH;
  }
}

void PHControl::setTargetPh(double newPh) {
  targetPh = newPh;
  EEPROM_TC::instance()->setPH(newPh);
}

void PHControl::updateControl(double pH) {
  if (usePID) {
    onTime = PID_TC::instance()->computeOutput(targetPh, pH);
  } else {
    onTime = pH > targetPh ? 10000 : 0;
  }
  long now = millis();
  if (now - window_start_time > WINDOW_SIZE) {  // time to shift the Relay Window
    window_start_time += WINDOW_SIZE;
    COUT("now: " << now << "; window_start_time: " << window_start_time);
  }
  COUT("target: " << targetPh << "; current: " << pH << "; onTime = " << onTime
                  << "; left = " << now - window_start_time);
  bool oldValue = digitalRead(PIN);
  bool newValue = oldValue;
  if ((onTime > SOLENOID_OPENING_TIME) && (onTime >= (now - window_start_time))) {
    newValue = LOW;  // open CO2 solenoid
  } else {
    newValue = HIGH;  // close CO2 solenoid
  }
  if (newValue != oldValue) {
    Serial_TC::instance()->ts_printf((const char *)(newValue ? F("CO2 bubbler off") : F("CO2 bubbler on")));
    digitalWrite(PIN, newValue);
  }
}
