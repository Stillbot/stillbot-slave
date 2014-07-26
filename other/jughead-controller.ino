// ---------------------------------------------------------------------------
//  minijughead controller
//  
//  crude console interpreter for testing the Jughead Arduino Shield
//  
//  Creative Commons Attribution-Share Alike 2.5 2009 by Snuffy
//
//  Version 8/11/09
//
// ---------------------------------------------------------------------------


#define LOWRANGE  470         // cut off for low linear fit
#define MIDRANGE 550          // cutoff for middle linear fit

#define SSR 8                 // Solid State Relay is pin 8
#define VALVE1 9              // Valve #1 is pin 9
#define VALVE2 10             // Valve #2 is pin 10
#define VALVE3 11             // Valve #3 is pin 11

#define HEATER 4500.0         // immersion element is 4500W
#define CYCLETIME 1000        // SSR is on one second duty cycle
#define TEMPTIME 5000         // Temperature is displayed every 5 seconds

int MaxAnalog = 5;            // use analog ports 0 - 5;
long TimeOn = 0;              // milliseconds on time
long TimeOff = 0;             // milliseconds off time
long PowerTimeOut = 0;        // next time to cycle SSR power

bool PowerOn = false;         // state of SSR output

float PowerLevel = 0.0;       // desired power level in Watts
int Watts = 0;                // actual power level (resolution based on CYCLETIME)

float reading[6];             // readings[i] is sum (or average) of analog  
float readCount = 0.0;        // readings taken this period
float temp[6];                // last temp in degrees C
long TempPeriod = 0;          // milliseconds to update temperature
long TempTimeOut = 0;         // end of reading period

char command;                 // single character commands
float num = -255.0;           // single numeric parameter initialized as null


// -----------------------------------------------------------------
// ------- linear fit parameters derived from calibration ----------
// -----------------------------------------------------------------

float mXlow[6] = {
  0.1381, 0.1367, 0.1392, 0.1334, 0.1365, 0.1387}; // slope
float bXlow[6] = {
  9.39, 9.13, 9.21, 8.75, 8.95, 9.20};             // Y intercept

float mXmid[6] = {
  0.1170, 0.1166, 0.1185, 0.1173, 0.1181, 0.1180}; // slope
float bXmid[6] = {
  17.21, 16.56, 16.73, 14.75, 15.69, 16.71};       // Y intercept 

float mXhi[6] = {
  0.1330, 0.1324, 0.1334, 0.1344, 0.1355, 0.1388}; // slope
float bXhi[6] = {
  9.03, 8.24, 9.08, 6.67, 6.37, 8.42};             // Y intercept 

// -----------------------------------------------------------------
//     error factors for linear interpolation of corrections
//     all values multiplied by 100 for fixed point math
//     the numbers in the error table names are derived from
//     the values at which the errors were calculated
// -----------------------------------------------------------------

long err306[6] = {
  -204, -198,-216, -198, -194, -195};
long err608[6] = {
  71, 44, 53, 81, 72, 69};
long err750[6] = {
  94, 63, 88, 108, 91, 94};
long err803[6] = {
  104, 51, 85, 110, 89, 87};
long err900[6] = {
  24, 6, 35, -67, 37, -255};
long err956[6] = {
  41, -2, 44, -30, 51, -284};
long err100[6] = {
  6, 29, 7, -115, 9, -317};

bool contflag = true;       // global flag to break out of loops


void setup(){
  Serial.begin(19200);      // start serial baud rate
  Reset();
  InitReading();
  SetPower(0);
}

void loop() {
  ReadIn();

  Execute();
  ControlPower();
  GetReading();
}

void Execute() {
  if (command > '\0') {
    switch (command) {
    case 'M':
      if (num >= 0.0) {
        MaxAnalog = num;
        Serial.print("MaxAnalog: ");
        Serial.print(MaxAnalog);
      }
      break;

    case 'P':
      if (num >= 0.0) {
        Serial.print("Set power: "); 
        Serial.println(num);
        SetPower(num);         // set the power to num 
      }
      else {
        Serial.println("Set power - no number!");
      }
      break;

    case 'T':
      if (num > 1000.0) {
        Serial.print("Temp period: "); 
        Serial.println(num);
        TempPeriod = num;      // set the temp display delay to num
      }
      else {
        Serial.println("Temp period - invalid number!");
      }
      break;

    case 'S':
      ShowSys();
      break;

    case 'R':
      DisplayReadings();  // display the raw analog accumulators
      break;

    case 'X':
      Reset();
      Serial.println("System Reset");
      ShowSys();
      break;

    default:
      Serial.println("unrecognized command");
      Serial.println("Commands: <M>axAnalog <P>ower <T>emp delay <S>how sys <R>eadings <X> Reset");
      Serial.println();
      break;
    }
    command = '\0';
    num = -255.0;
  }  // end if
}



// -----------------------------------------------------------------
//   ReadIn parses input stream
//     spaces are separators
//     numbers are pushed onto the stack
//     alpha commands passed to interpreter for execution
//     and the result(s) are pushed onto the stack
// -----------------------------------------------------------------

void ReadIn() {
  char c;
  int i;

  if (Serial.available() > 0) {
    delay(100);
    while (Serial.available() > 0) {
      c = Serial.read();
      if (c == 32) {
        // skip leading and trailing spaces
      }
      else if (c == '-') {
        i = -1;
        num = ReadNum(i);
      }
      else if (c >= '0' && c <= '9') {      // numeric value
        i = (c - '0');                  // convert char
        num = ReadNum(i);               // get the rest of the number
      }
      else {
        command = c;                   // load the command buffer
        UpCase();                       // A...Z or '\0'
      }
    }   
  }
}

//-----------------------------------
// read a number from input stream
// given the most significant digit
// ----------------------------------

float ReadNum(int n){    
  float num = 0.0;        // accumulated value
  float frac = 0.0;       // fractional part
  float fracdig = 0.0;    // fraction scaling
  int sign = 1;           //
  char c;                 // single input character
  float t = 0.0;          // value for conversion
  float result = 0.0;     // holds accumulated value

  if (n < 0) {
    sign = -1;
    n = 0;
  }
  else {
    result = n;
  }
  t = n;                  // first digit passed as seed
  while (Serial.available() > 0) {
    c = Serial.read();
    delay(10);                          // debounce input

    if (c == 32) {                      // is it a space?
      result = result * sign;           // apply the sign
      return result;                    // return value and exit
    }
    else if (c == 46 ) {                // is it "."?
      fracdig = 10.0;                     // we're into a fraction
    }
    else if (c >= '0' && c <= '9') {      // accept only 0...9
      t = (c - '0');
      if (fracdig < 10.0) {
        result = result * 10.0 + t;  // build integer value
      }
      else {
        t = t / fracdig;
        result = result + t;                // append fractional part
        fracdig *= 10;                      // ready for next fractional digit
      }
    }
  }
  result = result * sign;
  return result;                         // we reached end of input stream
}



//-------------------------------------------------------
// upper case utility to convert command
// to all upper case to make commands case-insensitive
// non-alpha characters are set to '\0'
//-------------------------------------------------------
void UpCase() {
  if (command >= 97 && command <= 122) {
    command = command - 32;
  }
  if (command < 65 || command > 90) {
    command = '\0';
  }
}



// -------------------------------------------------------------
//   Power control routines
// -------------------------------------------------------------


void Reset() {
  pinMode(SSR,OUTPUT);      // set the SSR pin to OUTPUT
  digitalWrite(SSR,LOW);    // and turn it off

  contflag = true;          // global flag to break out of loops

  TimeOn = 0;               // milliseconds on time
  TimeOff = 0;              // milliseconds off time
  PowerTimeOut = 0;         // next time to cycle SSR power

  PowerOn = false;          // state of SSR output

  PowerLevel = 0.0;         // desired power level in Watts
  Watts = 0;                // actual power level (resolution based on CYCLETIME)

  TempPeriod = TEMPTIME;    // milliseconds to update temperature
  TempTimeOut = 0;          // end of reading period

  InitReading();            // reset all the reading arrays and variables  
}


void ShowSys() {
  Serial.print("TimeOn: ");       
  Serial.print(TimeOn);
  Serial.print(" TimeOff: ");     
  Serial.print(TimeOff);
  Serial.print(" PowerLevel: ");  
  Serial.print(PowerLevel);
  Serial.print(" Watts: ");       
  Serial.print(Watts);
  Serial.print(" TempPeriod: ");  
  Serial.print(TempPeriod);
  Serial.print(" TempTimeOut: "); 
  Serial.print(TempTimeOut);
  Serial.print(" Now: ");         
  Serial.println(millis());
  Serial.println();
}


void SetPower(long p) {
  if (p < 0) {
    p = 0;
  }
  else if (p > HEATER) {
    p = HEATER;
  }
  PowerLevel = p;
  TimeOn = (CYCLETIME * p) / HEATER;      // set time on
  Watts = (TimeOn * HEATER) / CYCLETIME;             // actual power level
  TimeOff = CYCLETIME - TimeOn;           // set time off
  PowerTimeOut = millis() + TimeOn;       // start time cycle
  PowerOn = true;                         // set power state
}



void ControlPower() {
  long now;

  now = millis();                        // get current time
  if (now > PowerTimeOut) {                   // we've timed out
    if (PowerOn) {            
      digitalWrite(SSR,LOW);             // toggle the SSR
      PowerTimeOut = now + TimeOff;           // reset timeout
    }
    else {
      digitalWrite(SSR,HIGH);
      PowerTimeOut = now + TimeOn;
    } 
    PowerOn = (PowerOn == false);        // toggle the power state
  }
} 

void DisplayPower() {                    // display the power at the console
  Serial.print(Watts);
  Serial.println(" Watts");
}


// -------------------------------------------------------------
//   Temperature Routines
// -------------------------------------------------------------


void InitReading() {
  int i = 0;
  readCount = 0;            // initialize counter and readings array
  for (i= 0; i <= 6; i++){
    reading[i] = 0;
  }
  TempTimeOut = millis() + TempPeriod;  // when to calculate next temp
}

void GetReading() {
  long now;
  int i = 0;

  reading[0] += analogRead(0);         // accumulate reading sums
  reading[1] += analogRead(1);
  reading[2] += analogRead(2);
  reading[3] += analogRead(3);
  reading[4] += analogRead(4);
  reading[5] += analogRead(5);

  readCount++;                         // bump the counter

  now = millis();                      // get the current time
  if (now > TempTimeOut) {             // have we timed out?
    ComputeTemp();                     // temp{} now has most recent temperatures                  
    InitReading();                     // reintialize readings
  }
}


// --------------------------------------------
//   display the readings to the output
// --------------------------------------------

void DisplayReadings() {
  int i = 0;

  Serial.print(readCount);
  Serial.print(" Readings:  ");
  for (i = 0; i <= MaxAnalog; i++) {
    Serial.print(reading[i]);
    Serial.print("  ");
  }
  Serial.println();
  Serial.println();
}



// ------------------------------------------
//  Convert accumulated reading[] to temp[]
// ------------------------------------------

void ComputeTemp() {
  long now = millis();
  int i = 0;
  float r;                                // readings holder
  float t;                                // scratch holder

  if (now > TempTimeOut) {
    for (i = 0; i <= MaxAnalog; i++) {
      r = reading[i] / readCount;           // find average

      if (r < LOWRANGE) {                   // use linear fit by range
        t = r * mXlow[i] + bXlow[i];
      }
      else if (r < MIDRANGE) {
        t = r * mXmid[i] + bXmid[i];
      }
      else {
        t = r * mXhi[i] + bXhi[i];
      }
      temp[i] = t + err(t,i);               // add the error correction
    }

    Serial.print("Watts: "); 
    Serial.print(Watts);  
    Serial.print("  ");
    DisplayTemp();
    InitReading();                         // reset accumulators and next timeout
  }
}


// ----------------------------------
//   error correction function
// ----------------------------------

float err(float t, int i) {
  float v = 0.0;
  long r;
  long s;


  s = (long) t * 100;

  if (s <= 6080) {
    r = map(s, 3060, 6080, err306[i], err608[i]);
  }
  else if (s <= 7500) {
    r = map(s, 6080, 7500, err608[i], err750[i]);
  }
  else if (s <= 8030) {
    r = map(s, 7500, 8030, err750[i], err803[i]);    
  }
  else if (s <= 9560) {
    r = map(s, 8030, 9560, err803[i], err956[i]);
  }
  else  {
    r = map(s, 9560, 10000, err956[i], err100[i]);
  }

  v = (float) r / 100.0;

  return v;
}



// ------------------------------------------
//    Display the current temps to console
// ------------------------------------------

void DisplayTemp() {
  int i = 0;
  float now = millis();
  now = now / 1000.0;

  Serial.print(now);
  Serial.print("  Temp ");
  for (i = 0; i <= MaxAnalog; i++) {
    Serial.print(temp[i]);
    Serial.print("  ");
  }
  Serial.println();
}
