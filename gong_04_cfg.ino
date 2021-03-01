/*
    gong-04.cfg - Achtsamkeit-Glocke

    Jens, 2021-02-28
    MIT License
    
    sound files based on samples from
    freesound https://freesound.org/
    with appropriate licenses

    board: ESP32 pico kit
    note:
    'partition scheme: no OTA' --> 1.3 Mbyte wav files in PROGMEM

         +---------------+
         |o    ANT      o|
         |o ___________ o|
         |o             o|
         |o             o|
         |o          D22o|- I2S
    I2S -|oD25          o|
    I2S -|oD26          o|
         |o          D18o|- Vol
    key -|oD33          o| 
         |o          D10o|- Snd
         |o           D9o|- Snd  
         |o             o|
         |o             o|
         |o             o|
         |o             o|
         |o             o|
  -4k7R--|oIO0          o|       
  |------|o3V3        ENo|
         |oGND       GNDo|
         |o5V  |USB| 3V3o|
         +-----+---+-----+

  pull-up IO0 for power-up reset       
    
*/

//#include <WiFi.h>

#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

#include "bell.h"
#include "bowl.h"
#include "clock.h"
#include "dingdong.h"
#include "feedback.h"
#include "gong.h"
#include "horn.h"
#include "laser.h"
#include "over.h"
#include "power.h"
#include "tin.h"


// manual trigger
#define TRIGGER 33

// config pins
#define SNDCFG1 9
#define SNDCFG2 10
#define SNDVOL  18

// non-blocking timer
unsigned long previousMillis = 0;
const unsigned long interval = 60000;               //  1 min
const int wait = 60;                                // 60 min
int count = 0;


AudioGeneratorWAV *wav;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;


const char helpstr[] = "\
\nAchtsamkeits-Glocke\n\
BCGH   - sounds used\n\
bflpto - other sounds\n\
0      - sound off\n\
1..9   - volume\n\
T      - show timer\n\
s      - speed up timer\n\
!      - Reset\n\
?      - help\n";


void setup() {
  //  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);
  delay(1000);

  Serial.println(helpstr);

  // input pins
  pinMode(TRIGGER, INPUT_PULLUP);
  pinMode(SNDCFG1, INPUT_PULLUP);
  pinMode(SNDCFG2, INPUT_PULLUP);
  pinMode(SNDVOL, INPUT_PULLUP);

  audioLogger = &Serial;
  file = new AudioFileSourcePROGMEM( tin_wav, sizeof(tin_wav) );
  //  out = new AudioOutputI2S(0, 1);               // use ESP32-internal DAC channel 1 (pin25)
  out = new AudioOutputI2S();

  if (digitalRead(SNDVOL))                          // pin D18
    out -> SetGain(0.1);                            // volume quote low
  else
    out -> SetGain(0.05);                           // volume very low
  out -> SetPinout(26, 25, 22);

  count = wait;
  Serial.print("WAV starts...");
  wav = new AudioGeneratorWAV();
  wav->begin(file, out);
}


void loop() {

  if (wav->isRunning())
    if (!wav->loop()) {
      wav->stop();
      Serial.println("done.");
      delete file;
      delete wav;
    }

  char ch;
  char fb = ' ';                                    // neutral feedback, no sound

  if (Serial.available() > 0) {
    ch = Serial.read();                             // some serial input, using first character
    while (Serial.available())
      Serial.read();
    Serial.print("--> serial input: ");
    Serial.println(ch);

    if (ch < ' ')                                   // drop control characters
      ;
    else if ((ch >= '0') && (ch <= '9')) {          // Volume: '0' - '9'
      float vol = (float)(ch - '0') / 25;
      out -> SetGain(vol);
      Serial.print("vol: ");
      Serial.println(vol);
      fb = '+';
    } else if (ch == 's') {                         // speed up timer / shortcut
      count = 1;
      previousMillis = millis();
      Serial.println("shortcut");
      fb = '+';
    } else if (ch == '?') {                         // help
      Serial.println(helpstr);
    } else if (ch == 'T') {
      Serial.print("timer: ");                      // show time left
      Serial.println(count);
    } else if (ch == '!') {
      Serial.println("reset...");                   // reset ESP32
      Serial.println(".");
      Serial.println(".");
      Serial.println(".");
      ESP.restart();
    } else {
      fb = '#';                                     // play selected sound
      if (ch == 'b')
        file = new AudioFileSourcePROGMEM( bell_wav, sizeof(bell_wav) );
      else if (ch == 'B')
        file = new AudioFileSourcePROGMEM( bowl_wav, sizeof(bowl_wav) );
      else if (ch == 'C')
        file = new AudioFileSourcePROGMEM( clock_wav, sizeof(clock_wav) );
      else if (ch == 'd')
        file = new AudioFileSourcePROGMEM( dingdong_wav, sizeof(dingdong_wav) );
      else if (ch == 'f')
        file = new AudioFileSourcePROGMEM( feedback_wav, sizeof(feedback_wav) );
      else if (ch == 'G')
        file = new AudioFileSourcePROGMEM( gong_wav, sizeof(gong_wav) );
      else if (ch == 'H')
        file = new AudioFileSourcePROGMEM( horn_wav, sizeof(horn_wav) );
      else if (ch == 'l')
        file = new AudioFileSourcePROGMEM( laser_wav, sizeof(laser_wav) );
      else if (ch == 'p')
        file = new AudioFileSourcePROGMEM( power_wav, sizeof(power_wav) );
      else if (ch == 't')
        file = new AudioFileSourcePROGMEM( tin_wav, sizeof(tin_wav) );
      else
        fb = '-';                                   // ... no sound, negative feedback
    }
    if (fb != ' ') {
      if (fb == '+')
        file = new AudioFileSourcePROGMEM( feedback_wav, sizeof(feedback_wav) );
      if (fb == '-')
        file = new AudioFileSourcePROGMEM( over_wav, sizeof(over_wav) );
      wav = new AudioGeneratorWAV();
      wav->begin(file, out);
    }
  } // serial input

  if (digitalRead(TRIGGER) == 0) {
    delay(100);
    count = wait;                                       // restart timer
    file = new AudioFileSourcePROGMEM( tin_wav, sizeof(tin_wav) );
    wav = new AudioGeneratorWAV();
    Serial.println("--> manual trigger");
    wav->begin(file, out);
    delay(500);                                         // block and debounce
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    count--;
    if (count > 0) {
      Serial.print("waiting: ");
      Serial.println(count);                            // show timer status
    } else {
      Serial.println("--> timing trigger");
      count = wait;                                     // restart timer

      if (digitalRead(SNDCFG1))                         // pin d9
        if (digitalRead(SNDCFG2))                       // pin 10
          file = new AudioFileSourcePROGMEM( bowl_wav, sizeof(bowl_wav));
        else
          file = new AudioFileSourcePROGMEM( clock_wav, sizeof(clock_wav));
      else if (digitalRead(SNDCFG2))
        file = new AudioFileSourcePROGMEM( gong_wav, sizeof(gong_wav));
      else
        file = new AudioFileSourcePROGMEM( horn_wav, sizeof(horn_wav));

      wav = new AudioGeneratorWAV();
      Serial.print("WAV starts...\n");
      wav->begin(file, out);
    }
  }
}
