#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

// Motor control pins (L298N)
const int IN1 = A7;    // ADC7
const int IN2 = A6;    // ADC6
const int ENA = A3;    // PC3
const int IN3 = A2;    // PC2
const int IN4 = A1;    // PC1
const int ENB = A0;    // PC0

// Button pins
const int BTN_FORWARD = 10;   // PB2
const int BTN_BACKWARD = 6;   // PD6
const int BTN_POWER = 8;   // PB0

// Buzzer pin
const int BUZZER_PIN = 12;    // PB4

// Relay pin
const int RELAY_PIN = 9;      // PB1 (D9)

// Button debounce variables
const int buttonCount = 3; // Thêm nút BTN_POWER
unsigned long lastDebounceTime[3] = {0, 0, 0}; // Mở rộng mảng
int lastButtonState[3] = {HIGH, HIGH, HIGH}; // Mở rộng mảng
const unsigned long debounceDelay = 100;

// Motor variables
int motorSpeed = 200; // PWM speed (0-255)
bool motorDirection = true; // true = CW, false = CCW
bool motorRunning = false;

// Relay variable
bool relayOn = false;

// Motion sound variables
unsigned long lastForwardBeepTime = 0;
const unsigned long forwardBeepInterval = 200; // 200ms tick interval
bool forwardSoundActive = false;
bool backwardSoundActive = false;

// Command variables
String inputCommand = "";
bool commandComplete = false;

void setup() {
  // Initialize I2C bus first
  Wire.begin();
  
  // Motor pins setup
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Button pins setup
  pinMode(BTN_FORWARD, INPUT_PULLUP);
  pinMode(BTN_BACKWARD, INPUT_PULLUP);
  pinMode(BTN_POWER, INPUT_PULLUP); // Thêm khởi tạo cho BTN_POWER

  // Buzzer setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Relay setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay OFF initially
  relayOn = false;

  // Enable motor drivers
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  Serial.begin(9600);
  Serial.println("BLDC Motor Control with Relay Started");

  // OLED initialization with multiple address support
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledAvailable) {
    // Try alternative address
    oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  }

  if (oledAvailable) {
    Serial.println("OLED Initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("BLDC Control"));
    display.display();
    delay(500);
  } else {
    Serial.println("OLED Not Found");
  }

  Serial.println("Commands: h for help");
  updateDisplay();
}

void loop() {
  handleButtons();
  handleMotionSounds();

  // Check for serial commands
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      commandComplete = true;
      break;
    } else {
      inputCommand += c;
    }
  }
  
  // Process command
  if (commandComplete) {
    handleSerialCommand(inputCommand);
    inputCommand = "";
    commandComplete = false;
  }
}

// Hàm mới: Đảo trạng thái relay
void toggleRelay() {
  if (relayOn) {
    turnOffRelay();
  } else {
    turnOnRelay();
  }
}

void handleButtons() {
  int reading[3] = {
    digitalRead(BTN_FORWARD),
    digitalRead(BTN_BACKWARD),
    digitalRead(BTN_POWER)
  };

  for (int i = 0; i < 3; i++) {
    // Chỉ xử lý khi có thay đổi từ HIGH xuống LOW (nhấn nút)
    if (reading[i] == LOW && lastButtonState[i] == HIGH) {
      // Kiểm tra debounce chỉ cho lần nhấn
      if ((millis() - lastDebounceTime[i]) > debounceDelay) {
        Serial.print("Button ");
        Serial.print(i);
        Serial.println(" pressed (after debounce)");
        
        switch (i) {
          case 0: // Forward button
            startMotor(true);
            startForwardSound();
            Serial.println("BTN: Forward CW");
            break;
          case 1: // Backward button
            startMotor(false);
            startBackwardSound();
            Serial.println("BTN: Backward CCW");
            break;
          case 2: // Power button
            Serial.println("Power button case reached!"); // Debug line
            toggleRelay();
            Serial.println("BTN: Toggle Relay");
            break;
        }
        lastDebounceTime[i] = millis();
      }
    }
    
    lastButtonState[i] = reading[i];
  }
}

void handleSerialCommand(String input) {
  input.trim();
  input.toUpperCase();
  
  // Motor commands
  if (input == "START" || input == "F") {
    startMotor(true);
    startForwardSound();
    Serial.println("Motor: CW");
  } else if (input == "STOP" || input == "S") {
    stopMotor();
    stopMotionSounds();
    Serial.println("Motor: STOP");
  } else if (input == "CW") {
    setDirection(true);
    if (motorRunning) startForwardSound();
    Serial.println("Direction: CW");
  } else if (input == "CCW" || input == "B") {
    setDirection(false);
    if (motorRunning) startBackwardSound();
    Serial.println("Direction: CCW");
  } else if (input.startsWith("SPEED ")) {
    int speed = input.substring(6).toInt();
    setSpeed(speed);
  } else if (input.startsWith("V")) {
    int val = input.substring(1).toInt();
    setSpeed(val);
  }
  // Relay commands
  else if (input == "TRG") {
    turnOnRelay();
    Serial.println("Relay: ON");
  } else if (input == "OFF") {
    turnOffRelay();
    Serial.println("Relay: OFF");
  }
  // Status and help
  else if (input == "STATUS") {
    printStatus();
  } else if (input == "H" || input == "HELP") {
    showHelp();
  } else if (input.length() > 0) {
    Serial.println("Unknown command: " + input);
  }

  updateDisplay();
}

// BLDC Motor control functions
void startMotor(bool clockwise) {
  motorRunning = true;
  motorDirection = clockwise;
  
  if (clockwise) {
    // Clockwise rotation
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    // Counter-clockwise rotation
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
  
  // Set speed via PWM
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  
  updateDisplay();
}

void stopMotor() {
  motorRunning = false;
  
  // Turn off all motor pins
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  
  // Double beep when stopping
  for(int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
  
  updateDisplay();
}

void setDirection(bool clockwise) {
  motorDirection = clockwise;
  
  if (motorRunning) {
    // Update direction while running
    if (clockwise) {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
    } else {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
    }
  }
}

void setSpeed(int speed) {
  if (speed >= 0 && speed <= 255) {
    motorSpeed = speed;
    Serial.print("Speed: ");
    Serial.println(motorSpeed);
    
    // Apply new speed if motor is running
    if (motorRunning) {
      analogWrite(ENA, motorSpeed);
      analogWrite(ENB, motorSpeed);
    }
  } else {
    Serial.println("Speed 0-255 only");
  }
  updateDisplay();
}

// Relay control functions
void turnOnRelay() {
  digitalWrite(RELAY_PIN, HIGH);
  relayOn = true;
  
  // Single beep when relay turns on
  digitalWrite(BUZZER_PIN, HIGH);
  delay(150);
  digitalWrite(BUZZER_PIN, LOW);
  
  updateDisplay();
}

void turnOffRelay() {
  digitalWrite(RELAY_PIN, LOW);
  relayOn = false;
  
  // Three quick beeps when relay turns off
  for(int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN, LOW);
    delay(80);
  }
  
  updateDisplay();
}

// Motion sound functions
void startForwardSound() {
  forwardSoundActive = true;
  backwardSoundActive = false;
  lastForwardBeepTime = millis();
}

void startBackwardSound() {
  backwardSoundActive = true;
  forwardSoundActive = false;
  digitalWrite(BUZZER_PIN, HIGH);
}

void stopMotionSounds() {
  forwardSoundActive = false;
  backwardSoundActive = false;
  digitalWrite(BUZZER_PIN, LOW);
}

void handleMotionSounds() {
  if (forwardSoundActive) {
    // Tick sound (non-blocking)
    unsigned long currentTime = millis();
    if (currentTime - lastForwardBeepTime >= forwardBeepInterval) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
      lastForwardBeepTime = currentTime;
    }
  } else if (backwardSoundActive) {
    // Continuous beep (already handled)
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void updateDisplay() {
  if (!oledAvailable) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Line 1: Motor status
  display.setCursor(0, 0);
  display.print("MOTOR: ");
  display.print(motorRunning ? (motorDirection ? "CW " : "CCW") : "STOP");
  display.print(" SPD:");
  display.print(motorSpeed);

  // Line 2: Sound and relay status
  display.setCursor(0, 10);
  display.print("SND: ");
  if (forwardSoundActive) display.print("TICK");
  else if (backwardSoundActive) display.print("BEEP");
  else display.print("OFF");
  
  display.print(" RLY:");
  display.print(relayOn ? "ON" : "OFF");

  // Line 3: System info
  display.setCursor(0, 20);
  display.print("BLDC+RLY v1.1");

  display.display();
}

void printStatus() {
  Serial.println("===== STATUS =====");
  Serial.print("Motor: ");
  Serial.println(motorRunning ? (motorDirection ? "CW" : "CCW") : "STOP");
  Serial.print("Speed: ");
  Serial.println(motorSpeed);
  Serial.print("Sound: ");
  if (forwardSoundActive) Serial.println("TICK");
  else if (backwardSoundActive) Serial.println("BEEP");
  else Serial.println("OFF");
  Serial.print("Relay: ");
  Serial.println(relayOn ? "ON" : "OFF");
  Serial.println("=================");
}

void showHelp() {
  Serial.println("===== HELP =====");
  Serial.println("START/F  - Start CW");
  Serial.println("STOP/S   - Stop motor");
  Serial.println("CW       - Set direction CW");
  Serial.println("CCW/B    - Set direction CCW");
  Serial.println("SPEED XX - Set speed (0-255)");
  Serial.println("VXX      - Set speed (e.g. V150)");
  Serial.println("TRG      - Turn relay ON");
  Serial.println("OFF      - Turn relay OFF");
  Serial.println("STATUS   - System status");
  Serial.println("H        - This help");
  Serial.println("=================");
}