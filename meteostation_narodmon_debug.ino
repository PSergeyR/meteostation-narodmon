/*
 * 3 датчика 18b20 на порте d3, питаются от 5 вольт
 * датчик hdc1080 на улице, помимо него на d2 висит еще один 18b20 но пока не используется (не знаю как)
 */
#include <LiquidCrystal_I2C.h>  // подключаем библу
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 
#include <ClosedCube_HDC1080.h>
#include <DallasTemperature.h>
#include <OneWire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // адрес, столбцов, строк
ClosedCube_HDC1080 hdc1080;
const int ledPin = D8; // Нога на которой работает шим для подсветки
#define ONE_WIRE_BUS 0 // Пин подключения OneWire шины, 0 (D3)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress Thermometer;
DeviceAddress temperatureSensors[3]; // Размер массива определяем исходя из количества установленных датчиков
uint8_t deviceCount = 0;


// переменные для хранения данных с датчиков:
float tempSensor1, tempSensor2, tempSensor3, tempSensor4;
uint8_t sensor1[8] = { 0x28, 0xD6, 0x7D, 0x81, 0xE3, 0xE1, 0x3C, 0x30  };
uint8_t sensor2[8] = { 0x28, 0xD3, 0x25, 0x80, 0xE3, 0xE1, 0x3C, 0xFF  };
uint8_t sensor3[8] = { 0x28, 0xFF, 0x64, 0x1E, 0x26, 0x6C, 0x53, 0x00  };

double temp_hdc1080;
double humd_hdc1080;


#define debug true // вывод отладочных сообщений
#define postingInterval  300000 // интервал между отправками данных в миллисекундах (5 минут)



/**/unsigned long lastConnectionTime = 0;           // время последней передачи данных
/**/String Hostname; //имя железки - выглядит как ESPAABBCCDDEEFF т.е. ESP+mac адрес.



void wifimanstart() { // Волшебная процедура начального подключения к Wifi.
                      // Если не знает к чему подцепить - создает точку доступа ESP8266 и настроечную таблицу http://192.168.4.1
                      // Подробнее: https://github.com/tzapu/WiFiManager
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(debug);
  wifiManager.setMinimumSignalQuality();
  if (!wifiManager.autoConnect("ESP8266")) {
  if (debug) Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000); }
if (debug) Serial.println("connected...");
}



 
void setup() 
{
  analogWrite(ledPin, 7);
  lcd.init();
  lcd.backlight();
  hdc1080.begin(0x40);
  sensors.begin();  
  
  Hostname = "ESP"+WiFi.macAddress();
  Hostname.replace(":","");
  Serial.begin(115200);

  
  WiFi.hostname(Hostname);
  wifimanstart();
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
  Serial.print("Narodmon ID: ");
  Serial.println(Hostname); // выдаем в компорт мак и айпишник железки, так же выводим на дисплей

  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0, 1);
  lcd.print(Hostname);
  lastConnectionTime = millis() - postingInterval + 15000; //первая передача на народный мониторинг через 15 сек.
}

void WriteLcdTemp (void){ // заполнение дисплея. происходит каждые 5 минут после получения данных с датчиков
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("G");
  lcd.setCursor(2, 0);
  lcd.print(tempSensor3,1);
  
  lcd.setCursor(10,0);
  lcd.print("S");
  lcd.setCursor(12, 0);
  lcd.print(tempSensor1,1);
  
  
  lcd.setCursor(6, 1);
  lcd.print(temp_hdc1080,1);

}

bool SendToNarodmon() { // Собственно формирование пакета и отправка.
    WiFiClient client;
    String buf;
    buf = "#" + Hostname + "#ELKINA" + "\r\n"; // заголовок И ИМЯ, которое будет отображаться в народном мониторинге, чтоб не палить мак адрес
    sensors.requestTemperatures();
    tempSensor1 = sensors.getTempC(sensor1);
    tempSensor2 = sensors.getTempC(sensor2);
    tempSensor3 = sensors.getTempC(sensor3);
    temp_hdc1080 = hdc1080.readTemperature();
    humd_hdc1080 = hdc1080.readHumidity();

    buf = buf + "#T1#" + String(tempSensor1) + "\r\n";
    buf = buf + "#T2#" + String(tempSensor2) + "\r\n"; 
    buf = buf + "#T3#" + String(tempSensor3) + "\r\n"; 
    buf = buf + "#T4#" + String(temp_hdc1080) + "\r\n";
    buf = buf + "#H1#" + String(humd_hdc1080) + "\r\n";
    buf = buf + "##\r\n"; // закрываем пакет
 
    if (!client.connect("narodmon.ru", 8283)) { // попытка подключения
      Serial.println("connection failed");
      lcd.clear();
      lcd.print("connect failed");
      return false; // не удалось;
    } else
    {
      WriteLcdTemp();
      client.print(buf); // и отправляем данные
      if (debug) Serial.print(buf);
      while (client.available()) {
        String line = client.readStringUntil('\r'); // если что-то в ответ будет - все в Serial
        Serial.print(line);
        
        
        }
    }
      return true; //ушло
      
  }


void loop() 
{

  delay(100);// нужна, беж делея у меня не подключался к вайфаю

 
  if (millis() - lastConnectionTime > postingInterval) {// ждем 5 минут и отправляем
      if (WiFi.status() == WL_CONNECTED) { // ну конечно если подключены
      if (SendToNarodmon()) {
      lastConnectionTime = millis();
      }else{  lastConnectionTime = millis() - postingInterval + 15000; }//следующая попытка через 15 сек    
      }else{  lastConnectionTime = millis() - postingInterval + 15000; Serial.println("Not connected to WiFi"); lcd.clear(); lcd.print("No WiFi");}//следующая попытка через 15 сек
    } yield(); // что за команда - фиг знает, но ESP работает с ней стабильно и не глючит.
}
