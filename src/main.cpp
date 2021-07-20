#include "GyverTimer.h"
#include "IRremote.h"
#include <Arduino.h>
#include <Wire.h>
#include <avr/pgmspace.h>

#define P_REPEAT_CODE 4294967295

#define P_VOL_UP 83574975
#define P_VOL_DOWN 83595375

#define P_IN_SEL 83622405
#define P_IN_AU 83573445
#define P_IN_PC 83606085
#define P_IN_BT 83589765
#define P_MUTE 83610165

#define P_BASS_UP 83616285
#define P_BASS_DOWN 83604045

#define P_TREB_UP 83612205
#define P_TREB_DOWN 83595885

#define DISP_DELAY 1

//
//                            --A--
//                         F |     | B
//                           |--G--|
//                         E |     | C
//                            --D--
// int sigmentPins[7] = {A, B, C, D, E, F, G};
int segmentPins[7] = {6, 8, 9, 11, 10, 5, 7};
#define gnd1 3
#define gnd2 4

#define MAX_VOLUME 60
#define ADR 0x41 // адрес MCU I2C

// uint8_t dB[] = {120, 92, 76, 64, 56, 48, 42, 37, 32, 28,
//                 24,  20, 17, 14, 12, 9,  7,  4,  2,  0};
enum INPUTS { AUX, PC, BLUETOOTH };

IRrecv IrReceiver(2); // вывод, к которому подключен приемник
decode_results results;
unsigned long irCode = 0;

GTimer timeOutToProcessNextIrCode;
GTimer timeOutToDisplayVolume;

struct {
  bool updateFlag = true;
  bool isMute = true;
  INPUTS inputCh = AUX;
  int bass = 0;
  int treble = 0;
  int volume = 20;
} main;

struct {
  //          GFEDCBA
  byte d1 = B01000000;
  byte d2 = B01000000;
} disp;

void displayTick();
void displaySetInt(int i);
void setInputAndDisplayAUX();
void setInpeutAndDisplayBluetooth();
void setInputAndDisplayPC();
void processCode(unsigned long irCode);
void updateMCUState();
void switchMute();
void switchInputCh();

void setup() {
  timeOutToDisplayVolume.setTimeout(2000);
  timeOutToProcessNextIrCode.setTimeout(200);
  for (uint8_t i = 0; i < 7; i++) {
    pinMode(segmentPins[i], OUTPUT);
  }
  pinMode(gnd1, OUTPUT);
  pinMode(gnd2, OUTPUT);

  Wire.begin();
  IrReceiver.enableIRIn();

  Serial.begin(9600);
}

void loop() {
  if (IrReceiver.decode(&results)) { // если данные пришли
    if (results.value != P_REPEAT_CODE) {
      irCode = results.value;
    }
    if (timeOutToProcessNextIrCode.isReady()) {
      processCode(irCode);
      timeOutToProcessNextIrCode.start();
      timeOutToDisplayVolume.start();
      main.updateFlag = true;
    }
    IrReceiver.resume(); // сброс буфера
  }

  if (timeOutToDisplayVolume.isReady() && main.isMute == false) {
    displaySetInt(main.volume);
  }

  if (main.updateFlag) {
    updateMCUState();
    main.updateFlag = false;
  }

  displayTick();
}

void displayTick() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (disp.d1 >> i) & 1u);
  }
  digitalWrite(gnd1, HIGH);
  digitalWrite(gnd2, LOW);
  delay(DISP_DELAY);
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, LOW);
  for (int i = 0; i < 7; i++) {
    digitalWrite(segmentPins[i], (disp.d2 >> i) & 1u);
  }
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, HIGH);
  delay(DISP_DELAY);
  digitalWrite(gnd1, LOW);
  digitalWrite(gnd2, LOW);
}

void setInputAndDisplayAUX() {
  main.inputCh = AUX;
  disp.d1 = B01110111;
  disp.d2 = B00111110;
}

void setInputAndDisplayPC() {
  main.inputCh = PC;
  disp.d1 = B01110011;
  disp.d2 = B00111001;
}

void setInpeutAndDisplayBluetooth() {
  main.inputCh = BLUETOOTH;
  disp.d1 = B01111111;
  disp.d2 = B00111000;
}

void switchMute() {
  main.isMute = !main.isMute;
  if (main.isMute) {
    disp.d1 = B01000000;
    disp.d2 = B01000000;
  } else {
    displaySetInt(main.volume);
  }
}

void switchInputCh() {
  switch (main.inputCh) {
  case AUX:
    setInputAndDisplayPC();
    break;
  case PC:
    setInpeutAndDisplayBluetooth();
    break;
  case BLUETOOTH:
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
    disp.d1 = B01000000; // минус
    disp.d2 = nums[abs(i)];
  } else if (i < 10) {
    disp.d1 = B00000000; // погашен
    disp.d2 = nums[abs(i)];
  } else {
    disp.d1 = nums[abs(i) / 10];
    disp.d2 = nums[abs(i) % 10];
  }
}

void processCode(unsigned long irCode) {
  if (main.isMute && results.value != P_MUTE && results.value != 2155807485) {
    return;
  }

  // обработка команд которые НЕ ДОЛЖНЫ поддерживать удержание
  switch (results.value) {
  case P_IN_SEL:
    switchInputCh();
    return;
  case 2155807485:
  case P_MUTE:
    switchMute();
    return;
  }

  // обработка команд которые должны поддерживать удержание
  switch (irCode) {
  case 2155836045:
  case P_IN_AU:
    setInputAndDisplayAUX();
    break;
  case 2155851855:
  case P_IN_PC:
    setInputAndDisplayPC();
    break;
    // case P_IN_BT:
    //   setInpeutAndDisplayBluetooth();
    //   break;
  case 2155813095:
  case P_VOL_UP:
    main.volume++;
    main.volume = constrain(main.volume, 0, MAX_VOLUME);
    displaySetInt(main.volume);
    break;
  case 2155809015:
  case P_VOL_DOWN:
    main.volume--;
    main.volume = constrain(main.volume, 0, MAX_VOLUME);
    displaySetInt(main.volume);
    break;
  case 2155815135:
  case P_BASS_UP:
    main.bass++;
    main.bass = constrain(main.bass, -7, 7);
    displaySetInt(main.bass);
    break;
  case 2155831965:
  case P_BASS_DOWN:
    main.bass--;
    main.bass = constrain(main.bass, -7, 7);
    displaySetInt(main.bass);
    break;
  case 2155811055:
  case P_TREB_UP:
    main.treble++;
    main.treble = constrain(main.treble, -7, 7);
    displaySetInt(main.treble);
    break;
  case 2155827885:
  case P_TREB_DOWN:
    main.treble--;
    main.treble = constrain(main.treble, -7, 7);
    displaySetInt(main.treble);
    break;

  default:
    Serial.print("code ");
    Serial.println(results.value);
  }
}

void updateMCUState() {
  Wire.beginTransmission(ADR);
  Wire.write(0); // write mode

  // VOLUME
  if (main.volume <= 0) {
    Wire.write(255); // Data L
    Wire.write(255); // Data R
  } else {
    int vol = 96 - ((float) main.volume * 1.6f);
    Wire.write(vol); // Data L
    Wire.write(vol); // Data R
  }

  // INPUT
  // todo if main.volume = 0 to MUTE
  if (main.isMute) {
    Wire.write(B11100000);
  } else {
    switch (main.inputCh) {
    case AUX:
      Wire.write(B00000000);
      break;
    case PC:
      Wire.write(B00100000);
      break;
    case BLUETOOTH:
      Wire.write(B01000000);
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
  if (main.bass > 0) {
    bass = B10000000;
  }
  bass = bass | (abs(main.bass) << 4);

  byte treble = B00000000;
  if (main.treble > 0) {
    treble = B00001000;
  }
  treble = treble | abs(main.treble);

  Wire.write(bass | treble);

  Wire.endTransmission();
}