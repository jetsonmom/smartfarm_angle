#include <SimpleDHT.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display


// for DHT11, 
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2
int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

void setup() {
  Serial.begin(115200);
   lcd.init();                      // initialize the lcd 
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("hellow smartfarm!");
  delay(1000);
  lcd.clear();
  
}

void loop() {
  lcd_dht11();
}
void lcd_dht11(){
   // start working...
  Serial.println("=================================");
  Serial.println("Sample DHT11...");
  
  // read without samples.
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.print(SimpleDHTErrCode(err));
    Serial.print(","); Serial.println(SimpleDHTErrDuration(err)); delay(1000);
    return;
  }
  
  Serial.print("Sample OK: ");
  Serial.print((int)temperature); Serial.print(" *C, "); 
  Serial.print((int)humidity); Serial.println(" H");
  
  // DHT11 sampling rate is 1HZ.
  delay(1500);
   lcd.setCursor(0,0);
  lcd.print("temp = ");
  lcd.setCursor(8,0);
  lcd.print((int)temperature);
  lcd.write(223);  // 내장된 도 기호 코드 (0xDF)
  lcd.print("C"); 
  lcd.setCursor(0,1);
  lcd.print("humi = ");
  lcd.setCursor(8,1);
  lcd.print((int)humidity);
   lcd.print(" %");  // 습도 단위
}
