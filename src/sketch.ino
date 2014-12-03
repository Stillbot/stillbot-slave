/*
 * This file is basically the Arduino code from
 * github.com/thearn/Python-Arduino-Command-API plus the
 * OneWire/DallasTemperature features needed to take readings.
 */

#include <SoftwareSerial.h>
#include <Wire.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>

#define VERSION_STRING "StillBot-slave-0.1"

#define DEFAULT_BAUDRATE 115200
#define SSR_PIN 12
#define DUTY_CYCLE_SIZE 1000
#define DEFAULT_SETPOINT 35.0
//#define DEFAULT_SETPOINT 78.37
#define DEFAULT_PID_THERMISTOR 0
#define ONE_WIRE_BUS_PIN 10


OneWire     oneWire(ONE_WIRE_BUS_PIN);

DallasTemperature dt(&oneWire);

double      SetPoint = DEFAULT_SETPOINT,
            Input,
            Output;

/* Aggressive PID settings (< 10C from setpoint) */
double      Kp = 44,
            Ki=164,
            Kd=4;

/* Conservative PID settings */
double      cons_Kp=1,
            cons_Ki=0.05,
            cons_Kd=0.25;

PID         pid(&Input, &Output, &SetPoint, Kp, Ki, Kd, DIRECT);
unsigned int pid_thermistor_index = DEFAULT_PID_THERMISTOR;
boolean     boiler_on = false;
boolean     pid_on =false;
unsigned long windowStartTime;


int Str2int (String Str_value)
{
  char buffer[10]; //max length is three units
  Str_value.toCharArray(buffer, 10);
  int int_value = atoi(buffer);
  return int_value;
}

float Str2float(String Str_value)
{
  char floatbuf[32]; // make this at least big enough for the whole string
  Str_value.toCharArray(floatbuf, sizeof(floatbuf));
  float f = atof(floatbuf);
}

void split(String results[], int len, String input, char spChar) {
  String temp = input;
  for (int i=0; i<len; i++) {
    int idx = temp.indexOf(spChar);
    results[i] = temp.substring(0,idx);
    temp = temp.substring(idx+1);
  }
}

void Version(){
  Serial.println(VERSION_STRING);
}

float thermistorTemp(unsigned int thermistorIndex) {
    dt.requestTemperaturesByIndex(thermistorIndex);
    return dt.getTempCByIndex(thermistorIndex);
}

void Tone(String data){
  int idx = data.indexOf('%');
  int len = Str2int(data.substring(0,idx));
  String data2 = data.substring(idx+1);
  int idx2 = data2.indexOf('%');
  int pin = Str2int(data2.substring(0,idx2));
  String data3 = data2.substring(idx2+1);
  String melody[len*2];
  split(melody,len*2,data3,'%');

  for (int thisNote = 0; thisNote < len; thisNote++) {
    int noteDuration = 1000/Str2int(melody[thisNote+len]);
    int note = Str2int(melody[thisNote]);
    tone(pin, note, noteDuration);
    int pause = noteDuration * 1.30;
    delay(pause);
    noTone(pin);
  }
} 

void ToneNo(String data){
  int pin = Str2int(data);
  noTone(pin);
} 

void SerialParser(void) {
  char readChar[64];

  /*
     Strangely enough, this piece of code was never needed until
     I started communicating to the Arduino via Debian/3.2.0-4-amd64.

     I suspected the culprit was the FTDI driver but was never able to
     get enough visibility into the workings of it. Instead I cleared this
     buffer and all then seemed well *shrugs*.
   */
  for (int i = 0; i < 64; i++) {
    readChar[i] = 0;
  }
  
  Serial.readBytesUntil(33,readChar,64);
  String read_ = String(readChar);
  //Serial.println(readChar);
  int idx1 = read_.indexOf('%');
  int idx2 = read_.indexOf('$');
  // separate command from associated data
  String cmd = read_.substring(1,idx1);
  String data = read_.substring(idx1+1,idx2);
  
  // determine command sent
  if (cmd == "version") {
      Version();   
  }
  else if (cmd == "dscount") {
      Serial.println(dt.getDeviceCount());
  }
  else if (cmd == "dstemp") {
      Serial.println(thermistorTemp(Str2int(data)));
  }
  else if (cmd == "boiler") {
      if (data == "on") {
        digitalWrite(SSR_PIN, HIGH);
        boiler_on = true;
      } else if (data == "off") {
        digitalWrite(SSR_PIN, LOW);
        boiler_on = false;
      }
      if (boiler_on == true) {
          Serial.println("on");
      } else {
          Serial.println("off");
      }
  }
  else if (cmd == "pid") {
  
    if (data == "on") {
      if (dt.getDeviceCount() == 0) {
        Serial.println("NO_THERMISTORS_PRESENT");
        return;
      }
      pid_on = true;
    } else if (data == "off") {
      pid_on = false;
    }
    if (pid_on == true) {
        Serial.println("on");
    } else {
        Serial.println("off");
    }
  }
  else if (cmd == "sp") {
    if (data == "") {
      Serial.println(SetPoint);
    } else {
      SetPoint = Str2int(data);
      Serial.println(SetPoint);
    }
  }
  else if (cmd == "kp") {
    if (data == "") {
      Serial.println(pid.GetKp());
    } else {
      pid.SetTunings(Str2float(data), pid.GetKi(), pid.GetKd());
      Serial.println(pid.GetKp());
    }
  }
  else if (cmd == "ki") {
    if (data == "") {
      Serial.println(pid.GetKi());
    } else {
      pid.SetTunings(pid.GetKp(), Str2float(data), pid.GetKd());
      Serial.println(pid.GetKi());
    }
  }
  else if (cmd == "kd") {
    if (data == "") {
      Serial.println(pid.GetKd());
    } else {
      pid.SetTunings(pid.GetKp(), pid.GetKi(), Str2float(data));
      Serial.println(pid.GetKd());
    }
  }
  else if (cmd == "output") {
    Serial.println(Output);
  }
  
}

void setup()  {
  Serial.begin(DEFAULT_BAUDRATE); 
    while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  
  oneWire.reset();
  dt.begin();
  if (dt.getDeviceCount() >= DEFAULT_PID_THERMISTOR) {
    pid_thermistor_index = DEFAULT_PID_THERMISTOR;
  }
  
  pinMode(SSR_PIN, OUTPUT); 
  
  pid.SetTunings(Kp, Ki, Kd);
  pid.SetOutputLimits(0, DUTY_CYCLE_SIZE);          // PID output range: 0 - DUTY_CYCLE_SIZE
  pid.SetMode(AUTOMATIC);
  
  windowStartTime = millis();
}

void loop() {
   unsigned long now = millis();

   Input = thermistorTemp(pid_thermistor_index);
   double gap = abs(SetPoint - Input);              // distance away from setpoint
   
   pid.Compute();

   if (now - windowStartTime > DUTY_CYCLE_SIZE) {
     windowStartTime += DUTY_CYCLE_SIZE;
   }
   
   if (pid_on) {
     if (Output > now - windowStartTime) {
       digitalWrite(SSR_PIN, HIGH);
       boiler_on = true;
     } else {
       digitalWrite(SSR_PIN, LOW);
       boiler_on = false;
     }
   }

  SerialParser();
}
