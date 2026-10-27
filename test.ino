const int select_button = 2;
const int right_button = 3;
const int OhmMeter = A0;
const int CapacitancMeter = A1;
const int VoltMeter = A2;
const int Ammeter = A3;
const int R3 = 6;
const int R2 = 5;
const int R1 = 4;
const int ChargePin = 13;
const int DischargePin = 11;

boolean is_select = false;
int navigator = 0;
float R = 0.00;
float V = 0.00;
float I = 0.00;
float C = 0.00;
boolean nano = false;
boolean kilo = false;
boolean mili = false;

// Global variable for ACS712 auto-zero offset
float zero_voltage = 2.47; 

void calculate_resistor() {
  float v_ref = 4.94;
  float r1 = 0.00, r_ref1 = 1000.00, adc_value1 = 0.00, voltage1 = 0.00;
  float r2 = 0.00, r_ref2 = 10000.00, adc_value2 = 0.00, voltage2 = 0.00;
  float r3 = 0.00, r_ref3 = 100000.00, adc_value3 = 0.00, voltage3 = 0.00;
  
  pinMode(R1, OUTPUT); pinMode(R2, INPUT); pinMode(R3, INPUT);
  digitalWrite(R1, HIGH);
  for (int i = 0; i < 20 ; i++) { adc_value1 += analogRead(OhmMeter); delay(3); }
  adc_value1 /= 20;
  if (adc_value1 < 1022.90) { voltage1 = (adc_value1 * v_ref) / 1024; r1 = (voltage1 * r_ref1) / (v_ref - voltage1); }
  
  pinMode(R1, INPUT); pinMode(R2, OUTPUT); pinMode(R3, INPUT);
  digitalWrite(R2, HIGH);
  for (int i = 0; i < 20 ; i++) { adc_value2 += analogRead(OhmMeter); delay(3); }
  adc_value2 /= 20;
  if (adc_value2 < 1022.90) { voltage2 = (adc_value2 * v_ref) / 1024; r2 = (voltage2 * r_ref2) / (v_ref - voltage2); }
  
  pinMode(R1, INPUT); pinMode(R2, INPUT); pinMode(R3, OUTPUT);
  digitalWrite(R3, HIGH);
  for (int i = 0; i < 20 ; i++) { adc_value3 += analogRead(OhmMeter); delay(3); }
  adc_value3 /= 20;
  if (adc_value3 < 1022.90) { voltage3 = (adc_value3 * v_ref) / 1024; r3 = (voltage3 * r_ref3) / (v_ref - voltage2); }
  
  r1 /= 1000; r2 /= 1000; r3 /= 1000;
  if (r1 < 2 && r2 < 101 && r3 < 1001) R = r1 * 1000;
  else if (r1 > 2 && r2 < 101 && r3 < 1001) R = r2;
  else if (r1 > 2 && r2 > 101 && r3 < 2000) R = r3;
  else R = 0.00;
  
  if (R < 1) { R *= 1000; kilo = false; } else { kilo = true; }
}

void calculate_capacitance() {
  unsigned long start_time, elapsed_time;
  float microFarads;
  
  // Ensure capacitor is fully discharged before starting
  pinMode(DischargePin, OUTPUT);
  digitalWrite(DischargePin, LOW);
  delay(100);
  pinMode(DischargePin, INPUT); 
  
  digitalWrite(ChargePin, HIGH);
  start_time = millis();
  
  // Wait to reach 63.2% charge (~648 on ADC) with a 3-second timeout guard
  while (analogRead(CapacitancMeter) < 648) {
    if (millis() - start_time > 3000) {
      C = 0.00;
      nano = false;
      digitalWrite(ChargePin, LOW);
      pinMode(DischargePin, OUTPUT);
      digitalWrite(DischargePin, LOW);
      delay(100);
      pinMode(DischargePin, INPUT);
      return;
    }
  }
  
  elapsed_time = millis() - start_time;
  microFarads = (float)elapsed_time / 10.0; // Corrected 10k RC math formula
  
  if (microFarads > 1.0) { 
    C = microFarads; 
    nano = false; 
  } else { 
    C = microFarads * 1000.0; 
    nano = true; 
  }
  
  // Clean discharge after measurement
  digitalWrite(ChargePin, LOW);
  pinMode(DischargePin, OUTPUT);
  digitalWrite(DischargePin, LOW);
  delay(100);
  pinMode(DischargePin, INPUT);
}

void calculate_voltage() {
  float R1 = 10000.00, R2 = 4700.00, v_ref = 5.00;
  float resistor_ratio = R2 / (R1 + R2);
  float adc_value = 0.00;
  
  for (int i = 0; i < 20 ; i++) { adc_value += analogRead(VoltMeter); delay(3); }
  adc_value /= 20;
  
  // Safety filter to prevent floating baseline noise
  if (adc_value < 5) {
    V = 0.00;
  } else {
    V = ((adc_value * v_ref) / 1024) / resistor_ratio;
  }
}

void calculate_current() {
  int sensitivity = 185; // 185 for 5A ACS712 model
  int adc_value = 0;
  float v_ref = 4.94;
  float voltage = 0.00;
  float pure_voltage = 0.00;
  
  for (int i = 0; i < 40 ; i++) {
    adc_value += analogRead(Ammeter);
    delay(2);
  }
  adc_value /= 40;
  voltage = ((adc_value * v_ref) / 1024);
  
  pure_voltage = (voltage - zero_voltage) * 1000; 
  I = pure_voltage / sensitivity; 
  
  // Deadzone filter to lock idle noise to 0.00
  if (I > -0.15 && I < 0.15) {
    I = 0.00;
  }
  
  if (I < 1 && I > -1) {
    I = I * 1000;
    mili = true;
  } else {
    mili = false;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(right_button, INPUT_PULLUP);
  pinMode(select_button, INPUT_PULLUP);
  pinMode(ChargePin, OUTPUT);
  digitalWrite(ChargePin, LOW);
  
  Serial.println("=== DIY Multimeter Initialized ===");
  
  // Auto-zero calibration on startup (Keep motor/load OFF when resetting Arduino!)
  long sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += analogRead(Ammeter);
    delay(2);
  }
  float avg_adc = (float)sum / 100.0;
  zero_voltage = (avg_adc * 4.94) / 1024.0;
  
  Serial.print("Calibrated Zero Voltage: ");
  Serial.println(zero_voltage);
  Serial.println("Ready! Use Right Button to change modes, Select to enter.");
}

void loop() {
  if (digitalRead(right_button) == 0) {
    navigator++;
    while (digitalRead(right_button) == 0);
    delay(5);
    if (navigator > 3) navigator = 0;
    
    Serial.print("Selected Mode: ");
    if (navigator == 0) Serial.println("Resistor");
    else if (navigator == 1) Serial.println("Voltage");
    else if (navigator == 2) Serial.println("Current");
    else if (navigator == 3) Serial.println("Capacitor");
  }

  if (digitalRead(select_button) == 0) {
    is_select = true;
    while (digitalRead(select_button) == 0);
    Serial.println("--- Entering Measurement Mode ---");
  }

  if (navigator == 0) {
    while (is_select) {
      calculate_resistor();
      Serial.print("R = "); Serial.print(R);
      if (kilo) Serial.println(" kOhms"); else Serial.println(" Ohms");
      delay(500);
      if (digitalRead(select_button) == 0) { 
        is_select = false; 
        while (digitalRead(select_button) == 0); 
        Serial.println("--- Exiting Mode ---");
        Serial.println("=== BACK ON MAIN MENU ===");
      }
    }
  }

  if (navigator == 1) {
    while (is_select) {
      calculate_voltage();
      Serial.print("V = "); Serial.print(V); Serial.println(" v");
      delay(500);
      if (digitalRead(select_button) == 0) { 
        is_select = false; 
        while (digitalRead(select_button) == 0); 
        Serial.println("--- Exiting Mode ---");
        Serial.println("=== BACK ON MAIN MENU ===");
      }
    }
  }

  if (navigator == 2) {
    while (is_select) {
      calculate_current();
      Serial.print("I = "); Serial.print(I);
      if (mili) Serial.println(" mA"); else Serial.println(" A");
      delay(500);
      if (digitalRead(select_button) == 0) { 
        is_select = false; 
        while (digitalRead(select_button) == 0); 
        Serial.println("--- Exiting Mode ---");
        Serial.println("=== BACK ON MAIN MENU ===");
      }
    }
  }

  if (navigator == 3) {
    while (is_select) {
      calculate_capacitance();
      Serial.print("C = "); Serial.print(C);
      if (nano) Serial.println(" nF"); else Serial.println(" uF");
      delay(500);
      if (digitalRead(select_button) == 0) { 
        is_select = false; 
        while (digitalRead(select_button) == 0); 
        Serial.println("--- Exiting Mode ---");
        Serial.println("=== BACK ON MAIN MENU ===");
      }
    }
  }
}