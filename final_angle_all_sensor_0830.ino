#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SimpleDHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi 설정
const char* ssid = "TP-Link_0744";
const char* password = "75317768";

// 릴레이 핀 설정
const int INTERNAL_FAN_PIN = 7;         // 쿨링팬 (1시간 간격 3분 작동) - 3번에서 7번으로 변경
const int GROWTH_PUMP_PIN = 4;          // 성장용 워터펌프 (1시간 간격 2분20초 작동)
const int GERMINATION_PUMP_PIN = 5;     // 발아용 워터펌프 (1시간 간격 1분 작동)
const int PLANT_LED_PIN = 6;            // 식물용 LED (오전 5시~저녁 21시)

// 센서 핀 설정
const int DHT11_PIN = 2;                // DHT11 온습도 센서

// DHT11 센서 및 LCD 초기화
SimpleDHT11 dht11(DHT11_PIN);
LiquidCrystal_I2C lcd(0x27, 20, 4);     // 20x4 LCD

// 릴레이 제어 상수 (각 핀별로 개별 설정)
// 7번 핀 - 쿨링팬 릴레이
const int PIN7_ON = LOW;                // 7번 핀 켜짐
const int PIN7_OFF = HIGH;              // 7번 핀 꺼짐

// 4번 핀 - 성장용 펌프 릴레이
const int PIN4_ON = HIGH;                // 4번 핀 켜짐
const int PIN4_OFF = LOW;              // 4번 핀 꺼짐

// 5번 핀 - 발아용 펌프 릴레이
const int PIN5_ON = HIGH;                // 5번 핀 켜짐
const int PIN5_OFF = LOW;              // 5번 핀 꺼짐

// 6번 핀 - LED 릴레이
const int PIN6_ON = LOW;                // 6번 핀 켜짐
const int PIN6_OFF = HIGH;              // 6번 핀 꺼짐

// NTP 클라이언트 설정 (한국 시간대 +9시간)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 32400, 60000);

// 타이머 변수
unsigned long pumpStartTime = 0;
bool pumpRunning = false;
int lastMinute = -1;

// 상태 전송 타이머 (제트슨 나노와의 시리얼 통신용)
unsigned long lastStatusSend = 0;

// 센서 및 LCD 업데이트 타이머
unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;

// 센서 데이터 변수
byte temperature = 0;
byte humidity = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("WiFi Plant Monitor 시작");
    
    // LCD 초기화
    lcd.init();
    lcd.backlight();
    lcd.clear();
    
    // 시작 메시지 표시
    lcd.setCursor(0, 0);
    lcd.print("Plant Monitor v1.0");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    
    // 릴레이 핀 초기화
    pinMode(INTERNAL_FAN_PIN, OUTPUT);
    pinMode(GROWTH_PUMP_PIN, OUTPUT);
    pinMode(GERMINATION_PUMP_PIN, OUTPUT);
    pinMode(PLANT_LED_PIN, OUTPUT);
    
    // 초기 상태는 모두 OFF
    digitalWrite(INTERNAL_FAN_PIN, PIN7_OFF);
    digitalWrite(GROWTH_PUMP_PIN, PIN4_OFF);
    digitalWrite(GERMINATION_PUMP_PIN, PIN5_OFF);
    digitalWrite(PLANT_LED_PIN, PIN6_OFF);
    
    setupWiFi();
    
    Serial.println("설정 완료");
    
    // LCD 초기 화면
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ready!");
    delay(1000);
}

void setupWiFi() {
    Serial.print("WiFi 연결 중: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        // 연결 시도 중 모든 릴레이 OFF
        digitalWrite(INTERNAL_FAN_PIN, PIN7_OFF);
        digitalWrite(GROWTH_PUMP_PIN, PIN4_OFF);
        digitalWrite(GERMINATION_PUMP_PIN, PIN5_OFF);
        digitalWrite(PLANT_LED_PIN, PIN6_OFF);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi 연결 성공!");
        Serial.print("IP 주소: ");
        Serial.println(WiFi.localIP());
        
        // NTP 클라이언트 시작
        timeClient.begin();
        Serial.println("NTP 시간 동기화 시작");
        
        // 첫 시간 동기화 강제 실행
        while(!timeClient.update()) {
            timeClient.forceUpdate();
            delay(500);
        }
        
        Serial.println("시간 동기화 완료!");
        
    } else {
        Serial.println();
        Serial.println("WiFi 연결 실패! 재시도 중...");
        delay(5000);
        setupWiFi(); // 재귀 호출로 다시 시도
    }
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi 연결 끊김 - 재연결 시도");
        
        // 안전을 위해 모든 릴레이 OFF
        digitalWrite(INTERNAL_FAN_PIN, PIN7_OFF);
        digitalWrite(GROWTH_PUMP_PIN, PIN4_OFF);
        digitalWrite(GERMINATION_PUMP_PIN, PIN5_OFF);
        digitalWrite(PLANT_LED_PIN, PIN6_OFF);
        
        setupWiFi();
    }
}

void sendStatusToJetson() {
    // 제트슨 나노에 현재 상태 전송 (시리얼 통신용)
    String ledStatus = (digitalRead(PLANT_LED_PIN) == PIN6_ON) ? "ON" : "OFF";
    String fanStatus = (digitalRead(INTERNAL_FAN_PIN) == PIN7_ON) ? "ON" : "OFF";
    String growthPumpStatus = (digitalRead(GROWTH_PUMP_PIN) == PIN4_ON) ? "ON" : "OFF";
    String germPumpStatus = (digitalRead(GERMINATION_PUMP_PIN) == PIN5_ON) ? "ON" : "OFF";
    
    Serial.print("STATUS:");
    Serial.print("LED=" + ledStatus);
    Serial.print(",FAN=" + fanStatus);
    Serial.print(",GROWTH=" + growthPumpStatus);
    Serial.print(",GERM=" + germPumpStatus);
    Serial.print(",TEMP=" + String((int)temperature));
    Serial.println(",HUM=" + String((int)humidity));
}

void readDHT11Sensor() {
    // 2초마다 DHT11 센서 읽기
    if (millis() - lastSensorRead >= 2000) {
        int err = SimpleDHTErrSuccess;
        if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
            Serial.print("DHT11 읽기 실패, err=");
            Serial.println(SimpleDHTErrCode(err));
            // 에러 시 이전 값 유지
        } else {
            Serial.print("DHT11 OK: ");
            Serial.print((int)temperature);
            Serial.print("°C, ");
            Serial.print((int)humidity);
            Serial.println("%");
        }
        lastSensorRead = millis();
    }
}

void updateLCD(int hour, int minute, int second) {
    // 1초마다 LCD 업데이트
    if (millis() - lastLCDUpdate >= 1000) {
        lcd.clear();
        
        // 첫 번째 줄: 시간
        lcd.setCursor(0, 0);
        lcd.print("Time: ");
        if (hour < 10) lcd.print("0");
        lcd.print(hour);
        lcd.print(":");
        if (minute < 10) lcd.print("0");
        lcd.print(minute);
        lcd.print(":");
        if (second < 10) lcd.print("0");
        lcd.print(second);
        
        // 두 번째 줄: 온도
        lcd.setCursor(0, 1);
        lcd.print("Temp: ");
        lcd.print((int)temperature);
        lcd.print("C");
        
        // 세 번째 줄: 습도
        lcd.setCursor(0, 2);
        lcd.print("Humidity: ");
        lcd.print((int)humidity);
        lcd.print("%");
        
        // 네 번째 줄: 상태 표시
        lcd.setCursor(0, 3);
        String status = "";
        if (digitalRead(PLANT_LED_PIN) == PIN6_ON) status += "L";
        if (digitalRead(INTERNAL_FAN_PIN) == PIN7_ON) status += "F";
        if (digitalRead(GROWTH_PUMP_PIN) == PIN4_ON) status += "G";
        if (digitalRead(GERMINATION_PUMP_PIN) == PIN5_ON) status += "S";
        if (status == "") status = "ALL OFF";
        lcd.print("Status: " + status);
        
        lastLCDUpdate = millis();
    }
}

void controlLED(int hour) {
    // 오전 5시부터 오후 9시(21시)까지 LED 켜짐
    static bool ledState = false;
    bool shouldBeOn = (hour >= 5 && hour < 21);
    
    if (shouldBeOn != ledState) {
        if (shouldBeOn) {
            digitalWrite(PLANT_LED_PIN, PIN6_ON);
            Serial.println("LED: ON (주간 모드 5:00-21:00)");
        } else {
            digitalWrite(PLANT_LED_PIN, PIN6_OFF);
            Serial.println("LED: OFF (야간 모드)");
        }
        ledState = shouldBeOn;
    }
}

void controlPumps(int minute) {
    // 디버깅 출력
    static int lastDebugMinute = -1;
    if (minute != lastDebugMinute) {
        Serial.print("펌프 체크 - 현재 분: ");
        Serial.print(minute);
        Serial.print(", 이전 분: ");
        Serial.print(lastMinute);
        Serial.print(", 펌프 실행 중: ");
        Serial.println(pumpRunning ? "예" : "아니오");
        lastDebugMinute = minute;
    }
    
    // 매시 0분에 워터펌프 동작 시작
    if (minute == 0 && lastMinute != 0 && !pumpRunning) {
        Serial.println("=== 워터펌프 사이클 시작 ===");
        
        // 모든 펌프와 팬 동시 시작
        digitalWrite(INTERNAL_FAN_PIN, PIN7_ON);
        digitalWrite(GROWTH_PUMP_PIN, PIN4_ON);
        digitalWrite(GERMINATION_PUMP_PIN, PIN5_ON);
        
        pumpStartTime = millis();
        pumpRunning = true;
        
        Serial.println("쿨링팬 시작 (3분간)");
        Serial.println("성장용 펌프 시작 (2분20초간)");
        Serial.println("발아용 펌프 시작 (1분간)");
    }
    
    // 펌프 동작 시간 제어
    if (pumpRunning) {
        unsigned long elapsed = millis() - pumpStartTime;
        
        // 1분 후 발아용 펌프 OFF
        if (elapsed >= 60000 && digitalRead(GERMINATION_PUMP_PIN) == PIN5_ON) {
            digitalWrite(GERMINATION_PUMP_PIN, PIN5_OFF);
            Serial.println("발아용 펌프 정지 (1분 완료)");
        }
        
        // 2분 20초 후 성장용 펌프 OFF
        if (elapsed >= 140000 && digitalRead(GROWTH_PUMP_PIN) == PIN4_ON) {
            digitalWrite(GROWTH_PUMP_PIN, PIN4_OFF);
            Serial.println("성장용 펌프 정지 (2분20초 완료)");
        }
        
        // 3분 후 쿨링팬 OFF 및 사이클 완료
        if (elapsed >= 180000) {
            digitalWrite(INTERNAL_FAN_PIN, PIN7_OFF);
            Serial.println("쿨링팬 정지 (3분 완료)");
            Serial.println("=== 워터펌프 사이클 종료 ===");
            pumpRunning = false;
        }
    }
    
    lastMinute = minute;
}

void loop() {
    // WiFi 연결 상태 체크 (30초마다)
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck >= 30000) {
        checkWiFi();
        lastWiFiCheck = millis();
    }
    
    // DHT11 센서 읽기
    readDHT11Sensor();
    
    // 시간 업데이트 (WiFi 연결된 경우만)
    if (WiFi.status() == WL_CONNECTED) {
        timeClient.update();
        
        int hour = timeClient.getHours();
        int minute = timeClient.getMinutes();
        int second = timeClient.getSeconds();
        
        // LCD 업데이트
        updateLCD(hour, minute, second);
        
        // 매 10초마다 시간 출력 (디버깅용)
        static unsigned long lastTimeDisplay = 0;
        if (millis() - lastTimeDisplay >= 10000) {
            Serial.print("현재 시간 (한국): ");
            if (hour < 10) Serial.print("0");
            Serial.print(hour);
            Serial.print(":");
            if (minute < 10) Serial.print("0");
            Serial.print(minute);
            Serial.print(":");
            if (second < 10) Serial.print("0");
            Serial.print(second);
            Serial.print(" (lastMinute: ");
            Serial.print(lastMinute);
            Serial.println(")");
            lastTimeDisplay = millis();
        }
        
        // LED 제어
        controlLED(hour);
        
        // 펌프 제어
        controlPumps(minute);
        
    } else {
        // WiFi 미연결시 안전 모드
        digitalWrite(INTERNAL_FAN_PIN, PIN7_OFF);
        digitalWrite(GROWTH_PUMP_PIN, PIN4_OFF);
        digitalWrite(GERMINATION_PUMP_PIN, PIN5_OFF);
        digitalWrite(PLANT_LED_PIN, PIN6_OFF);
        
        Serial.println("WiFi 미연결 - 안전 모드 (모든 릴레이 OFF)");
        
        // WiFi 미연결 시 LCD 표시
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Disconnected");
        lcd.setCursor(0, 1);
        lcd.print("Reconnecting...");
        lcd.setCursor(0, 2);
        lcd.print("Temp: ");
        lcd.print((int)temperature);
        lcd.print("C");
        lcd.setCursor(0, 3);
        lcd.print("Humidity: ");
        lcd.print((int)humidity);
        lcd.print("%");
    }
    
    // 제트슨 나노에 상태 전송 (5초마다)
    if (millis() - lastStatusSend >= 5000) {
        sendStatusToJetson();
        lastStatusSend = millis();
    }
    
    delay(100); // CPU 부하 줄이기
}