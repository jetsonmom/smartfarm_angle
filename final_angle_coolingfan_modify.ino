#include <SimpleDHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD 설정
LiquidCrystal_I2C lcd(0x27, 16, 2);

// DHT11 센서 설정
int dht11Pin = 2;
SimpleDHT11 dht11(dht11Pin);

// 릴레이 핀 설정 - 상수로 변경하여 메모리 절약
const byte INTERNAL_FAN_PIN = 3;       // 내부 쿨링팬 (온도 29도 이상 시 작동)
const byte GROWTH_PUMP_PIN = 4;        // 워터펌프2 (성장용)
const byte GERMINATION_PUMP_PIN = 5;   // 워터펌프1 (발아용)
const byte PLANT_LED_PIN = 6;          // 식물용 LED (오전 5시~저녁 21시)
const byte GERMINATION_FAN_PIN = 7;    // 발아용 쿨링팬
const byte GROWTH_FAN_PIN = 8;         // 성장용 쿨링팬

// 온습도 변수
byte temperature = 0;
byte humidity = 0;

// 온도 기준점 (쿨링팬 작동 기준)
const byte TEMP_THRESHOLD = 29;

// 시간 변수
byte currentHour = 12;
byte currentMinute = 0;
byte previousMinute = 0;

// 디바이스 제어 상태 - 비트 플래그로 통합하여 메모리 절약
byte deviceStatus = 0;
// 비트 위치 정의
#define LED_ON_BIT 0
#define INTERNAL_FAN_ON_BIT 1
#define PUMP_ACTIVE_BIT 2
#define GERMINATION_FAN_ON_BIT 3
#define GROWTH_FAN_ON_BIT 4
#define DEBUG_MODE_BIT 5

// LED 작동 시간 설정
const byte LED_ON_HOUR = 5;   // 오전 5시 켜짐
const byte LED_OFF_HOUR = 21; // 저녁 21시 꺼짐

// 펌프 타이머
unsigned long pumpStartTime = 0;
const unsigned int PUMP_DURATION = 60000; // 1분

// 쿨링팬 타이머
unsigned long fanStartTime = 0;
const unsigned int FAN_DURATION = 300000; // 5분

// 시리얼 입력 처리를 위한 버퍼
char serialBuffer[32]; // 작은 고정 크기 버퍼 사용
byte bufferPos = 0;
bool stringComplete = false;

// 비트 설정/해제/체크 매크로 함수
#define SET_BIT(var, bit) (var |= (1 << bit))
#define CLEAR_BIT(var, bit) (var &= ~(1 << bit))
#define CHECK_BIT(var, bit) (var & (1 << bit))

// 상태 getter/setter 함수
bool ledIsOn() { return CHECK_BIT(deviceStatus, LED_ON_BIT); }
bool internalFanIsOn() { return CHECK_BIT(deviceStatus, INTERNAL_FAN_ON_BIT); }
bool pumpIsActive() { return CHECK_BIT(deviceStatus, PUMP_ACTIVE_BIT); }
bool germinationFanIsOn() { return CHECK_BIT(deviceStatus, GERMINATION_FAN_ON_BIT); }
bool growthFanIsOn() { return CHECK_BIT(deviceStatus, GROWTH_FAN_ON_BIT); }
bool debugMode() { return CHECK_BIT(deviceStatus, DEBUG_MODE_BIT); }

void setLedOn(bool on) { on ? SET_BIT(deviceStatus, LED_ON_BIT) : CLEAR_BIT(deviceStatus, LED_ON_BIT); }
void setInternalFanOn(bool on) { on ? SET_BIT(deviceStatus, INTERNAL_FAN_ON_BIT) : CLEAR_BIT(deviceStatus, INTERNAL_FAN_ON_BIT); }
void setPumpActive(bool active) { active ? SET_BIT(deviceStatus, PUMP_ACTIVE_BIT) : CLEAR_BIT(deviceStatus, PUMP_ACTIVE_BIT); }
void setGerminationFanOn(bool on) { on ? SET_BIT(deviceStatus, GERMINATION_FAN_ON_BIT) : CLEAR_BIT(deviceStatus, GERMINATION_FAN_ON_BIT); }
void setGrowthFanOn(bool on) { on ? SET_BIT(deviceStatus, GROWTH_FAN_ON_BIT) : CLEAR_BIT(deviceStatus, GROWTH_FAN_ON_BIT); }
void setDebugMode(bool debug) { debug ? SET_BIT(deviceStatus, DEBUG_MODE_BIT) : CLEAR_BIT(deviceStatus, DEBUG_MODE_BIT); }

void setup() {
  Serial.begin(115200);
  
  // LCD 초기화
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Farm");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  
  // 릴레이 핀 설정
  pinMode(INTERNAL_FAN_PIN, OUTPUT);
  pinMode(GROWTH_PUMP_PIN, OUTPUT);
  pinMode(GERMINATION_PUMP_PIN, OUTPUT);
  pinMode(PLANT_LED_PIN, OUTPUT);
  pinMode(GERMINATION_FAN_PIN, OUTPUT);
  pinMode(GROWTH_FAN_PIN, OUTPUT);
  
  // 모든 릴레이 초기화
  digitalWrite(INTERNAL_FAN_PIN, LOW);      // 3번 핀 - 꺼짐 (LOW = 꺼짐)
  digitalWrite(GROWTH_PUMP_PIN, LOW);       // 4번 핀 - 꺼짐 (LOW = 꺼짐, 반전)
  digitalWrite(GERMINATION_PUMP_PIN, LOW);  // 5번 핀 - 꺼짐 (LOW = 꺼짐, 반전)
  digitalWrite(PLANT_LED_PIN, HIGH);        // 6번 핀 - 꺼짐 (HIGH = 꺼짐)
  digitalWrite(GERMINATION_FAN_PIN, HIGH);  // 7번 핀 - 꺼짐 (HIGH = 꺼짐)
  digitalWrite(GROWTH_FAN_PIN, HIGH);       // 8번 핀 - 꺼짐 (HIGH = 꺼짐)
  
  // 초기 상태 설정 - 모두 꺼짐
  deviceStatus = 0;
  
  delay(2000);
  lcd.clear();
  
  // 초기 LED 상태 확인
  updateLedState();
  
  // 초기화 완료 메시지
  Serial.println(F("시스템 초기화 완료"));
  Serial.println(F("내부 팬 (핀 3): 꺼짐"));
  Serial.println(F("식물용 LED: 시간 기준으로 확인 중"));
  Serial.println(F("발아용 팬 (핀 7): 꺼짐"));
  Serial.println(F("성장용 팬 (핀 8): 꺼짐"));
}

void loop() {
  // 시리얼 데이터 읽기
  readSerial();
  
  // 온습도 읽기
  readTempAndHumidity();
  
  // 디바이스 제어
  controlDevices();
  
  // LCD 업데이트
  updateLCD();
  
  // 오작동 감지 및 복구
  checkForMalfunctions();
  
  delay(200);
}

// 시리얼 데이터 읽기 함수
void readSerial() {
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    
    if (inChar == '\n') {
      serialBuffer[bufferPos] = '\0';  // null 종료
      stringComplete = true;
      break;
    } else if (bufferPos < sizeof(serialBuffer) - 1) {
      serialBuffer[bufferPos++] = inChar;
    }
  }
  
  if (stringComplete) {
    processInputData();
    bufferPos = 0;
    stringComplete = false;
  }
}

// 입력 데이터 처리
void processInputData() {
  // 시간 데이터 처리
  if (serialBuffer[0] == 'T') {
    // T12:34:56 형식
    if (bufferPos >= 8) {
      int h = (serialBuffer[1] - '0') * 10 + (serialBuffer[2] - '0');
      int m = (serialBuffer[4] - '0') * 10 + (serialBuffer[5] - '0');
      
      if (h >= 0 && h < 24 && m >= 0 && m < 60) {
        currentHour = h;
        currentMinute = m;
        
        Serial.print(F("시간 업데이트: "));
        Serial.print(currentHour);
        Serial.print(F(":"));
        if (currentMinute < 10) Serial.print('0');
        Serial.println(currentMinute);
        
        updateLedState();
      }
    }
  }
  // 디버그 명령어
  else if (strcmp(serialBuffer, "DEBUG") == 0) {
    setDebugMode(true);
    Serial.println(F("디버그 모드 활성화"));
    printDebugInfo();
  }
  // 모든 장치 강제 종료
  else if (strcmp(serialBuffer, "STOP_ALL") == 0) {
    forceAllOff();
    Serial.println(F("모든 장치 강제 종료"));
  }
  // 강제로 펌프 테스트
  else if (strcmp(serialBuffer, "TEST_PUMP") == 0) {
    startPumps();
    Serial.println(F("펌프 테스트 시작 (1분간 작동)"));
  }
  // 강제로 발아용 팬 테스트
  else if (strcmp(serialBuffer, "TEST_GERM_FAN") == 0) {
    digitalWrite(GERMINATION_FAN_PIN, LOW);
    setGerminationFanOn(true);
    fanStartTime = millis();
    Serial.println(F("발아용 팬 테스트 시작 (5분간 작동)"));
  }
  // 강제로 성장용 팬 테스트
  else if (strcmp(serialBuffer, "TEST_GROWTH_FAN") == 0) {
    digitalWrite(GROWTH_FAN_PIN, LOW);
    setGrowthFanOn(true);
    fanStartTime = millis();
    Serial.println(F("성장용 팬 테스트 시작 (5분간 작동)"));
  }
}

// 온습도 읽기
void readTempAndHumidity() {
  byte t = 0;
  byte h = 0;
  
  if (dht11.read(&t, &h, NULL) == SimpleDHTErrSuccess) {
    temperature = t;
    humidity = h;
    
    // 비정상적인 온도 값 필터링
    if (temperature > 50) {
      temperature = 25; // 안전한 기본값
    }
  }
}

// 모든 장치 제어
void controlDevices() {
  // LED 시간에 따른 제어
  updateLedState();
  
  // 내부 쿨링팬 온도에 따른 제어
  controlInternalFan();
  
  // 4시간마다 펌프 시작 (LED가 켜져 있을 때만)
  if (currentMinute == 0 && previousMinute != 0 && 
      ledIsOn() && (currentHour % 4 == 0)) {
    startPumps();
    Serial.print(F("4시간 주기 펌프 작동 - 시간: "));
    Serial.println(currentHour);
  }
  previousMinute = currentMinute;
  
  // 펌프 타이머 확인 (1분 후 자동 종료)
  if (pumpIsActive() && (millis() - pumpStartTime >= PUMP_DURATION)) {
    stopPumps();
    startFans(); // 펌프 종료 후 쿨링팬 시작
  }
  
  // 쿨링팬 타이머 확인 (5분 후 자동 종료)
  if ((germinationFanIsOn() || growthFanIsOn()) && 
      (millis() - fanStartTime >= FAN_DURATION)) {
    stopFans();
  }
}

// 내부 쿨링팬 제어 - 3번 핀
void controlInternalFan() {
  if (temperature >= TEMP_THRESHOLD && !internalFanIsOn()) {
    digitalWrite(INTERNAL_FAN_PIN, HIGH);  // 3번 핀 - HIGH로 켜짐
    setInternalFanOn(true);
    Serial.print(F("내부 팬 켜짐 - 온도: "));
    Serial.print(temperature);
    Serial.println(F("C"));
  } 
  else if (temperature < TEMP_THRESHOLD) {
    // 임계값 이하면 항상 꺼져 있어야 함
    digitalWrite(INTERNAL_FAN_PIN, LOW);  // 3번 핀 - LOW로 꺼짐
    
    // 상태 변수 업데이트
    if (internalFanIsOn()) {
      Serial.print(F("내부 팬 꺼짐 - 온도: "));
      Serial.print(temperature);
      Serial.println(F("C"));
      setInternalFanOn(false);
    }
  }
  
  // 내부 팬이 잘못 켜져 있는지 확인
  if (temperature < TEMP_THRESHOLD && digitalRead(INTERNAL_FAN_PIN) == HIGH) {
    Serial.println(F("오류: 내부 팬이 잘못 켜져 있음. 강제로 끔"));
    digitalWrite(INTERNAL_FAN_PIN, LOW);
    setInternalFanOn(false);
  }
}

// LED 상태 업데이트
void updateLedState() {
  // LED 시간 제어 (오전 5시~저녁 21시)
  bool shouldBeOn = (currentHour >= LED_ON_HOUR && currentHour < LED_OFF_HOUR);
  
  if (shouldBeOn && !ledIsOn()) {
    digitalWrite(PLANT_LED_PIN, LOW);  // 켜기
    setLedOn(true);
    Serial.println(F("LED 켜짐 - 활성 시간 내"));
  } 
  else if (!shouldBeOn && ledIsOn()) {
    digitalWrite(PLANT_LED_PIN, HIGH); // 끄기
    setLedOn(false);
    Serial.println(F("LED 꺼짐 - 활성 시간 외"));
  }
}

// 펌프 시작
void startPumps() {
  // 이미 작동 중이면 무시
  if (pumpIsActive()) return;
  
  digitalWrite(GROWTH_PUMP_PIN, HIGH);      // 4번 핀 - HIGH로 켜짐 (반전)
  digitalWrite(GERMINATION_PUMP_PIN, HIGH); // 5번 핀 - HIGH로 켜짐 (반전)
  
  setPumpActive(true);
  pumpStartTime = millis();
  
  Serial.println(F("펌프 시작 (1분간 작동)"));
}

// 펌프 중지
void stopPumps() {
  digitalWrite(GROWTH_PUMP_PIN, LOW);       // 4번 핀 - LOW로 꺼짐 (반전)
  digitalWrite(GERMINATION_PUMP_PIN, LOW);  // 5번 핀 - LOW로 꺼짐 (반전)
  
  setPumpActive(false);
  
  Serial.println(F("펌프 정지"));
}

// 쿨링팬 시작
void startFans() {
  // LED가 켜져 있을 때만 팬 작동
  if (ledIsOn()) {
    digitalWrite(GERMINATION_FAN_PIN, LOW); // 7번 핀 - LOW로 켜짐
    digitalWrite(GROWTH_FAN_PIN, LOW);      // 8번 핀 - LOW로 켜짐
    
    setGerminationFanOn(true);
    setGrowthFanOn(true);
    fanStartTime = millis();
    
    Serial.println(F("쿨링팬 시작 (5분간 작동)"));
  } else {
    Serial.println(F("LED가 꺼져 있어 쿨링팬을 시작하지 않음"));
  }
}

// 쿨링팬 중지
void stopFans() {
  digitalWrite(GERMINATION_FAN_PIN, HIGH);  // 7번 핀 - HIGH로 꺼짐
  digitalWrite(GROWTH_FAN_PIN, HIGH);       // 8번 핀 - HIGH로 꺼짐
  
  setGerminationFanOn(false);
  setGrowthFanOn(false);
  
  Serial.println(F("쿨링팬 정지"));
}

// LCD 업데이트
void updateLCD() {
  // 첫 번째 줄: 시간과 상태
  lcd.setCursor(0, 0);
  
  // 시간 표시 (항상 두 자리)
  if (currentHour < 10) lcd.print('0');
  lcd.print(currentHour);
  lcd.print(':');
  if (currentMinute < 10) lcd.print('0');
  lcd.print(currentMinute);
  
  // 상태 표시
  lcd.setCursor(6, 0);
  if (pumpIsActive()) {
    lcd.print("WATERING");
  } else if (germinationFanIsOn() || growthFanIsOn()) {
    lcd.print("FAN ON  ");
  } else {
    lcd.print("IDLE    ");
  }
  
  // 두 번째 줄: 온습도와 장치 상태
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature);
  lcd.write(223); // 도 기호
  lcd.print("C ");
  
  lcd.setCursor(7, 1);
  lcd.print("H:");
  lcd.print(humidity);
  lcd.print("%  ");
  
  // 장치 상태 표시
  lcd.setCursor(13, 1);
  lcd.print(ledIsOn() ? "L" : " ");
  lcd.print(internalFanIsOn() ? "I" : " ");
  
  // 두 가지 쿨링팬 상태를 하나로 표시
  if (germinationFanIsOn() || growthFanIsOn()) {
    lcd.print("F");
  } else {
    lcd.print(" ");
  }
}

// 디버그 정보 출력
void printDebugInfo() {
  Serial.println(F("\n--- 시스템 상태 ---"));
  Serial.print(F("시간: "));
  Serial.print(currentHour);
  Serial.print(':');
  if (currentMinute < 10) Serial.print('0');
  Serial.println(currentMinute);
  
  Serial.print(F("온도: "));
  Serial.print(temperature);
  Serial.println(F("C"));
  
  Serial.print(F("습도: "));
  Serial.print(humidity);
  Serial.println(F("%"));
  
  Serial.println(F("\n--- 장치 상태 ---"));
  Serial.print(F("LED: "));
  Serial.println(ledIsOn() ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("내부 팬: "));
  Serial.println(internalFanIsOn() ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("펌프: "));
  Serial.println(pumpIsActive() ? F("작동 중") : F("꺼짐"));
  
  Serial.print(F("발아용 팬: "));
  Serial.println(germinationFanIsOn() ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("성장용 팬: "));
  Serial.println(growthFanIsOn() ? F("켜짐") : F("꺼짐"));
  
  Serial.println(F("--- 핀 상태 ---"));
  Serial.print(F("내부 팬(3): "));
  Serial.println(digitalRead(INTERNAL_FAN_PIN) == HIGH ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("성장 펌프(4): "));
  Serial.println(digitalRead(GROWTH_PUMP_PIN) == HIGH ? F("켜짐") : F("꺼짐"));  // 반전됨
  
  Serial.print(F("발아 펌프(5): "));
  Serial.println(digitalRead(GERMINATION_PUMP_PIN) == HIGH ? F("켜짐") : F("꺼짐"));  // 반전됨
  
  Serial.print(F("LED(6): "));
  Serial.println(digitalRead(PLANT_LED_PIN) == LOW ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("발아용 팬(7): "));
  Serial.println(digitalRead(GERMINATION_FAN_PIN) == LOW ? F("켜짐") : F("꺼짐"));
  
  Serial.print(F("성장용 팬(8): "));
  Serial.println(digitalRead(GROWTH_FAN_PIN) == LOW ? F("켜짐") : F("꺼짐"));
  
  Serial.println(F("------------------------"));
}

// 오작동 감지 및 복구
void checkForMalfunctions() {
  // 3번 핀(내부 팬) 확인 - 온도가 낮은데 작동 중이면 문제
  if (temperature < TEMP_THRESHOLD && digitalRead(INTERNAL_FAN_PIN) == HIGH) {
    Serial.println(F("오작동: 내부 팬 강제 종료"));
    digitalWrite(INTERNAL_FAN_PIN, LOW);
    setInternalFanOn(false);
  }
  
  // LED가 꺼져 있을 때 쿨링팬이 켜져 있으면 강제 종료
  if (!ledIsOn() && (digitalRead(GERMINATION_FAN_PIN) == LOW || digitalRead(GROWTH_FAN_PIN) == LOW)) {
    Serial.println(F("오작동: LED 꺼짐 상태에서 쿨링팬이 켜져 있음. 강제 종료"));
    digitalWrite(GERMINATION_FAN_PIN, HIGH);
    digitalWrite(GROWTH_FAN_PIN, HIGH);
    setGerminationFanOn(false);
    setGrowthFanOn(false);
  }
  
  // 펌프가 작동 중이 아닌데 켜져 있는지 확인 (4번, 5번)
  if (!pumpIsActive()) {
    if (digitalRead(GROWTH_PUMP_PIN) == HIGH) {
      Serial.println(F("오작동: 성장 펌프가 잘못 켜져 있음. 강제 종료"));
      digitalWrite(GROWTH_PUMP_PIN, LOW);
    }
    if (digitalRead(GERMINATION_PUMP_PIN) == HIGH) {
      Serial.println(F("오작동: 발아 펌프가 잘못 켜져 있음. 강제 종료"));
      digitalWrite(GERMINATION_PUMP_PIN, LOW);
    }
  }
}

// 모든 장치 강제 종료
void forceAllOff() {
  digitalWrite(INTERNAL_FAN_PIN, LOW);       // 3번 핀 - LOW로 꺼짐
  digitalWrite(GROWTH_PUMP_PIN, LOW);        // 4번 핀 - LOW로 꺼짐 (반전)
  digitalWrite(GERMINATION_PUMP_PIN, LOW);   // 5번 핀 - LOW로 꺼짐 (반전)
  digitalWrite(PLANT_LED_PIN, HIGH);         // 6번 핀 - HIGH로 꺼짐
  digitalWrite(GERMINATION_FAN_PIN, HIGH);   // 7번 핀 - HIGH로 꺼짐
  digitalWrite(GROWTH_FAN_PIN, HIGH);        // 8번 핀 - HIGH로 꺼짐
  
  // 상태 변수 초기화 - 모두 0으로
  deviceStatus = 0;
  
  // 타이머 초기화
  pumpStartTime = 0;
  fanStartTime = 0;
}
