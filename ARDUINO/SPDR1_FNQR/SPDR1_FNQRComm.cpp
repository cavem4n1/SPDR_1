﻿/*
 * File       Communication class for Freenove Quadruped Robot
 * Author     Ethan Pan @ Freenove (support@freenove.com)
 * Date       2018/03/30
 * Copyright  Copyright © Freenove (http://www.freenove.com)
 * License    Creative Commons Attribution ShareAlike 3.0
 *            (http://creativecommons.org/licenses/by-sa/3.0/legalcode)
 * -----------------------------------------------------------------------------------------------*/

#if defined(__AVR_ATmega2560__)

#include "SPDR1_FNQRComm.h"

Communication* communication = NULL;

Communication::Communication() {}

void Communication::Start()
{

  StartStateLed();

  robotAction.Start();

  communication = this;

  FlexiTimer2::set(20, UpdateService);
  FlexiTimer2::start();


    SetRobotBootState(GetRobotBootState());

  if (robotAction.robot.power.powerGroupAutoSwitch)
    delay(1000);
}


void Communication::ActiveMode()
{
    robotAction.ActiveMode();
}

void Communication::SleepMode()
{
    robotAction.SleepMode();
}

void Communication::SwitchMode()
{
    robotAction.SwitchMode();
}

void Communication::CrawlForward()
{
    robotAction.CrawlForward();
}

void Communication::CrawlBackward()
{
    robotAction.CrawlBackward();
}

void Communication::TurnLeft()
{
    robotAction.TurnLeft();
}

void Communication::TurnRight()
{
    robotAction.TurnRight();
}

void Communication::MoveBody(float x, float y, float z)
{
    robotAction.MoveBody(x, y, z);
}

void Communication::RotateBody(float x, float y, float z, float angle)
{
    robotAction.RotateBody(x, y, z, angle);
}


void Communication::StartStateLed()
{
  pinMode(stateLedPin, OUTPUT);
  digitalWrite(stateLedPin, LOW);
}

void Communication::SetStateLed(bool state)
{
  digitalWrite(stateLedPin, state);
}

void Communication::ReverseStateLed()
{
  stateLedState = !stateLedState;
  SetStateLed(stateLedState);
}

float Communication::GetSupplyVoltage()
{
  return robotAction.robot.power.voltage;
}

void Communication::StartPins()
{
  for (int i = 0; i < 8; i++)
  {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }
}

void Communication::StartSerial()
{
  Serial.begin(115200);
}

void Communication::UpdateSerial()
{
  while (Serial.available() > 0)
  {
    byte inByte = Serial.read();
    if (inByte == Orders::transStart)
      serialInDataCounter = 0;
    serialInData[serialInDataCounter++] = inByte;
    if (inByte == Orders::transEnd)
      break;
  }

  if (serialInData[0] == Orders::transStart && serialInData[serialInDataCounter - 1] == Orders::transEnd)
  {
    HandleOrder(serialInData, OrderSource::FromSerial);
    serialInDataCounter = 0;
  }
}

void Communication::HandleOrder(byte inData[], OrderSource orderSource)
{
  if (blockedOrder != 0)
    return;

  this->orderSource = orderSource;

  byte outData[outDataSize];
  byte outDataCounter = 0;

  outData[outDataCounter++] = Orders::transStart;

  if (inData[1] == Orders::requestEcho)
  {
    outData[outDataCounter++] = Orders::echo;
  }
  else if (inData[1] == Orders::requestSupplyVoltage)
  {
    float supplyVoltage = GetSupplyVoltage();
    outData[outDataCounter++] = Orders::supplyVoltage;
    outData[outDataCounter++] = (int)(supplyVoltage * 100) / 128;
    outData[outDataCounter++] = (int)(supplyVoltage * 100) % 128;
  }
  else if (inData[1] == Orders::requestChangeIO)
  {
    digitalWrite(pins[inData[2]], inData[3]);
    outData[outDataCounter++] = Orders::orderDone;
  }
  else if (inData[1] == Orders::requestMoveLeg)
  {
    robotAction.LegMoveToRelativelyDirectly(inData[2], Point(inData[3] - 64, inData[4] - 64, inData[5] - 64));
    outData[outDataCounter++] = Orders::orderDone;
  }
  else if (inData[1] == Orders::requestCalibrate)
  {
    robotAction.robot.CalibrateServos();
    outData[outDataCounter++] = Orders::orderDone;
  }
  else if (inData[1] >= 64 && inData[1] <= 98)
  {
    blockedOrder = inData[1];
    outData[outDataCounter++] = Orders::orderStart;
  }
  else if (inData[1] == Orders::requestMoveBody)
  {
    blockedOrder = inData[1];
    moveBodyParameters[0] = inData[2];
    moveBodyParameters[1] = inData[3];
    moveBodyParameters[2] = inData[4];
    outData[outDataCounter++] = Orders::orderStart;
  }
  else if (inData[1] == Orders::requestRotateBody)
  {
    blockedOrder = inData[1];
    rotateBodyParameters[0] = inData[2];
    rotateBodyParameters[1] = inData[3];
    outData[outDataCounter++] = Orders::orderStart;
  }
  else if (inData[1] == Orders::requestMoveBodyTo)
  {
    dynamicOrder = inData[1];
    moveBodyParameters[0] = inData[2];
    moveBodyParameters[1] = inData[3];
    moveBodyParameters[2] = inData[4];
    outData[outDataCounter++] = Orders::orderDone;
  }
  else if (inData[1] == Orders::requestRotateBodyTo)
  {
    dynamicOrder = inData[1];
    rotateBodyParameters[0] = inData[2];
    rotateBodyParameters[1] = inData[3];
    outData[outDataCounter++] = Orders::orderDone;
  }
  outData[outDataCounter++] = Orders::transEnd;

  if (blockedOrder != 0)
    dynamicOrder = 0;

  if (orderSource == OrderSource::FromSerial)
    Serial.write(outData, outDataCounter);

}

void Communication::UpdateCommunication()
{
  UpdateStateLED();

}

void Communication::UpdateOrder()
{
  UpdateBlockedOrder();
  UpdateDynamicOrder();
  UpdateAutoSleep();
}

void Communication::UpdateStateLED()
{
  if (ledCounter / ledBlinkCycle < abs(ledState))
  {
    if (ledCounter % ledBlinkCycle == 0)
      SetStateLed(ledState > 0 ? HIGH : LOW);
    else if (ledCounter % ledBlinkCycle == ledBlinkCycle / 2)
      SetStateLed(ledState > 0 ? LOW : HIGH);
  }

  ledCounter++;

  if (ledCounter / ledBlinkCycle >= abs(ledState) + 3)
  {
    ledCounter = 0;

    if (robotAction.robot.state == Robot::State::Action)
      ledState = 1;
    else if (robotAction.robot.state == Robot::State::Boot)
      ledState = 1;
    else if (robotAction.robot.state == Robot::State::Calibrate)
      ledState = 2;
    else if (robotAction.robot.state == Robot::State::Install)
      ledState = 3;

    if (robotAction.robot.power.powerGroupAutoSwitch)
      if (!robotAction.robot.power.powerGroupState)
        ledState = -1;
  }
}

void Communication::UpdateBlockedOrder()
{
  byte blockedOrder = this->blockedOrder;
  if (blockedOrder == 0)
    return;

  lastBlockedOrderTime = millis();

  orderState = OrderState::ExecuteStart;

  if (blockedOrder == Orders::requestCrawlForward)
  {
    robotAction.CrawlForward();
  }
  else if (blockedOrder == Orders::requestCrawlBackward)
  {
    robotAction.CrawlBackward();
  }
  else if (blockedOrder == Orders::requestTurnLeft)
  {
    robotAction.TurnLeft();
  }
  else if (blockedOrder == Orders::requestTurnRight)
  {
    robotAction.TurnRight();
  }
  else if (blockedOrder == Orders::requestActiveMode)
  {
    robotAction.ActiveMode();
  }
  else if (blockedOrder == Orders::requestSleepMode)
  {
    robotAction.SleepMode();
  }
  else if (blockedOrder == Orders::requestSwitchMode)
  {
    robotAction.SwitchMode();
  }
  else if (blockedOrder == Orders::requestInstallState)
  {
    SaveRobotBootState(Robot::State::Install);
    robotAction.robot.InstallState();
  }
  else if (blockedOrder == Orders::requestCalibrateState)
  {
    SaveRobotBootState(Robot::State::Calibrate);
    robotAction.robot.CalibrateState();
  }
  else if (blockedOrder == Orders::requestBootState)
  {
    SaveRobotBootState(Robot::State::Boot);
    robotAction.robot.BootState();
  }
  else if (blockedOrder == Orders::requestCalibrateVerify)
  {
    robotAction.robot.CalibrateVerify();
  }
  else if (blockedOrder == Orders::requestMoveBody)
  {
    SaveRobotBootState(Robot::State::Boot);
    float x = moveBodyParameters[0] - 64;
    float y = moveBodyParameters[1] - 64;
    float z = moveBodyParameters[2] - 64;
    robotAction.MoveBody(x, y, z);
  }
  else if (blockedOrder == Orders::requestRotateBody)
  {
    SaveRobotBootState(Robot::State::Boot);
    float x = rotateBodyParameters[0] - 64;
    float y = rotateBodyParameters[1] - 64;
    float angle = sqrt(pow(x, 2) + pow(y, 2));
    robotAction.RotateBody(x, y, 0, angle);
  }

  this->blockedOrder = 0;
  orderState = OrderState::ExecuteDone;
}

void Communication::UpdateDynamicOrder()
{
  byte dynamicOrder = this->dynamicOrder;
  if (dynamicOrder == 0)
    return;

  lastBlockedOrderTime = 0;

  if (dynamicOrder == Orders::requestMoveBodyTo)
  {
    SaveRobotBootState(Robot::State::Boot);
    float x = moveBodyParameters[0] - 64;
    float y = moveBodyParameters[1] - 64;
    float z = moveBodyParameters[2] - 64;
    robotAction.MoveBody(x, y, z);
  }
  else if (dynamicOrder == Orders::requestRotateBodyTo)
  {
    SaveRobotBootState(Robot::State::Boot);
    float x = rotateBodyParameters[0] - 64;
    float y = rotateBodyParameters[1] - 64;
    float angle = sqrt(pow(x, 2) + pow(y, 2));
    robotAction.RotateBody(x, y, 0, angle);
  }
}

void Communication::UpdateAutoSleep()
{
  if (lastBlockedOrderTime != 0)
  {
    if (millis() - lastBlockedOrderTime > autoSleepOvertime)
    {
      if (robotAction.robot.state == Robot::State::Action)
        robotAction.SleepMode();
      lastBlockedOrderTime = 0;
    }
  }
}

void Communication::CheckBlockedOrder()
{
  if (orderState != OrderState::ExecuteDone)
    return;

  byte outData[outDataSize];
  byte outDataCounter = 0;

  outData[outDataCounter++] = Orders::transStart;
  outData[outDataCounter++] = Orders::orderDone;
  outData[outDataCounter++] = Orders::transEnd;

  if (orderSource == OrderSource::FromSerial)
    Serial.write(outData, outDataCounter);

  orderState = OrderState::ExecuteNone;
}

void Communication::SaveRobotBootState(Robot::State state)
{
  byte stateByte = 0;

  if (state == Robot::State::Install)
    stateByte = 0;
  else if (state == Robot::State::Calibrate)
    stateByte = 1;
  else if (state == Robot::State::Boot)
    stateByte = 2;

  if (EEPROM.read(EepromAddresses::robotState) != stateByte)
    EEPROM.write(EepromAddresses::robotState, stateByte);
}

Robot::State Communication::GetRobotBootState()
{
  byte stateByte = EEPROM.read(EepromAddresses::robotState);
  Robot::State state;

  if (stateByte == 0)
    state = Robot::State::Install;
  else if (stateByte == 1)
    state = Robot::State::Calibrate;
  else if (stateByte == 2)
    state = Robot::State::Boot;

  return state;
}

void Communication::SetRobotBootState(Robot::State state)
{
  if (state == Robot::State::Install)
    robotAction.robot.InstallState();
  else if (state == Robot::State::Calibrate)
    robotAction.robot.CalibrateState();
  else if (state == Robot::State::Boot)
    robotAction.robot.BootState();
}

void UpdateService()
{
  sei();

  if (communication != NULL) {
    communication->robotAction.robot.Update();
    communication->UpdateCommunication();
  }
}

#endif
