#include <LiquidCrystal.h>

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Данные
int cpuUsage = 0;
int cpuTemp = 0; 
int cpuPower = 0;
int gpuUsage = 0;
int gpuTemp = 0;
int gpuPower = 0;
int ramUsage = 0;
float ramUsedGB = 0;
float ramTotalGB = 0;
int gpuMemUsage = 0;

// Переменные для управления
int displayMode = 0;  // 0=CPU/GPU, 1=RAM/System
bool autoSwitch = true;
unsigned long lastModeSwitch = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastSplashUpdate = 0;
bool dataReceived = false;
bool showSplash = true;
int splashState = 0;

// Кнопки
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Объявления функций
void parseData(String data);
void parseCPUData(String data);
void parseGPUData(String data);
void parseRAMData(String data);
void updateDisplay();
void displayCPU_GPU();
void displayRAM_System();
void printPadded(int value, int digits);
void printFloatPadded(float value, int totalWidth, int decimals);
int readButtons();
void handleButtons(int button);
void showSplashScreen();
void showLoadingAnimation();

void setup() {
  lcd.begin(16, 2);
  Serial.begin(9600);
  showSplashScreen();
}

void loop() {
  // Чтение данных от Python
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    parseData(data);
    dataReceived = true;
    showSplash = false;  // Получили данные - убираем заставку
  }
  
  // Показываем заставку если нет данных
  if (showSplash) {
    if (millis() - lastSplashUpdate > 800) {
      showLoadingAnimation();
      lastSplashUpdate = millis();
    }
    
    // Проверка кнопок на заставке
    if (millis() - lastButtonCheck > 200) {
      int button = readButtons();
      if (button != btnNONE) {
        lcd.clear();
        lcd.print("Waiting data...");
        lcd.setCursor(0, 1);
        lcd.print("Start Python app");
        delay(2000);
        showSplashScreen();
      }
      lastButtonCheck = millis();
    }
    return;  // Не выполняем основной код пока на заставке
  }
  
  // Основной код (когда есть данные)
  
  // Проверка кнопок каждые 200ms
  if (millis() - lastButtonCheck > 200) {
    int button = readButtons();
    if (button != btnNONE) {
      handleButtons(button);
    }
    lastButtonCheck = millis();
  }
  
  // Автопереключение режимов ТОЛЬКО если авторежим включен
  if (autoSwitch && millis() - lastModeSwitch > 6000) {
    displayMode = 1 - displayMode;
    lastModeSwitch = millis();
  }
  
  // Обновление дисплея
  updateDisplay();
  delay(100);
}

void showSplashScreen() {
  lcd.clear();
  lcd.print("=== SYSTEM ======");
  lcd.setCursor(0, 1);
  lcd.print("=== MONITOR =====");
  delay(1500);
  
  lcd.clear();
  lcd.print(" PC Performance ");
  lcd.setCursor(0, 1);
  lcd.print("    Monitor     ");
  delay(1500);
  
  lcd.clear();
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("Python data...");
}

void showLoadingAnimation() {
  static int animState = 0;
  String dots[] = {".  ", ".. ", "...", "   "};
  
  lcd.setCursor(11, 1);
  lcd.print(dots[animState]);
  
  animState = (animState + 1) % 4;
  
  // Мигание надписи
  if (animState % 2 == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Waiting for  ");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("WAITING FOR  ");
  }
}

void handleButtons(int button) {
  switch (button) {
    case btnSELECT:
      // SELECT - переключение авторежима/ручного режима
      autoSwitch = !autoSwitch;
      lcd.clear();
      lcd.print("Auto mod:");
      lcd.setCursor(0, 1);
      lcd.print(autoSwitch ? "ON" : "OFF");
      delay(1500);
      break;
      
    case btnUP:
      // UP - переключить на RAM/System
      displayMode = 1;
      autoSwitch = false;  // При ручном переключении выключаем авторежим
      lastModeSwitch = millis();
      lcd.clear();
      lcd.print("RAM/System");
      lcd.setCursor(0, 1);
      lcd.print("Manual control");
      delay(1000);
      break;
      
    case btnDOWN:
      // DOWN - переключить на CPU/GPU
      displayMode = 0;
      autoSwitch = false;  // При ручном переключении выключаем авторежим
      lastModeSwitch = millis();
      lcd.clear();
      lcd.print("CPU/GPU");
      lcd.setCursor(0, 1);
      lcd.print("Manual control");
      delay(1000);
      break;
      
    case btnRIGHT:
      // RIGHT - показать информацию о системе
      lcd.clear();
      lcd.print("System Monitor");
      lcd.setCursor(0, 1);
      lcd.print("v1.0 LCD Keypad");
      delay(2000);
      break;
      
    case btnLEFT:
      // LEFT - показать статус подключения (компактно)
      lcd.clear();
      lcd.print("Data: ");
      lcd.print(dataReceived ? "OK" : "No data");
      lcd.setCursor(0, 1);
      lcd.print("PC: ");
      lcd.print(dataReceived ? "CONNECT" : "WAIT");
      delay(2000);
      break;
  }
}

int readButtons() {
  int buttonValue = analogRead(A0);
  
  // Более точные диапазоны для LCD Keypad Shield
  if (buttonValue > 1000) return btnNONE;
  if (buttonValue < 50)   return btnRIGHT;
  if (buttonValue < 200)  return btnUP;
  if (buttonValue < 400)  return btnDOWN;
  if (buttonValue < 600)  return btnLEFT;
  if (buttonValue < 800)  return btnSELECT;
  
  return btnNONE;
}

void parseData(String data) {
  int cpuStart = data.indexOf("CPU:");
  int gpuStart = data.indexOf("GPU:");
  int ramStart = data.indexOf("RAM:");
  int gpuMemStart = data.indexOf("GPUMEM:");
  
  if (cpuStart != -1) {
    int cpuEnd = data.indexOf(",GPU:");
    if (cpuEnd == -1) cpuEnd = data.length();
    String cpuData = data.substring(cpuStart + 4, cpuEnd);
    parseCPUData(cpuData);
  }
  
  if (gpuStart != -1) {
    int gpuEnd = data.indexOf(",RAM:");
    if (gpuEnd == -1) gpuEnd = data.length();
    String gpuData = data.substring(gpuStart + 4, gpuEnd);
    parseGPUData(gpuData);
  }
  
  if (ramStart != -1) {
    int ramEnd = data.indexOf(",GPUMEM:");
    if (ramEnd == -1) ramEnd = data.length();
    String ramData = data.substring(ramStart + 4, ramEnd);
    parseRAMData(ramData);
  }
  
  if (gpuMemStart != -1) {
    String gpuMemData = data.substring(gpuMemStart + 7);
    gpuMemUsage = gpuMemData.toInt();
  }
  
  dataReceived = true;
}

void parseCPUData(String data) {
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  
  if (firstComma != -1 && secondComma != -1) {
    cpuUsage = data.substring(0, firstComma).toInt();
    cpuTemp = data.substring(firstComma + 1, secondComma).toInt();
    cpuPower = data.substring(secondComma + 1).toInt();
  }
}

void parseGPUData(String data) {
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  
  if (firstComma != -1 && secondComma != -1) {
    gpuUsage = data.substring(0, firstComma).toInt();
    gpuTemp = data.substring(firstComma + 1, secondComma).toInt();
    gpuPower = data.substring(secondComma + 1).toInt();
  }
}

void parseRAMData(String data) {
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  
  if (firstComma != -1 && secondComma != -1) {
    ramUsage = data.substring(0, firstComma).toInt();
    ramUsedGB = data.substring(firstComma + 1, secondComma).toFloat();
    ramTotalGB = data.substring(secondComma + 1).toFloat();
  }
}

void updateDisplay() {
  lcd.clear();
  
  if (displayMode == 0) {
    displayCPU_GPU();
  } else {
    displayRAM_System();
  }
}

void displayCPU_GPU() {
  // РЕЖИМ 1: Только CPU и GPU с мощностью
  lcd.setCursor(0, 0);
  lcd.print("CPU:");
  printPadded(cpuUsage, 2);
  lcd.print("% ");
  printPadded(cpuTemp, 2);
  lcd.print("C ");
  printPadded(cpuPower, 3);
  lcd.print("W");
  
  lcd.setCursor(0, 1);
  lcd.print("GPU:");
  if (gpuUsage == 100) {
    lcd.print("99% ");
  } else {
    printPadded(gpuUsage, 2);
    lcd.print("% ");
  }
  printPadded(gpuTemp, 2);
  lcd.print("C ");
  printPadded(gpuPower, 3);
  lcd.print("W");
}

void displayRAM_System() {
  // РЕЖИМ 2: RAM и системная информация
  lcd.setCursor(0, 0);
  lcd.print("RAM:");
  printPadded(ramUsage, 2);
  lcd.print("% GPUM:");
  printPadded(gpuMemUsage, 2);
  lcd.print("%");
  
  lcd.setCursor(0, 1);
  lcd.print("USE:");
  printFloatPadded(ramUsedGB, 4, 1);
  lcd.print("/");
  printFloatPadded(ramTotalGB, 4, 1);
  lcd.print("G");
}

void printPadded(int value, int digits) {
  if (digits == 2) {
    if (value < 10) lcd.print("0");
    lcd.print(value);
  } else if (digits == 3) {
    if (value < 10) lcd.print("00");
    else if (value < 100) lcd.print("0");
    lcd.print(value);
  }
}

void printFloatPadded(float value, int totalWidth, int decimals) {
  String str = String(value, decimals);
  while (str.length() < totalWidth) str = " " + str;
  lcd.print(str);
}
