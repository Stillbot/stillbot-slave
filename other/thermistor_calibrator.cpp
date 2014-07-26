// ---------------------------------------------------------------------------
//  Jughead thermistor calibration
//  
//  Use this to get the linear fit parameters
//  for the thermitors for the Jughead Arduino Shizeld
//  
//  Creative Commons Attribution-Share Alike 2.5 2009 by Snuffy
// ---------------------------------------------------------------------------


#define MaxAnalog 5          // use ports 0 - 5
#define Resolution 20        // read and average #Resolution times
#define TableSize 30         // array size - limited by SRAM to 30 or so

float readings[7][TableSize];        // readings[0][i] is temp deg C 
// readings[1..MaxAnalog][i] averaged analog values from A/D ports
// standard is reading[j][i]  where j is port and i is reading#

// ---- Stats variables and arrays ----
float sumY;                   // Y is the calibration temp
float averageY;
float deviationY;
float stddevY;

float sumX[6];                // X is the port reading 0..5
float sumX2[6];
float sumXY[6];

float averageX[6];             
float deviationX[6];
float stddevX[6];
float minX[6] = {1100.0,1100.0,1100.0,1100.0,1100.0,1100.0};
float maxX[6] = {0.0,0.0,0.0,0.0,0.0,0.0};


// ------- linear fit parameters ------
// ---- derived from testing ----------
float mX[6] = {0.1203, 0.1224, 0.1217, 0.1229, 0.1232, 0.1240}; // slope
//float bX[6] = {13.75, 14.08, 14.61, 14.84, 14.29, 13.96};       // Y intercept - original
float bX[6] = {12.68, 13.13, 13.62, 13.97, 13.57, 13.16};       // Y intercept - corrected
boolean contflag = true;       // global flag to break out of loops

char buffer[255];              // I/O buffer string

char space[] = "    ";         // padding string
char colnums[] = "         0       1      2       3       4       5";

void setup(){
  Serial.begin(19200);      // start serial baud rate
}


void loop(){
  Calibrate();
}

void Calibrate(){
  int i = -1;                    // counters
  int j = 0;
  float f = 0.0;                // holder for calibration temp
  int MaxReadings = 0;          // maximum # of calibration readings
  boolean testing = false;
  delay(500);               // wait for serial port intialization
  
  Serial.println();
  Serial.println("begin");

  while (++i < TableSize){ 
    if (testing) {
      f = i + 1;              // testing find noise level
      delay(500);           // testing wait 0.50 sec
    }
    else {
      f = GetTemp();        // get calibration temp from console
    }

    if (contflag == false ) {
      break;                // bail out: we're done
    }

    readings[6][i] = f;   // store the calibration temp
    Serial.print(i);
    Serial.print(")");
    Serial.print(space);
    Serial.print(f);
    Serial.print(space);

    for (j = 0; j <= MaxAnalog; j++) { 
      readings[j][i] = ReadTemp(j);    // read thermistor values
      Serial.print(readings[j][i]);
      Serial.print(space);
      if (readings[j][i] < minX[j]) minX[j] = readings[j][i];
      if (readings[j][i] > maxX[j]) maxX[j] = readings[j][i];
    }   //  for j
    Serial.println();
  }  // while i
 
  // done with readings

  MaxReadings = i;
  
  //DisplayReadings(MaxReadings);   // output readings to console
  
  DoStats(MaxReadings);           // Compute stats on readings 
  DisplayStats();                 // output stats to console

  LinFit(MaxReadings);
  DisplayLin(MaxAnalog + 1);

}


// ***********************************************
//        function definitions
//************************************************


// -----------------------------------------------------------------
// GetTemp waits for a numberical input from console, returns value
// if user enters "#" instead of a number, sets contflag to false
//   and returns zero.
// -----------------------------------------------------------------

float GetTemp(){          
  float result = 0.0;     // accumulated value
  float frac = 0.0;       // fractional part
  float fracdig = 0.0;    // fraction scaling
  char c;                 // single input character
  float t = 0.0;          // temp value for conversion


  while (!Serial.available() ) {
    delay(100);            // wait for input
  }

  while (Serial.available() > 0) {
    c = Serial.read();
    delay(10);                          // debounce input
    if (c == 35) {                      // is it "#"?
      contflag = false;                 // done with input
      return 0.0;                       // dummy return value is ignored
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
  return result;
}

// -------------------------------------------------------
// ReadTemp returns float average of specified analog port
// -------------------------------------------------------

float  ReadTemp(int Port) {
  int r = 0;                  // analog read value
  int sum = 0;                // acumulator
  float result = 0.0;         // return value

  for (int t = 0; t < Resolution; t++) {    // read #Resolution times
    r = analogRead(Port);
    sum += r;                  // sum results
    delay(2);
  }
  result = float(sum) / float(Resolution);   // average results
  return result;
}

//-------------------------------------------
// DoCalibration returns # readings 
// fills global array readings[][]
// until end of array is reached - testing mode
// or user enters <#> at console
// ------------------------------------------

int DoCalibration(boolean testing = false) {  // defaults to not testing
  int i = 0;                    // counters
  int j = 0;
  float f = 0.0;                // holder for calibration temp

  Serial.println("**** DoCalibration ****");

  do{
    if (testing) f = GetTemp();   // get calibration temp from console
    else f = i + 1.0;             // testing: dummy values for f 

    if (!contflag) break;   // bail out of do-while: we're done

    readings[6][i] = f;   // store the calibration temp

    for (j = 0; j <= MaxAnalog; j++) 
      readings[j][i] = ReadTemp(j);    // read thermistor values
  } 
  while (++i < TableSize);  
  // done with readings 

  return i;      // max count of readings: 
                 //   readings[j][i-1]is last valid reading
}

// ------------------------------------------------------------
//  DisplayReadings outputs the contents of readings[]
//  to the console
//  N is the maxiumum # of readings
// -------------------------------------------------------------

void DisplayReadings(int N) {

  Serial.print(N);
  Serial.println(" Calibration readings");

  for (int i = 0; i < N; i++) {
    Serial.print(i);
    Serial.print(") ");
    for (int j = 0; j <= MaxAnalog; j++) {
      Serial.print(readings[j][i]);
      Serial.print(space);
    }
    Serial.print("Cal ");
    Serial.print(readings[6][i]);
    Serial.println();
  }  
}

// ------------------------------------------------------------
//  DoStats -- average, deviation, sumX, sumX2, sumXY, sumX2Y
//  passed N as the # of readings taken = population

void DoStats(int N) {
  int i = 0;
  int j = 0;
  
  float x = 0.0;
  float y = 0.0;
  float x2 = 0.0;
  float t = 0.0;              // temp scratch variable
  
  sumY = 0.0;                 // zero out all stats

  for (j = 0; j < 6; j++) {
    sumX[j] = 0.0;
    sumXY[j] = 0.0;
    sumX2[j] = 0.0;    
  }

  for (i = 0; i < N; i++) {
    y = readings[6][i];                    // y is calibration temp
    sumY += y;                             // find all the SigmaYs
   
    for (j = 0; j <= MaxAnalog; j++) {
      x = readings[j][i];
      x2 = x * x;
      sumX[j] += x;                            // find all the SigmaXs
      sumXY[j] += (x * y);
      sumX2[j] += x2;
    } // for j
  }  // for i

  averageY = sumY / N;                       // find the averages
  deviationY = 0.0;                          // mean deviations
  stddevY = 0.0;                             // and standard deviations

  for (j = 0; j < 6; j++) {
    averageX[j] = sumX[j] / N;
    deviationX[j] = 0.0;
    stddevX[j] = 0.0;
  }  // for j

  for (i = 0; i < N; i++) {
    t = readings[6][i] - averageY;
    deviationY += abs(t);
    stddevY += t * t;  
    for (j = 0; j <= MaxAnalog; j++) {
      t = readings[j][i] - averageX[j];
      deviationX[j] += abs(t);
      stddevX[j] = t * t;
    } // for j
  }  // for i

  deviationY = deviationY / N;               // mean deviations
  stddevY = sqrt(stddevY / N);               // and standard deviations

  for (j = 0; j < 6; j++) {
    deviationX[j] = deviationX[j] / N;
    stddevX[j] = sqrt(stddevX[j] / N);
  }  // for j
}


//  --------------------------------------------------
//          DisplayStats
//----------------------------------------------------

void DisplayStats() {
  int i = 0;
  float err = 0.0;
  
  Serial.println("AvgY  DevY   StdvY");
  Serial.print(averageY);                 // show  the average
  Serial.print(space);
  Serial.print(deviationY);              // mean deviation
  Serial.print(space);  
  Serial.println(stddevY);               // and standard deviation
  Serial.println(space);  
  // now print out a table for the X states
  Serial.println(colnums);
  
  Serial.print("MaxX    ");
  for (i = 0; i < 6; i++) {
    Serial.print(maxX[i]);
    Serial.print(space);     
  }  // for i
  Serial.println();  
  
  Serial.print("MinX    ");
  for (i = 0; i < 6; i++) {
    Serial.print(minX[i]);
    Serial.print(space);     
  }  // for i
  Serial.println();  
  
  Serial.print("AvgX    ");
  for (i = 0; i < 6; i++) {
    Serial.print(averageX[i]);
    Serial.print(space);     
  }  // for i
  Serial.println();
  
    Serial.print("devX    ");
  for (i = 0; i < 6; i++) {
    Serial.print(deviationX[i]);
    Serial.print(space);     
  }  // for i
  Serial.println();  
  
    Serial.print("StdevX  ");
  for (i = 0; i < 6; i++) {
    Serial.print(stddevX[i] * 100);
    Serial.print(space);     
  }  // for i
  Serial.println(); 
 
  Serial.print("ErrX    ");
  for (i = 0; i < 6; i++) {
    err = abs(minX[i] - averageX[i]);
    if (err < abs(maxX[i] - averageX[i])) 
      err = abs(maxX[i] - averageX[i]);
    Serial.print(err);
    Serial.print(space);     
  }  // for i
  Serial.println();  
}

//--------------------------------------------
//   LinFit - linear least squares fit
//     passed N:  # of readings
//     puts results in global arrays
//      mX[]  - slope
//      bX[]  - Y intercept
//--------------------------------------------

void LinFit(int N) {
  int j;
  float t1;            // scratch variables
  float t2;
  
  for (j = 0; j < 6; j++) {
      t2 = (N * sumX2[j]) - (sumX[j] * sumX[j]);      // lower term common to all
      t1 = (sumY * sumX2[j]) - (sumX[j] * sumXY[j]);  // Y intercept numerator
      bX[j] = t1 / t2;                                // find Y intercept
      t1 = (N * sumXY[j]) - (sumX[j] * sumY);         // slope numerator
      mX[j] = t1 / t2;
  }
}

// -------------------------------------
//  Display the linear fit parameters
// -------------------------------------

void  DisplayLin(int N) {
  int j;
  
  Serial.println(colnums);
  Serial.print("M*100 ");
  Serial.print(space);
  for (j = 0; j < N; j++) {
    Serial.print(mX[j] * 100);
    Serial.print(space);
  }
  Serial.println();
  
  Serial.print("b    ");
  Serial.print(space);
  for (j = 0; j < N; j++) {
    Serial.print(bX[j]);
    Serial.print(space);
  }
  Serial.println();  
  Serial.println();    
}


// -----------------------------------------------------------------
//  -----------------  Check linearity -----------------------------
// -----------------------------------------------------------------


void  CheckTemp() {
  int i = -1;                    // counters
  int j = 0;
  float f = 0.0;                // holder for calibration temp
  float error = 0.0;            // error of linfit from calibration temp
  float errAvg[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  //  average error over range
  int MaxReadings = 0;          // maximum # of calibration readings
  
  delay(500);               // wait for serial port intialization
  
  Serial.println();
  Serial.println("begin");

  while (++i < TableSize){ 
    f = GetTemp();        // get calibration temp from console

    if (contflag == false ) {
      break;                // bail out: we're done
    }

    readings[6][i] = f;   // store the calibration temp
    Serial.print(i);
    Serial.print(")");
    Serial.print(space);
    Serial.print(f);
    Serial.print(space);

    for (j = 0; j <= MaxAnalog; j++) { 
      f = ReadTemp(j);                 // read thermistor values
      readings[j][i] = f * mX[j] + bX[j];    // Convert to Celsius      
      Serial.print(readings[j][i]);
      Serial.print(space);
    }   //  for j
    Serial.println();
    
    Serial.print("Error:        ");              // show error of lin fit
    for (j = 0; j <= MaxAnalog; j++) {
      error = readings[j][i] - readings[6][i];
      errAvg[j] += abs(error);
      Serial.print(error);
      Serial.print(space);
    }
    Serial.println();
    
  }  // while i
  // done with readings
  MaxReadings = i;
  Serial.println();
  
  Serial.print("AvgErr:  ");
  
  for (j = 0; j <= MaxAnalog; j++) {
    errAvg[j] = errAvg[j] / MaxReadings;
    Serial.print(errAvg[j]);
    Serial.print(space);
  }
  Serial.println();
  
  
}
