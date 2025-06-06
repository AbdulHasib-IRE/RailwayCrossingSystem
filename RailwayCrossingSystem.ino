#include <WiFi.h>
#include <WebServer.h>
#include <ESP_Mail_Client.h>
#include <ESP32Servo.h>

// WiFi Configuration
const char* ssid = "##################";
const char* password = "************";

// Email Configuration
#define SENDER_EMAIL "&&&&&&&&&&&&&&&&&"
#define SENDER_APP_PASSWORD "******************"
#define RECIPIENT_EMAIL "sm.abdulhasib.bd@gmail.com"

// System Components
WebServer server(80);
SMTPSession smtp;
ESP_Mail_Session emailSession;
bool emailSent = false;
bool wifiConnected = false;
unsigned long lastWifiAttempt = 0;
const long WIFI_RETRY_INTERVAL = 30000;

// Sensor Pins (NPN NO - ACTIVE LOW)
const int SENSOR_A = 4;
const int SENSOR_B = 5;
const int SENSOR_C = 18;
const int SENSOR_D = 19;
const int SECURITY_SENSOR = 26;

// LED Pins
const int LED_A = 21;
const int LED_B = 22;
const int LED_C = 23;
const int LED_D = 2;
const int SECURITY_LED1 = 27;
const int SECURITY_LED2 = 32;

// Buzzer and Servo Pins
const int SECURITY_BUZZER = 33;
const int SERVO1_PIN = 13;
const int SERVO2_PIN = 12;
const int IND_SERVO_PIN = 25;
const int BUZZER_PIN = 15;
const int GATE_LED_PIN = 14;

// Servo Positions
const int GATE_OPEN = 90;
const int GATE_CLOSED = 0;

Servo gateServo1;
Servo gateServo2;
Servo indServo;

// System State Variables
bool gateActive = false;
bool sensorError = false;
bool securityAlarmActive = false;
String trainDirection = "None";
String systemStatus = "Initializing";
String securityStatus = "Normal";
String gateStatus = "Open";
String wifiStatus = "Disconnected";

// Timing Variables
unsigned long trainExitTime = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastSensorCheck = 0;
unsigned long metalDetectedTime = 0;
unsigned long lastIndMove = 0;
unsigned long indPauseStart = 0;

// Independent Servo Control
int indServoPos = 0;
int indServoTarget = 180;
const int IND_MOVE_DELAY = 20;
const unsigned long IND_PAUSE_DURATION = 5000;
const unsigned long ALARM_OFF_DELAY = 2000;

void setup() {
  Serial.begin(115200);
  
  // Allocate timers for ESP32Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  setupPins();
  setupEmail();
  connectToWiFi();
  
  server.on("/", handleRoot);
  server.begin();
  
  updateSystemStatus("System Ready");
  Serial.println("\n\nRAILWAY CROSSING SYSTEM - ESP32");
  Serial.println("================================");
  printSystemHeader();
}

void setupPins() {
  // Configure sensor pins
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
  
  // Initialize outputs
  digitalWrite(LED_A, LOW);
  digitalWrite(LED_B, LOW);
  digitalWrite(LED_C, LOW);
  digitalWrite(LED_D, LOW);
  digitalWrite(SECURITY_LED1, LOW);
  digitalWrite(SECURITY_LED2, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GATE_LED_PIN, LOW);
  digitalWrite(SECURITY_BUZZER, LOW);
  
  // Configure servos
  gateServo1.setPeriodHertz(50);
  gateServo2.setPeriodHertz(50);
  indServo.setPeriodHertz(50);
  
  gateServo1.attach(SERVO1_PIN, 500, 2400);
  gateServo2.attach(SERVO2_PIN, 500, 2400);
  indServo.attach(IND_SERVO_PIN, 500, 2400);
  
  // Initialize servos
  openGate();
  indServo.write(indServoPos);
}

void setupEmail() {
  emailSession.server.host_name = "smtp.gmail.com";
  emailSession.server.port = 465;
  emailSession.login.email = SENDER_EMAIL;
  emailSession.login.password = SENDER_APP_PASSWORD;
  emailSession.login.user_domain = "";
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  wifiStatus = "Connecting...";
  lastWifiAttempt = millis();
}

void sendEmailAlert() {
  if (!wifiConnected) return;
  
  SMTP_Message message;
  message.sender.name = "Railway Security";
  message.sender.email = SENDER_EMAIL;
  message.subject = "SECURITY ALERT: Metal Disconnected!";
  message.addRecipient("Admin", RECIPIENT_EMAIL);
  
  String htmlMsg = "<div style=\"color:#00466a;font-family:Helvetica,Arial,sans-serif\">";
  htmlMsg += "<h2>Security Alert</h2>";
  htmlMsg += "<p>Metal object has been disconnected from the security sensor!</p>";
  htmlMsg += "<p><strong>System Status:</strong> " + systemStatus + "</p>";
  htmlMsg += "<p><strong>Time:</strong> " + getFormattedTime() + "</p>";
  htmlMsg += "<hr><p>Automated alert from Railway Security System</p></div>";
  
  message.html.content = htmlMsg.c_str();
  message.html.charSet = "utf-8";
  message.text.charSet = "utf-8";
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
  
  // Connect to SMTP server
  if (!smtp.connect(&emailSession)) {
    Serial.println("Email connection failed");
    return;
  }
  
  // Send email
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending email: " + smtp.errorReason());
  } else {
    Serial.println("Email alert sent");
    emailSent = true;
  }
}

String getFormattedTime() {
  unsigned long seconds = millis() / 1000;
  int hours = seconds / 3600;
  int minutes = (seconds % 3600) / 60;
  int secs = seconds % 60;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
  return String(buffer);
}

void loop() {
  unsigned long currentMillis = millis();
  
  server.handleClient();
  handleWiFi(currentMillis);
  
  // Read sensor states
  bool trainAtA = !digitalRead(SENSOR_A);
  bool trainAtB = !digitalRead(SENSOR_B);
  bool trainAtC = !digitalRead(SENSOR_C);
  bool trainAtD = !digitalRead(SENSOR_D);
  
  // Control track LEDs
  digitalWrite(LED_A, trainAtA);
  digitalWrite(LED_B, trainAtB);
  digitalWrite(LED_C, trainAtC);
  digitalWrite(LED_D, trainAtD);
  
  // Update system state
  updateTrainDirection(trainAtA, trainAtB, trainAtC, trainAtD);
  handleGateSystem(trainAtA, trainAtB);
  handleSecuritySensor(currentMillis);
  
  // Sensor health check
  if (currentMillis - lastSensorCheck >= 2000) {
    lastSensorCheck = currentMillis;
    checkSensorHealth();
  }
  
  // Independent servo control
  controlIndependentServo(currentMillis);
  
  // Serial monitor update
  if (currentMillis - lastSerialUpdate >= 500) {
    lastSerialUpdate = currentMillis;
    printSystemStatus(trainAtA, trainAtB, trainAtC, trainAtD, currentMillis);
  }
}

void handleWiFi(unsigned long currentMillis) {
  if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
    wifiConnected = true;
    wifiStatus = "Connected: " + WiFi.localIP().toString();
    Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
    updateSystemStatus("WiFi Connected");
  } 
  else if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected = false;
    wifiStatus = "Disconnected";
    updateSystemStatus("WiFi Lost");
  }
  
  if (!wifiConnected && currentMillis - lastWifiAttempt > WIFI_RETRY_INTERVAL) {
    WiFi.reconnect();
    wifiStatus = "Reconnecting...";
    lastWifiAttempt = currentMillis;
  }
}

void handleSecuritySensor(unsigned long currentMillis) {
  bool metalDetected = !digitalRead(SECURITY_SENSOR);
  
  // Metal NOT detected - Activate alarm
  if (!metalDetected) {
    digitalWrite(SECURITY_LED1, HIGH);
    digitalWrite(SECURITY_LED2, HIGH);
    digitalWrite(SECURITY_BUZZER, HIGH);
    securityAlarmActive = true;
    securityStatus = "ALARM: Metal Disconnected";
    metalDetectedTime = 0;
    
    if (wifiConnected && !emailSent) {
      sendEmailAlert();
    }
    return;
  }
  
  // Metal detected - Start deactivation timer
  if (metalDetected && securityAlarmActive && metalDetectedTime == 0) {
    metalDetectedTime = currentMillis;
    securityStatus = "Recovering...";
    Serial.println(">> Security: Metal detected, starting deactivation timer");
  }
  
  // Deactivate after 2 seconds
  if (metalDetected && securityAlarmActive && metalDetectedTime > 0 && 
      (currentMillis - metalDetectedTime >= ALARM_OFF_DELAY)) {
    digitalWrite(SECURITY_LED1, LOW);
    digitalWrite(SECURITY_LED2, LOW);
    digitalWrite(SECURITY_BUZZER, LOW);
    securityAlarmActive = false;
    securityStatus = "Normal";
    emailSent = false;
    metalDetectedTime = 0;
    Serial.println(">> Security: Alarm deactivated");
  }
}

void controlIndependentServo(unsigned long currentMillis) {
  if (indPauseStart > 0) {
    if (currentMillis - indPauseStart >= IND_PAUSE_DURATION) {
      indPauseStart = 0;
      indServoTarget = (indServoTarget == 0) ? 180 : 0;
    }
    return;
  }
  
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
  
  // Activate gate system
  if (!gateActive && (trainAtA || trainAtB)) {
    gateActive = true;
    trainExitTime = 0;
    activateGateSystem();
    gateStatus = "Closed";
    updateSystemStatus("Train Detected");
    Serial.println(">> Gate: Closing, train detected");
  }
  
  // Handle train departure
  if (gateActive) {
    if (trainAtA || trainAtB) {
      trainExitTime = 0;
    } else {
      if (trainExitTime == 0) {
        trainExitTime = millis();
        gateStatus = "Opening Soon";
        updateSystemStatus("Train Cleared");
        Serial.println(">> Gate: Train cleared, waiting 5s");
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
  gateStatus = "Open";
  trainExitTime = 0;
  openGate();
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GATE_LED_PIN, LOW);
  updateSystemStatus("System Normal");
  Serial.println(">> Gate: Opened");
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
  Serial.println("SENSORS | TRACK LEDs | GATE SYSTEM | SECURITY | IND SERVO");
  Serial.println("A B C D E|A B C D    | STATE       | ALARM    | POS/STATUS");
  Serial.println("-----------------------------------------------------------");
}

void printSystemStatus(bool a, bool b, bool c, bool d, unsigned long currentMillis) {
  // Sensor status
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
    Serial.print("ACTIVE  | ");
    if (trainExitTime > 0) {
      int remaining = 5 - ((currentMillis - trainExitTime) / 1000);
      Serial.print(remaining > 0 ? remaining : 0);
      Serial.print("s     | ");
    } else {
      Serial.print("PRESENT | ");
    }
  } else {
    Serial.print("INACTIVE| --      | ");
  }
  
  // Security alarm status
  if (securityAlarmActive) {
    if (metalDetectedTime > 0) {
      int remaining = (ALARM_OFF_DELAY - (currentMillis - metalDetectedTime)) / 1000;
      Serial.print(remaining > 0 ? remaining : 0);
      Serial.print("s     | ");
    } else {
      Serial.print("ON      | ");
    }
  } else {
    Serial.print("OFF     | ");
  }
  
  // Independent servo status
  if (indPauseStart > 0) {
    Serial.print("PAUSED @ ");
    Serial.print(indServoPos);
    Serial.print("° (");
    Serial.print((IND_PAUSE_DURATION - (currentMillis - indPauseStart)) / 1000);
    Serial.print("s left)");
  } else {
    Serial.print("MOVING: ");
    Serial.print(indServoPos);
    Serial.print("° → ");
    Serial.print(indServoTarget);
    Serial.print("°");
  }
  
  if (sensorError) Serial.print(" | SENSOR ERROR!");
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
        updateSystemStatus("Sensor Error!");
        Serial.print("!! SENSOR ERROR: ");
        Serial.print(char('A' + i));
        Serial.println(" stuck active!");
        return;
      }
    } else {
      sensorActiveTime[i] = 0;
    }
  }
  sensorError = false;
}

void updateSystemStatus(const String& status) {
  systemStatus = status;
  Serial.println(">> STATUS: " + status);
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Railway Control System</title>
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    
    body {
      background: linear-gradient(135deg, #1a2a6c, #b21f1f, #1a2a6c);
      color: #333;
      line-height: 1.6;
      padding: 20px;
      min-height: 100vh;
    }
    
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    
    header {
      text-align: center;
      padding: 20px 0;
      margin-bottom: 30px;
    }
    
    h1 {
      color: white;
      font-size: 2.5rem;
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
      margin-bottom: 10px;
    }
    
    .subtitle {
      color: #e0e0e0;
      font-size: 1.2rem;
    }
    
    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 25px;
      margin-top: 20px;
    }
    
    .card {
      background: rgba(255, 255, 255, 0.95);
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
      padding: 25px;
      transition: transform 0.3s ease, box-shadow 0.3s ease;
    }
    
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 15px 35px rgba(0, 0, 0, 0.4);
    }
    
    .card-title {
      font-size: 1.5rem;
      color: #2c3e50;
      margin-bottom: 20px;
      padding-bottom: 10px;
      border-bottom: 2px solid #3498db;
      display: flex;
      align-items: center;
    }
    
    .card-title i {
      margin-right: 10px;
      font-size: 1.8rem;
    }
    
    .status-item {
      margin-bottom: 15px;
      padding: 12px;
      border-radius: 8px;
      background-color: #f8f9fa;
      box-shadow: 0 2px 5px rgba(0, 0, 0, 0.05);
    }
    
    .status-label {
      font-weight: 600;
      color: #2c3e50;
      margin-bottom: 5px;
      display: flex;
      justify-content: space-between;
    }
    
    .status-value {
      font-size: 1.2rem;
      font-weight: 700;
    }
    
    .status-badge {
      padding: 5px 12px;
      border-radius: 20px;
      font-size: 0.9rem;
      font-weight: 600;
      display: inline-block;
    }
    
    .normal { background-color: #2ecc71; color: white; }
    .warning { background-color: #f39c12; color: white; }
    .alert { background-color: #e74c3c; color: white; }
    .info { background-color: #3498db; color: white; }
    
    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 15px;
      margin-top: 10px;
    }
    
    .sensor-item {
      padding: 15px;
      border-radius: 8px;
      text-align: center;
      background: #f8f9fa;
    }
    
    .sensor-name {
      font-weight: 600;
      margin-bottom: 5px;
    }
    
    .sensor-value {
      font-size: 1.5rem;
      font-weight: 700;
    }
    
    .active { color: #27ae60; }
    .inactive { color: #7f8c8d; }
    
    .uptime {
      text-align: center;
      margin-top: 20px;
      padding: 15px;
      background: rgba(0, 0, 0, 0.2);
      border-radius: 10px;
      color: white;
    }
    
    .last-update {
      text-align: center;
      color: rgba(255, 255, 255, 0.7);
      margin-top: 20px;
      font-size: 0.9rem;
    }
    
    @media (max-width: 768px) {
      .dashboard {
        grid-template-columns: 1fr;
      }
      
      h1 {
        font-size: 2rem;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>Railway Crossing Control System</h1>
      <p class="subtitle">Real-time Monitoring and Control Dashboard</p>
    </header>
    
    <div class="dashboard">
      <!-- System Status Card -->
      <div class="card">
        <h2 class="card-title">System Status</h2>
        <div class="status-item">
          <div class="status-label">Overall Status</div>
          <div class="status-value status-badge )=====";
          
  html += getStatusClass(systemStatus);
  html += R"=====(">)=====";
  html += systemStatus;
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">Uptime</div>
          <div class="status-value">)=====";
  html += getFormattedTime();
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">WiFi Connection</div>
          <div class="status-value">)=====";
  html += wifiStatus;
  html += R"=====(</div>
        </div>
      </div>
      
      <!-- Gate System Card -->
      <div class="card">
        <h2 class="card-title">Gate System</h2>
        <div class="status-item">
          <div class="status-label">Gate Status</div>
          <div class="status-value status-badge )=====";
          
  if (gateStatus == "Closed") html += "alert";
  else if (gateStatus == "Opening Soon") html += "warning";
  else html += "normal";
  
  html += R"=====(">)=====";
  html += gateStatus;
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">Train Direction</div>
          <div class="status-value">)=====";
  html += trainDirection;
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">Buzzer & Light</div>
          <div class="status-value">)=====";
  html += (gateActive || trainExitTime > 0) ? "ACTIVE" : "INACTIVE";
  html += R"=====(</div>
        </div>
      </div>
      
      <!-- Security System Card -->
      <div class="card">
        <h2 class="card-title">Security System</h2>
        <div class="status-item">
          <div class="status-label">Security Status</div>
          <div class="status-value status-badge )=====";
  html += getSecurityClass();
  html += R"=====(">)=====";
  html += securityStatus;
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">Email Alerts</div>
          <div class="status-value">)=====";
  html += emailSent ? "SENT" : "NOT SENT";
  html += R"=====(</div>
        </div>
        
        <div class="status-item">
          <div class="status-label">Alarm State</div>
          <div class="status-value">)=====";
  html += securityAlarmActive ? "ACTIVE" : "INACTIVE";
  html += R"=====(</div>
        </div>
      </div>
      
      <!-- Sensors Card -->
      <div class="card">
        <h2 class="card-title">Sensor Status</h2>
        <div class="sensor-grid">
          <div class="sensor-item">
            <div class="sensor-name">Sensor A</div>
            <div class="sensor-value )=====";
  html += (digitalRead(SENSOR_A) ? "inactive" : "active");
  html += R"=====(">)=====";
  html += digitalRead(SENSOR_A) ? "INACTIVE" : "ACTIVE";
  html += R"=====(</div>
          </div>
          
          <div class="sensor-item">
            <div class="sensor-name">Sensor B</div>
            <div class="sensor-value )=====";
  html += (digitalRead(SENSOR_B) ? "inactive" : "active");
  html += R"=====(">)=====";
  html += digitalRead(SENSOR_B) ? "INACTIVE" : "ACTIVE";
  html += R"=====(</div>
          </div>
          
          <div class="sensor-item">
            <div class="sensor-name">Sensor C</div>
            <div class="sensor-value )=====";
  html += (digitalRead(SENSOR_C) ? "inactive" : "active");
  html += R"=====(">)=====";
  html += digitalRead(SENSOR_C) ? "INACTIVE" : "ACTIVE";
  html += R"=====(</div>
          </div>
          
          <div class="sensor-item">
            <div class="sensor-name">Sensor D</div>
            <div class="sensor-value )=====";
  html += (digitalRead(SENSOR_D) ? "inactive" : "active");
  html += R"=====(">)=====";
  html += digitalRead(SENSOR_D) ? "INACTIVE" : "ACTIVE";
  html += R"=====(</div>
          </div>
          
          <div class="sensor-item">
            <div class="sensor-name">Security Sensor</div>
            <div class="sensor-value )=====";
  html += (digitalRead(SECURITY_SENSOR) ? "inactive" : "active");
  html += R"=====(">)=====";
  html += digitalRead(SECURITY_SENSOR) ? "NORMAL" : "TRIGGERED";
  html += R"=====(</div>
          </div>
        </div>
      </div>
    </div>
    
    <div class="uptime">
      <div class="status-label">Independent Servo Status</div>
      <div class="status-value">)=====";
  
  if (indPauseStart > 0) {
    html += "PAUSED @ ";
    html += String(indServoPos);
    html += "° (";
    html += String((IND_PAUSE_DURATION - (millis() - indPauseStart)) / 1000);
    html += "s left)";
  } else {
    html += "MOVING: ";
    html += String(indServoPos);
    html += "° → ";
    html += String(indServoTarget);
    html += "°";
  }
  
  html += R"=====(</div>
    </div>
    
    <div class="last-update">
      Last updated: )=====";
  html += getFormattedTime();
  html += R"=====( | System will refresh automatically
    </div>
  </div>
  
  <script>
    // Auto-refresh every 5 seconds
    setTimeout(function() {
      location.reload();
    }, 5000);
  </script>
</body>
</html>
)=====";

  server.send(200, "text/html", html);
}

String getStatusClass(const String& status) {
  if (status.indexOf("Error") != -1 || status.indexOf("ALARM") != -1) return "alert";
  if (status.indexOf("Detected") != -1) return "warning";
  return "normal";
}

String getSecurityClass() {
  return (securityStatus == "Normal") ? "normal" : "alert";
}
