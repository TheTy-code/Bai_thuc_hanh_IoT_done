#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <DHT.h>

// --- KHAI BÁO CHÂN GPIO ---
#define DHTPIN 4
#define DHTTYPE DHT11

#define RAIN_PIN 34
#define LDR_PIN 35
#define BTN_CTRL_PIN 19    // Nút điều khiển duy nhất
#define LED_AUTO_PIN 12
#define LED_MAN_PIN 14
#define SERVO_PIN 13

// --- ĐỊNH NGHĨA LỆNH CHO SERVO 360 ĐỘ (dùng writeMicroseconds) ---
// ĐẢO CHIỀU: hoán đổi giá trị CW và CCW
#define SERVO_STOP      1500   // Dừng (xung trung tâm)
#define SERVO_CW        1000   // Quay thuận (mở rèm) – đã đổi từ 2000 → 1000
#define SERVO_CCW       2000   // Quay ngược (đóng rèm) – đã đổi từ 1000 → 2000
#define ONE_FULL_TURN_MS  1000  // Thời gian quay 1 vòng (ms) – cần chỉnh theo thực tế

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo curtainServo;
DHT dht(DHTPIN, DHTTYPE);

bool isAutoMode = true;
bool isCurtainOpen = false;

// Biến quản lý nút bấm
bool lastBtnCtrlState = HIGH;
unsigned long btnCtrlPressTime = 0;
bool btnCtrlLongPressHandled = false;

// Biến đọc cảm biến và thời gian
unsigned long lastReadTime = 0;
float temperature = 0.0;
float humidity = 0.0;
bool isRaining = false;
bool isBright = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n==========================================");
  Serial.println("   HE THONG REM TU DONG ESP32 KHOI DONG   ");
  Serial.println("==========================================");

  pinMode(LED_AUTO_PIN, OUTPUT);
  pinMode(LED_MAN_PIN, OUTPUT);
  pinMode(BTN_CTRL_PIN, INPUT_PULLUP);
  pinMode(RAIN_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(DHTPIN, INPUT_PULLUP);

  // Cấu hình Servo – dừng ngay khi khởi động
  ESP32PWM::allocateTimer(0);
  curtainServo.setPeriodHertz(50);
  curtainServo.attach(SERVO_PIN, 500, 2400);
  curtainServo.writeMicroseconds(SERVO_STOP);
  delay(100);
  curtainServo.detach();

  isCurtainOpen = false;

  dht.begin();
  lcd.init();
  lcd.backlight();

  updateLEDs();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 Curtain");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1500);
  lcd.clear();
}

void loop() {
  checkButton();

  if (millis() - lastReadTime >= 500) {
    lastReadTime = millis();

    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    isRaining = (digitalRead(RAIN_PIN) == LOW);
    isBright  = (digitalRead(LDR_PIN) == LOW);

    if (isAutoMode) {
      if (isRaining) {
        setCurtain(false);
      } else if (isBright) {
        setCurtain(true);
      } else {
        setCurtain(false);
      }
    }

    updateLCD();
    printToSerial();
  }

  delay(20);
}

// ------------------ XỬ LÝ NÚT NHẤN ------------------
void checkButton() {
  bool currentBtnCtrl = digitalRead(BTN_CTRL_PIN);

  if (lastBtnCtrlState == HIGH && currentBtnCtrl == LOW) {
    btnCtrlPressTime = millis();
    btnCtrlLongPressHandled = false;
  }

  if (currentBtnCtrl == LOW && !btnCtrlLongPressHandled) {
    if (millis() - btnCtrlPressTime >= 1000) {
      toggleMode();
      btnCtrlLongPressHandled = true;
    }
  }

  if (lastBtnCtrlState == LOW && currentBtnCtrl == HIGH) {
    if (!btnCtrlLongPressHandled && (millis() - btnCtrlPressTime < 1000)) {
      if (!isAutoMode) {
        setCurtain(!isCurtainOpen);
        updateLCD();
        Serial.println(">> [SU KIEN] Bấm ngắn nút 19: đóng/mở rèm thủ công.");
      } else {
        Serial.println(">> [THÔNG BÁO] Bấm ngắn không có tác dụng khi ở chế độ AUTO.");
      }
    }
  }

  lastBtnCtrlState = currentBtnCtrl;
}

// ------------------ CÁC HÀM CHỨC NĂNG ------------------
void toggleMode() {
  isAutoMode = !isAutoMode;
  updateLEDs();
  updateLCD();
  Serial.print(">> [SU KIEN] Giữ nút 1s -> Chuyển sang chế độ: ");
  Serial.println(isAutoMode ? "Auto" : "Manual");
}

void setCurtain(bool openState) {
  if (isCurtainOpen != openState) {
    isCurtainOpen = openState;

    curtainServo.attach(SERVO_PIN, 500, 2400);

    if (isCurtainOpen) {
      Serial.println(">> [SERVO 360] Quay 1 vòng thuận chiều để MỞ rèm...");
      curtainServo.writeMicroseconds(SERVO_CW);
      delay(ONE_FULL_TURN_MS);
    } else {
      Serial.println(">> [SERVO 360] Quay 1 vòng ngược chiều để ĐÓNG rèm...");
      curtainServo.writeMicroseconds(SERVO_CCW);
      delay(ONE_FULL_TURN_MS);
    }

    curtainServo.writeMicroseconds(SERVO_STOP);
    delay(50);
    curtainServo.detach();

    Serial.println(">> [SERVO 360] Đã dừng hoàn toàn.");
  }
}

void updateLEDs() {
  digitalWrite(LED_AUTO_PIN, isAutoMode ? HIGH : LOW);
  digitalWrite(LED_MAN_PIN, isAutoMode ? LOW : HIGH);
}

void updateLCD() {
  lcd.setCursor(0, 0);
  if (isnan(temperature) || isnan(humidity)) {
    lcd.print("DHT Error       ");
  } else {
    lcd.print("T:");
    lcd.print((int)temperature);
    lcd.print((char)223);
    lcd.print("C H:");
    lcd.print((int)humidity);
    lcd.print("%   ");
  }

  lcd.setCursor(0, 1);
  lcd.print(isAutoMode ? "A| " : "M| ");
  lcd.print(isCurtainOpen ? "OPEN " : "CLOSE");
}

void printToSerial() {
  Serial.print("[THONG SO 5S] ");

  Serial.print("Nhiet do: ");
  if (isnan(temperature)) Serial.print("LOI | ");
  else { Serial.print(temperature, 1); Serial.print(" C | "); }

  Serial.print("Do am: ");
  if (isnan(humidity)) Serial.print("LOI | ");
  else { Serial.print(humidity, 1); Serial.print(" % | "); }

  Serial.print("Mua: ");
  Serial.print(isRaining ? "CO MUA | " : "KHONG | ");

  Serial.print("Anh sang: ");
  Serial.print(isBright ? "TROI SANG | " : "TROI TOI | ");

  Serial.print("Che do: ");
  Serial.print(isAutoMode ? "AUTO" : "MANUAL");
  Serial.print(" | ");

  Serial.print("Rem: ");
  Serial.println(isCurtainOpen ? "MO (OPEN)" : "DONG (CLOSE)");
}