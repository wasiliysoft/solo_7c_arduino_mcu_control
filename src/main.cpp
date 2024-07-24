#include <Arduino.h>
#include "GyverTimer.h"
#include "EncButton.h"
#include "IRremote.h"
#include <Wire.h>
#include <avr/pgmspace.h>

#define KEY_UNDEFINED 0
#define KEY_REPEAT    1
#define KEY_MUTE      2
#define KEY_VOL_UP    3
#define KEY_VOL_DOWN  4
#define KEY_BASS_UP   5
#define KEY_BASS_DOWN 6
#define KEY_TREB_UP   7
#define KEY_TREB_DOWN 8
#define KEY_INPUT_CH  9
#define KEY_RESET     10


#define P_REPEAT_CODE 4294967295

#define P_VOL_UP 16486511
#define P_VOL_DOWN 16490591

#define P_IN_SEL 16476311
#define P_MUTE 16460501

#define P_BASS_UP 16494671
#define P_BASS_DOWN 16462541

#define P_TREB_UP 16484471
#define P_TREB_DOWN 16452341

#define DISP_DELAY 1

//
//                            --A--
//                         F |     | B
//                           |--G--|
//                         E |     | C
//                            --D--
//                   {A, B, C, D, E,   F, G};
int segmentPins[7] = {6, 8, 9, 11, 10, 5, 7};
#define gnd1 3
#define gnd2 4

#define MAX_VOLUME 60
#define ADR 0x41 // адрес MCU I2C

enum INPUTS { AUX, PC};

IRrecv IrReceiver(2); // вывод, к которому подключен приемник
EncButton eb(A3, A2, A1); // pin энкодера



GTimer timeOutToDisplayVolume;


typedef struct {
  bool isMute = true;
  INPUTS inputCh = AUX;
  int bass = 0;
  int treble = 0;
  int volume = 20;
} stateStruct;

typedef struct {
  //          GFEDCBA
  byte d1 = B01000000;
  byte d2 = B01000000;
} dispStruct;

stateStruct avrState;
dispStruct dispState;

void displayTick();
void displaySetInt(int i);
void setInputAndDisplayAUX();
void setInputAndDisplayPC();
void processCode(unsigned long irCode);
void processOneEventKey(unsigned long irCode);
void syncMCU();
stateStruct setMCUState(stateStruct newState);
void switchMute();
void switchInputCh();
void irReceiveTick();

void setup() {
  timeOutToDisplayVolume.setTimeout(2000);
  for (uint8_t i = 0; i < 7; i++) {
    pinMode(segmentPins[i], OUTPUT);
  }
  pinMode(gnd1, OUTPUT);
  pinMode(gnd2, OUTPUT);

  Wire.begin();
  IrReceiver.enableIRIn();
  eb.setEncType(EB_STEP4_LOW);

  Serial.begin(9600);
}

void loop() {
  eb.tick();

  if (eb.click()) processCode(P_MUTE);
  if (eb.turn()){
    if(eb.dir()>0) processCode(P_VOL_UP);
    if(eb.dir()<0) processCode(P_VOL_DOWN);
  }

  irReceiveTick();

  syncMCU();  

  if (timeOutToDisplayVolume.isReady() && avrState.isMute == false) {
    displaySetInt(avrState.volume);
  }

  displayTick();
}

void irReceiveTick(){
  static decode_results results;
  static unsigned long nextReadyTime = 0;
  if (IrReceiver.decode(&results)) { // если данные пришли
    if (nextReadyTime < millis()) {
      processCode(results.value);
      nextReadyTime =  millis() + 200;
    }else if (results.value != P_REPEAT_CODE){
      processCode(results.value);
    }
    IrReceiver.resume(); // сброс буфера
  }
}

void displayTick() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (dispState.d1 >> i) & 1u);
  }
  digitalWrite(gnd1, HIGH);
  digitalWrite(gnd2, LOW); 
  delay(DISP_DELAY);
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, LOW);
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (dispState.d2 >> i) & 1u);
  }
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, HIGH);
  delay(DISP_DELAY);
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, LOW);
}

void setInputAndDisplayAUX() {
  avrState.inputCh = AUX;
  dispState.d1 = B01110111;
  dispState.d2 = B00111110;
}

void setInputAndDisplayPC() {
  avrState.inputCh = PC;
  dispState.d1 = B01110011;
  dispState.d2 = B00111001;
}

void switchMute() {
  avrState.isMute = !avrState.isMute;
  if (avrState.isMute) {
    dispState.d1 = B01000000;
    dispState.d2 = B01000000;
  } else {
    displaySetInt(avrState.volume);
  }
}

void switchInputCh() {
  switch (avrState.inputCh) {
  case AUX:
    setInputAndDisplayPC();
    break;
  case PC:
    setInputAndDisplayAUX();
    break;
  }
}

void displaySetInt(int i) {
  static const byte nums[10] = {B00111111, B00000110, B01011011, B01001111,
                                B01100110, B01101101, B01111101, B00000111,
                                B01111111, B01100111};
  i = constrain(i, -9, 99);
  if (i < 0) {
    dispState.d1 = B01000000; // минус
    dispState.d2 = nums[abs(i)];
  } else if (i < 10) {
    dispState.d1 = B00000000; // погашен
    dispState.d2 = nums[abs(i)];
  } else {
    dispState.d1 = nums[abs(i) / 10];
    dispState.d2 = nums[abs(i) % 10];
  }
}


void processCode(unsigned long irCode) {
  
  if (avrState.isMute && irCode != P_MUTE) return;
  
  static unsigned long lastIrKey = 0;

  if(irCode != P_REPEAT_CODE){
    lastIrKey = irCode;
    // обработка команд которые НЕ ДОЛЖНЫ поддерживать удержание
    switch (lastIrKey) {
      case P_IN_SEL:
        switchInputCh();
        break;
      case P_MUTE:
        switchMute();
        break;
      }
  }
   
  switch (lastIrKey) {
    case P_VOL_UP:
      avrState.volume++;
      avrState.volume = constrain(avrState.volume, 0, MAX_VOLUME);
      displaySetInt(avrState.volume);
      break;
    case P_VOL_DOWN:
      avrState.volume--;
      avrState.volume = constrain(avrState.volume, 0, MAX_VOLUME);
      displaySetInt(avrState.volume);
      break;
    case P_BASS_UP:
      avrState.bass++;
      avrState.bass = constrain(avrState.bass, -7, 7);
      displaySetInt(avrState.bass);
      break;
    case P_BASS_DOWN:
      avrState.bass--;
      avrState.bass = constrain(avrState.bass, -7, 7);
      displaySetInt(avrState.bass);
      break;
    case P_TREB_UP:
      avrState.treble++;
      avrState.treble = constrain(avrState.treble, -7, 7);
      displaySetInt(avrState.treble);
      break;
    case P_TREB_DOWN:
      avrState.treble--;
      avrState.treble = constrain(avrState.treble, -7, 7);
      displaySetInt(avrState.treble);
      break;
    case P_REPEAT_CODE:
      break;
    default:
      Serial.print("code ");
      Serial.println(lastIrKey);
      break;
  }
}

 void syncMCU(){
  static stateStruct mcuState = avrState;
  if (mcuState.volume != avrState.volume) goto needUpdate;
  if (mcuState.bass != avrState.bass) goto needUpdate;
  if (mcuState.treble != avrState.treble) goto needUpdate;
  if (mcuState.inputCh != avrState.inputCh) goto needUpdate;
  if (mcuState.isMute != avrState.isMute) goto needUpdate;  
  return;
  needUpdate:
    mcuState = setMCUState(avrState);
    timeOutToDisplayVolume.start();
 }

stateStruct setMCUState(stateStruct avrState) {
  Wire.beginTransmission(ADR);
  Wire.write(0); // write mode

  // VOLUME
  if (avrState.volume <= 0) {
    Wire.write(255); // Data L
    Wire.write(255); // Data R
  } else {
    int vol = 96 - ((float) avrState.volume * 1.6f);
    Wire.write(vol); // Data L
    Wire.write(vol); // Data R
  }

  // INPUT
  // todo if mainState.volume = 0 to MUTE
  if (avrState.isMute) {
    Wire.write(B11100000);
  } else {
    switch (avrState.inputCh) {
    case AUX:
      Wire.write(B00000000);
      break;
    case PC:
      Wire.write(B00100000);
      break;
    }
  }
  // Stereo    00 | Bypass              00  |   0000
  // Lch Mono  01 | Tone                01  |   0000
  // Rch Mono  10 | Tone & Surround Hi  10  |   0000
  //              | Tone & Surround Low 11  |   0000
  Wire.write(B00010000);

  // EQ
  byte bass = B00000000;
  if (avrState.bass > 0) {
    bass = B10000000;
  }
  bass = bass | (abs(avrState.bass) << 4);

  byte treble = B00000000;
  if (avrState.treble > 0) {
    treble = B00001000;
  }
  treble = treble | abs(avrState.treble);

  Wire.write(bass | treble);

  Wire.endTransmission();
  return avrState;
}