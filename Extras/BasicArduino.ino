#include <Servo.h>

// Main Sensor pins (NPN NO - ACTIVE LOW)
const int SENSOR_A = 2;   // Before gate (entry)
const int SENSOR_B = 3;   // After gate (exit)
const int SENSOR_C = 4;   // Track section 1
const int SENSOR_D = 5;   // Track section 2

// Independent Security Sensor
const int SECURITY_SENSOR = 15;  // A1 pin

// Track LED pins
const int LED_A = 6;
const int LED_B = 7;
const int LED_C = 8;
const int LED_D = 11;

// Security LEDs and Buzzer
const int SECURITY_LED1 = 16;  // A2 pin
const int SECURITY_LED2 = 17;  // A3 pin
const int SECURITY_BUZZER = 18; // A4 pin

// Gate system pins
const int SERVO1_PIN = 9;
const int SERVO2_PIN = 10;
const int IND_SERVO_PIN = 14;  // A0 pin
const int BUZZER_PIN = 12;
const int GATE_LED_PIN = 13;

// Servo positions
const int GATE_OPEN = 90;
const int GATE_CLOSED = 0;

Servo gateServo1, gateServo2, indServo;
bool gateActive = false;
unsigned long trainExitTime = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastSensorCheck = 0;
bool sensorError = false;
String trainDirection = "None";

// Independent servo variables
int indServoPos = 0;
int indServoTarget = 180;
unsigned long lastIndMove = 0;
const int IND_MOVE_DELAY = 20;
unsigned long indPauseStart = 0;
const unsigned long IND_PAUSE_DURATION = 5000;

// Security sensor variables
bool securityAlarmActive = false;
unsigned long metalDetectedTime = 0;
const unsigned long ALARM_OFF_DELAY = 2000;  // 2 seconds delay

void setup() {
  // Configure sensor pins with PULLUP resistors
  pinMode(SENSOR_A, INPUT_PULLUP);
  pinMode(SENSOR_B, INPUT_PULLUP);
  pinMode(SENSOR_C, INPUT_PULLUP);
  pinMode(SENSOR_D, INPUT_PULLUP);
  pinMode(SECURITY_SENSOR, INPUT_PULLUP);
  
  // Configure LED pins
  pinMode(LED_A, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_C, OUTPUT);
  pinMode(LED_D, OUTPUT);
  pinMode(SECURITY_LED1, OUTPUT);
  pinMode(SECURITY_LED2, OUTPUT);
  
  // Configure buzzers
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GATE_LED_PIN, OUTPUT);
  pinMode(SECURITY_BUZZER, OUTPUT);
  
  // Attach servos
  gateServo1.attach(SERVO1_PIN);
  gateServo2.attach(SERVO2_PIN);
  indServo.attach(IND_SERVO_PIN);
  
  // Initialize outputs
  openGate();
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GATE_LED_PIN, LOW);
  digitalWrite(SECURITY_LED1, LOW);
  digitalWrite(SECURITY_LED2, LOW);
  digitalWrite(SECURITY_BUZZER, LOW);
  indServo.write(indServoPos);
  
  // Start serial monitor
  Serial.begin(9600);
  Serial.println(F("\n\nRAILWAY CROSSING SYSTEM"));
  Serial.println(F("======================================="));
  Serial.println(F("Hardware Configuration:"));
  Serial.println(F("- Main Sensors: LJ12A3-4-ZBX (NPN NO)"));
  Serial.println(F("- Security Sensor: Independent LJ12A3"));
  Serial.println(F("- Controller: Arduino Uno"));
  Serial.println(F("======================================="));
  Serial.println(F("System Initialized - Waiting for train..."));
  Serial.println(F("-----------------------------------------"));
  printSystemHeader();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Read main sensor states
  bool trainAtA = !digitalRead(SENSOR_A);
  bool trainAtB = !digitalRead(SENSOR_B);
  bool trainAtC = !digitalRead(SENSOR_C);
  bool trainAtD = !digitalRead(SENSOR_D);
  
  // Control track LEDs
  digitalWrite(LED_A, trainAtA ? HIGH : LOW);
  digitalWrite(LED_B, trainAtB ? HIGH : LOW);
  digitalWrite(LED_C, trainAtC ? HIGH : LOW);
  digitalWrite(LED_D, trainAtD ? HIGH : LOW);
  
  // Determine train direction
  updateTrainDirection(trainAtA, trainAtB, trainAtC, trainAtD);
  
  // Gate control logic
  handleGateSystem(trainAtA, trainAtB);
  
  // Security sensor logic
  handleSecuritySensor(currentMillis);
  
  // Sensor health check
  if (currentMillis - lastSensorCheck >= 2000) {
    lastSensorCheck = currentMillis;
    checkSensorHealth();
  }
  
  // Independent servo control
  controlIndependentServo(currentMillis);
  
  // Serial update
  if (currentMillis - lastSerialUpdate >= 500) {
    lastSerialUpdate = currentMillis;
    printSystemStatus(trainAtA, trainAtB, trainAtC, trainAtD, currentMillis);
  }
}

void handleSecuritySensor(unsigned long currentMillis) {
  // Read security sensor state (inverted for NPN NO)
  bool metalDetected = !digitalRead(SECURITY_SENSOR);
  
  // Metal NOT detected: Turn on both LEDs and buzzer immediately
  if (!metalDetected) {
    digitalWrite(SECURITY_LED1, HIGH);
    digitalWrite(SECURITY_LED2, HIGH);
    digitalWrite(SECURITY_BUZZER, HIGH);
    securityAlarmActive = true;
    metalDetectedTime = 0;  // Reset timer
    return;
  }
  
  // Metal detected: Start timer if not already started
  if (metalDetected && securityAlarmActive && metalDetectedTime == 0) {
    metalDetectedTime = currentMillis;
    Serial.println(F("\n>> METAL DETECTED ON SECURITY SENSOR <<"));
    Serial.println(F("- Starting 2-second countdown to turn off alarm"));
    Serial.println(F("-----------------------------------------"));
  }
  
  // Metal detected for 2 seconds: Turn off both LEDs and buzzer
  if (metalDetected && securityAlarmActive && metalDetectedTime > 0 && 
      (currentMillis - metalDetectedTime >= ALARM_OFF_DELAY)) {
    digitalWrite(SECURITY_LED1, LOW);
    digitalWrite(SECURITY_LED2, LOW);
    digitalWrite(SECURITY_BUZZER, LOW);
    securityAlarmActive = false;
    metalDetectedTime = 0;
    Serial.println(F("\n>> SECURITY ALARM DEACTIVATED <<"));
    Serial.println(F("- 2-second delay complete"));
    Serial.println(F("- Security LEDs and Buzzer OFF"));
    Serial.println(F("---------------------------------"));
  }
}

void controlIndependentServo(unsigned long currentMillis) {
  // Check if paused
  if (indPauseStart > 0) {
    if (currentMillis - indPauseStart >= IND_PAUSE_DURATION) {
      indPauseStart = 0;
      indServoTarget = (indServoTarget == 0) ? 180 : 0;
    }
    return;
  }
  
  // Move servo
  if (currentMillis - lastIndMove >= IND_MOVE_DELAY) {
    lastIndMove = currentMillis;
    
    if (indServoPos < indServoTarget) indServoPos++;
    else if (indServoPos > indServoTarget) indServoPos--;
    
    indServo.write(indServoPos);
    
    if (indServoPos == indServoTarget) {
      indPauseStart = currentMillis;
    }
  }
}

void updateTrainDirection(bool a, bool b, bool c, bool d) {
  if (!gateActive) {
    if (a && !b && !c && !d) trainDirection = "A → B";
    else if (!a && b && !c && !d) trainDirection = "B → A";
    else if (!a && !b && c && !d) trainDirection = "C → Gate";
    else if (!a && !b && !c && d) trainDirection = "D → Gate";
    else if (a && b) trainDirection = "At Gate";
    else if (c && d) trainDirection = "Between C-D";
    else trainDirection = "None";
  }
}

void handleGateSystem(bool trainAtA, bool trainAtB) {
  if (sensorError) return;
  
  if (!gateActive && (trainAtA || trainAtB)) {
    gateActive = true;
    trainExitTime = 0;
    activateGateSystem();
    
    Serial.println(F("\n>> TRAIN DETECTED <<"));
    Serial.print(F("Direction: "));
    Serial.println(trainDirection);
    Serial.println(F("ACTIVATING GATE SYSTEM"));
    Serial.println(F("- Closing gate"));
    Serial.println(F("- Buzzer ON"));
    Serial.println(F("- Gate LED ON"));
    Serial.println(F("---------------------------------"));
  }
  
  if (gateActive) {
    if (trainAtA || trainAtB) {
      trainExitTime = 0;
    } else {
      if (trainExitTime == 0) {
        trainExitTime = millis();
        Serial.println(F(">> Train cleared gate area"));
        Serial.println(F(">> Starting 5s safety delay..."));
      } else if (millis() - trainExitTime >= 5000) {
        deactivateGateSystem();
      }
    }
  }
}

void activateGateSystem() {
  closeGate();
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(GATE_LED_PIN, HIGH);
}

void deactivateGateSystem() {
  gateActive = false;
  trainExitTime = 0;
  openGate();
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GATE_LED_PIN, LOW);
  
  Serial.println(F("\n>> SYSTEM DEACTIVATED <<"));
  Serial.println(F("- Opening gate"));
  Serial.println(F("- Buzzer OFF"));
  Serial.println(F("- Gate LED OFF"));
  Serial.println(F("---------------------------------"));
}

void closeGate() {
  for (int pos = GATE_OPEN; pos >= GATE_CLOSED; pos--) {
    gateServo1.write(pos);
    gateServo2.write(pos);
    delay(15);
  }
}

void openGate() {
  for (int pos = GATE_CLOSED; pos <= GATE_OPEN; pos++) {
    gateServo1.write(pos);
    gateServo2.write(pos);
    delay(15);
  }
}

void printSystemHeader() {
  Serial.println(F("SENSORS | TRACK LEDs | GATE SYSTEM | SECURITY | IND SERVO"));
  Serial.println(F("A B C D E|A B C D    | STATE       | ALARM    | POS/STATUS"));
  Serial.println(F("-----------------------------------------------------------"));
}

void printSystemStatus(bool a, bool b, bool c, bool d, unsigned long currentMillis) {
  // Sensor status (including security sensor)
  Serial.print(a ? "1 " : "0 ");
  Serial.print(b ? "1 " : "0 ");
  Serial.print(c ? "1 " : "0 ");
  Serial.print(d ? "1 " : "0 ");
  Serial.print(!digitalRead(SECURITY_SENSOR) ? "1 | " : "0 | ");
  
  // LED status
  Serial.print(a ? "1 " : "0 ");
  Serial.print(b ? "1 " : "0 ");
  Serial.print(c ? "1 " : "0 ");
  Serial.print(d ? "1    | " : "0    | ");
  
  // Gate system status
  if (gateActive) {
    Serial.print(F("ACTIVE  | "));
    if (trainExitTime > 0) {
      int remaining = 5 - ((currentMillis - trainExitTime) / 1000);
      Serial.print(remaining > 0 ? remaining : 0);
      Serial.print(F("s     | "));
    } else {
      Serial.print(F("PRESENT | "));
    }
  } else {
    Serial.print(F("INACTIVE| --      | "));
  }
  
  // Security alarm status
  if (securityAlarmActive) {
    if (metalDetectedTime > 0) {
      int remaining = (ALARM_OFF_DELAY - (currentMillis - metalDetectedTime)) / 1000;
      Serial.print(remaining > 0 ? remaining : 0);
      Serial.print(F("s     | "));
    } else {
      Serial.print(F("ON      | "));
    }
  } else {
    Serial.print(F("OFF     | "));
  }
  
  // Independent servo status
  if (indPauseStart > 0) {
    Serial.print(F("PAUSED @ "));
    Serial.print(indServoPos);
    Serial.print(F("° ("));
    Serial.print((IND_PAUSE_DURATION - (currentMillis - indPauseStart)) / 1000);
    Serial.print(F("s left)"));
  } else {
    Serial.print(F("MOVING: "));
    Serial.print(indServoPos);
    Serial.print(F("° → "));
    Serial.print(indServoTarget);
    Serial.print(F("°"));
  }
  
  // Sensor error indicator
  if (sensorError) {
    Serial.print(F(" | SENSOR ERROR!"));
  }
  Serial.println();
}

void checkSensorHealth() {
  static unsigned long sensorActiveTime[5] = {0};
  const long ERROR_THRESHOLD = 300000;
  
  bool sensors[5] = {
    !digitalRead(SENSOR_A),
    !digitalRead(SENSOR_B),
    !digitalRead(SENSOR_C),
    !digitalRead(SENSOR_D),
    !digitalRead(SECURITY_SENSOR)
  };
  
  for (int i = 0; i < 5; i++) {
    if (sensors[i]) {
      if (sensorActiveTime[i] == 0) {
        sensorActiveTime[i] = millis();
      } else if (millis() - sensorActiveTime[i] > ERROR_THRESHOLD) {
        sensorError = true;
        Serial.println(F("\n!! SENSOR ERROR DETECTED !!"));
        Serial.print(F("Sensor "));
        Serial.print(char('A' + i));
        Serial.println(F(" stuck in active state!"));
        Serial.println(F("System in safety mode - Gate disabled"));
        Serial.println(F("-------------------------------------"));
        return;
      }
    } else {
      sensorActiveTime[i] = 0;
    }
  }
  sensorError = false;
}
