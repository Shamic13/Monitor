#include <LiquidCrystal.h>

// Распиновка дисплея LCD Keypad Shield
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Переменные для текущих данных
int cpuUsage = 0; int cpuTemp = 0; int cpuPower = 0;
int gpuUsage = 0; int gpuTemp = 0; int gpuPower = 0;
int ramUsage = 0; float ramUsedGB = 0; float ramTotalGB = 0;
int gpuMemUsage = 0;

// Переменные для МАКСИМАЛЬНЫХ значений
int maxCpuTemp = 0; int maxCpuPower = 0;
int maxGpuTemp = 0; int maxGpuPower = 0;
int maxCpuUsage = 0; int maxGpuUsage = 0;
int maxRamUsage = 0; int maxGpuMemUsage = 0;
float maxRamUsedGB = 0;

int displayMode = 0;   // 0 - CPU/GPU, 1 - RAM
bool autoSwitch = true;
bool isLogging = false; 
bool showMaxValues = false; // Флаг: включен ли режим МАКСИМУМОВ

unsigned long lastModeSwitch = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastSplashUpdate = 0;
unsigned long lastDataReceivedTime = 0; 
bool dataReceived = false;
bool showSplash = true;

const unsigned long BUTTON_INTERVAL = 150; 
const unsigned long SWITCH_INTERVAL = 6000; 
const unsigned long SPLASH_INTERVAL = 400;  
const unsigned long CONN_TIMEOUT = 10000;   

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

byte bar1[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000}; 
byte bar2[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000}; 
byte bar3[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100, B11100}; 
byte bar4[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110, B11110}; 
byte bar5[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111}; 
byte recSymbol[8] = {B00000, B01110, B11111, B11111, B11111, B01110, B00000, B00000}; // Точка ●

// Объявления функций
void updateDisplay(bool forceUpdate);
int readButtons();
void handleButtons(int button);
void showLoadingAnimation();

void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  Serial.setTimeout(50); // Ускоряем таймаут для parseInt, чтобы избежать фризов
  
  lcd.createChar(1, bar1); lcd.createChar(2, bar2); lcd.createChar(3, bar3);
  lcd.createChar(4, bar4); lcd.createChar(5, bar5); lcd.createChar(6, recSymbol);

  lcd.clear();
  lcd.print(F("=== SYSTEM ======"));
  lcd.setCursor(0, 1);
  lcd.print(F("=== MONITOR ====="));
  lastSplashUpdate = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  bool newValuesArrived = false;

  // Парсинг пакета от Python
  if (Serial.available() > 0) {
    if (Serial.read() == '<') { // Ждем маркер начала пакета
      cpuUsage = Serial.parseInt();
      cpuTemp = Serial.parseInt();
      cpuPower = Serial.parseInt();
      gpuUsage = Serial.parseInt();
      gpuTemp = Serial.parseInt();
      gpuPower = Serial.parseInt();
      ramUsage = Serial.parseInt();
      ramUsedGB = Serial.parseFloat();
      ramTotalGB = Serial.parseFloat();
      gpuMemUsage = Serial.parseInt();

      dataReceived = true;
      showSplash = false;  
      lastDataReceivedTime = currentMillis; 
      newValuesArrived = true; 
      
      // Постоянно обновляем пиковые максимумы в фоне
      if (cpuTemp > maxCpuTemp) maxCpuTemp = cpuTemp;
      if (cpuPower > maxCpuPower) maxCpuPower = cpuPower;
      if (cpuUsage > maxCpuUsage) maxCpuUsage = cpuUsage;
      if (gpuTemp > maxGpuTemp) maxGpuTemp = gpuTemp;
      if (gpuPower > maxGpuPower) maxGpuPower = gpuPower;
      if (gpuUsage > maxGpuUsage) maxGpuUsage = gpuUsage;
      if (ramUsage > maxRamUsage) maxRamUsage = ramUsage;
      if (gpuMemUsage > maxGpuMemUsage) maxGpuMemUsage = gpuMemUsage;
      if (ramUsedGB > maxRamUsedGB) maxRamUsedGB = ramUsedGB;
    } else {
      // Очистка буфера от мусора
      while(Serial.available() > 0 && Serial.read() != '\n'); 
    }
  }
  
  if (!showSplash && (currentMillis - lastDataReceivedTime > CONN_TIMEOUT)) {
    showSplash = true;
    dataReceived = false;
    isLogging = false;
    showMaxValues = false;
    lcd.clear();
    lcd.print(F("Waiting for"));
    lcd.setCursor(0, 1);
    lcd.print(F("Python data..."));
  }

  if (showSplash) {
    if (currentMillis - lastSplashUpdate > SPLASH_INTERVAL) {
      showLoadingAnimation();
      lastSplashUpdate = currentMillis;
    }
    if (currentMillis - lastButtonCheck > BUTTON_INTERVAL) {
      int button = readButtons();
      if (button != btnNONE) {
        lcd.clear();
        lcd.print(F("No PC Data Yet"));
        lcd.setCursor(0, 1);
        lcd.print(F("Run Python App"));
        lastDataReceivedTime = currentMillis - CONN_TIMEOUT + 2000; 
      }
      lastButtonCheck = currentMillis;
    }
    return; 
  }
  
  if (currentMillis - lastButtonCheck > BUTTON_INTERVAL) {
    int button = readButtons();
    if (button != btnNONE) {
      handleButtons(button);
      newValuesArrived = true; 
    }
    lastButtonCheck = currentMillis;
  }
  
  // Автопереключение экранов
  if (autoSwitch && !showMaxValues && (currentMillis - lastModeSwitch > SWITCH_INTERVAL)) {
    displayMode = 1 - displayMode; 
    lastModeSwitch = currentMillis;
    newValuesArrived = true;
  }
  
  updateDisplay(newValuesArrived);
}

void showLoadingAnimation() {
  static int animState = 0;
  lcd.setCursor(12, 1);
  if (animState == 0) lcd.print(F(".  "));
  else if (animState == 1) lcd.print(F(".. "));
  else if (animState == 2) lcd.print(F("..."));
  else lcd.print(F("   "));
  animState = (animState + 1) % 4;
}

void handleButtons(int button) {
  switch (button) {
    case btnSELECT:
      isLogging = !isLogging;
      if (isLogging) Serial.println("START_LOG");
      else Serial.println("STOP_LOG");
      break;
      
    case btnLEFT:
      if (!showMaxValues) { // Переключаем авто/ручной режим
        autoSwitch = !autoSwitch;
        lcd.clear();
      }
      break;
      
    case btnUP:
      if (!showMaxValues) {
        displayMode = 1;
        autoSwitch = false;  
        lcd.clear();
      }
      break;
      
    case btnDOWN:
      displayMode = 0;
      if (!showMaxValues) autoSwitch = false;  
      lcd.clear();
      break;

    case btnRIGHT: 
      showMaxValues = !showMaxValues;
      displayMode = 0; // ПРИНУДИТЕЛЬНО переключаем на экран CPU 
      autoSwitch = false; 
      lcd.clear(); 
      break;
  }
}

int readButtons() {
  int buttonValue = analogRead(A0);
  if (buttonValue > 1000) return btnNONE;
  if (buttonValue < 50)  return btnRIGHT;
  if (buttonValue < 200) return btnUP;
  if (buttonValue < 400) return btnDOWN;
  if (buttonValue < 600) return btnLEFT;
  if (buttonValue < 800) return btnSELECT;
  return btnNONE;
}

void updateDisplay(bool forceUpdate) {
  static unsigned long lastFlash = 0;
  static bool toggle = false;
  
  // Мигающая точка при записи логов
  if (millis() - lastFlash > 500) {
    toggle = !toggle;
    lastFlash = millis();
    if (isLogging && !showMaxValues) { 
      lcd.setCursor(15, 0);
      if (toggle) lcd.write(6);
      else lcd.print(F(" "));
    }
  }

  if (!forceUpdate) return;

  if (displayMode == 0) {
    // === ЭКРАН 0: CPU и GPU ===
    lcd.setCursor(0, 0);
    if (showMaxValues) {
      lcd.print(F("MAX:")); lcd.print(maxCpuTemp); lcd.print(F("C ")); lcd.print(maxCpuPower); lcd.print(F("W ")); lcd.print(maxCpuUsage); lcd.print(F("%   "));
    } else {
      lcd.print(F("CPU:")); lcd.print(cpuTemp); lcd.print(F("C ")); lcd.print(cpuPower); lcd.print(F("W ")); lcd.print(cpuUsage); lcd.print(F("%   "));
    }

    lcd.setCursor(0, 1);
    if (showMaxValues) {
      lcd.print(F("MAX:")); lcd.print(maxGpuTemp); lcd.print(F("C ")); lcd.print(maxGpuPower); lcd.print(F("W ")); lcd.print(maxGpuUsage); lcd.print(F("%   "));
    } else {
      lcd.print(F("GPU:")); lcd.print(gpuTemp); lcd.print(F("C ")); lcd.print(gpuPower); lcd.print(F("W ")); lcd.print(gpuUsage); lcd.print(F("%   "));
    }
    
  } else if (displayMode == 1) {
    // === ЭКРАН 1: ОПЕРАТИВНАЯ ПАМЯТЬ ===
    lcd.setCursor(0, 0);
    if (showMaxValues) {
      lcd.print(F("RAM:")); lcd.print(maxRamUsage); lcd.print(F("% VR:")); lcd.print(maxGpuMemUsage); lcd.print(F("%  MAX"));
    } else {
      lcd.print(F("RAM:")); lcd.print(ramUsage); lcd.print(F("% VR:")); lcd.print(gpuMemUsage); lcd.print(F("%     "));
      lcd.setCursor(15, 0); lcd.print(autoSwitch ? 'A' : 'M');
    }

    lcd.setCursor(0, 1);
    if (showMaxValues) {
      lcd.print(F("RAM:")); lcd.print(maxRamUsedGB, 0); lcd.print(F("/")); lcd.print(ramTotalGB, 0); lcd.print(F("G       "));
    } else {
      lcd.print(F("RAM:")); lcd.print(ramUsedGB, 0); lcd.print(F("/")); lcd.print(ramTotalGB, 0); lcd.print(F("G       "));
    }
  }
}
