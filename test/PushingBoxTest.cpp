#include <Arduino.h>
#include <ArduinoUnitTests.h>

#include "Devices/EEPROM_TC.h"
#include "Devices/EthernetServer_TC.h"
#include "Devices/PushingBox.h"
#include "Devices/TempProbe_TC.h"
#include "Devices/TemperatureControl.h"
#include "TankControllerLib.h"

unittest(NoTankID) {
  // set tank id to 0, set time interval to 1 minute
  EEPROM_TC::instance()->setTankID(0);
  EEPROM_TC::instance()->setGoogleSheetInterval(1);

  GodmodeState *state = GODMODE();
  PushingBox *pPushingBox = PushingBox::instance();
  TankControllerLib *pTC = TankControllerLib::instance();
  pTC->loop();
  delay(75 * 1000);  // a bit over one minute
  state->serialPort[0].dataOut = "";
  pTC->loop();
  char expected[] = "Set Tank ID in order to send data to PushingBox\r\n";
  assertEqual(expected, state->serialPort[0].dataOut);
}

unittest(SendData) {
  GodmodeState *state = GODMODE();
  state->reset();
  state->serialPort[0].dataOut = "";
  PushingBox *pPushingBox = PushingBox::instance();
  TankControllerLib *pTC = TankControllerLib::instance();
  TempProbe_TC *tempProbe = TempProbe_TC::instance();

  // set tank id
  EEPROM_TC::instance()->setTankID(99);

  // set temperature
  TemperatureControl::instance()->setTargetTemperature(20.25);
  tempProbe->setTemperature(20.25);
  tempProbe->setCorrection(0.0);
  for (int i = 0; i < 100; ++i) {
    delay(1000);
    tempProbe->getRunningAverage();
  }

  // set pH
  state->serialPort[1].dataIn = "7.125\r";  // the queue of data waiting to be read
  pTC->serialEvent1();                      // fake interrupt

  EthernetClient::startMockServer(pPushingBox->getServer(), 80);
  assertEqual(0, pPushingBox->getClient()->writeBuffer().size());
  pPushingBox->getClient()->pushToReadBuffer('A');
  state->serialPort[0].dataOut = "";
  delay(60 * 1000);  // wait for one minute to ensure we send again
  pTC->loop();
  deque<uint8_t> buffer = pPushingBox->getClient()->writeBuffer();
  String bufferResult;
  for (int i = 0; i < buffer.size(); i++) {
    bufferResult += buffer[i];
  }
  char expected1[] =
      "GET /pushingbox?devid=v172D35C152EDA6C&tankid=99&tempData=20.26&pHdata=7.125 HTTP/1.1\r\n"
      "Host: api.pushingbox.com\r\n"
      "Connection: close\r\n"
      "\r\n";

  assertEqual(expected1, bufferResult.c_str());
  char expected2[] =
      "GET /pushingbox?devid=v172D35C152EDA6C&tankid=99&tempData=20.26&pHdata=7.125 HTTP/1.1\r\n"
      "Host: api.pushingbox.com\r\n"
      "Connection: close\r\n"
      "\r\n"
      "\r\n"
      "attempting to connect to PushingBox...\r\n"
      "connected\r\n"
      "A";
  assertEqual(expected2, state->serialPort[0].dataOut);
  EthernetClient::stopMockServer(pPushingBox->getServer(), 80);
}

unittest_main()
