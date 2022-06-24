#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_SPIDevice.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>              
#include <WiFiClient.h>
#include <TimeLib.h>
#include <Time.h>

#define OUTPUT_SIGNAL 5
#define MANUAL_SWITCH_BUTTON 14
#define MANUAL_SYNC_BUTTON 13

const char* ssid = "SSID";
const char* password = "PASSWORD";
WiFiClient client;

// Библеотека для работы с часами реального времени
RTC_DS1307 RTC;

volatile unsigned long globalTimeBufferMillis = 0;

// Текущий код региона - Самарская область, Самара
String regionID = "51";

// Время восхода
unsigned long SunriseTime;

// Время заката
unsigned long SunsetTime;

// Ожидание следующего ивента
boolean awaitFlag = false;

// Переменная ответственная за состояние света
boolean isLightTurnedOn = false;

/*
* Инициализируем подключение к WiFi.
* Инициализируем подключение часов реального времени.
* Синхронизируем время на самом микроконтроллере и на часах реального времени.
* Подключаем прерывание на кнопки ручного переключения света
* и кнопки ручного переключения света.
*/
void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);
    syncTime();
    Serial.begin(9600);
    
    Wire.begin();
    RTC.begin();

    // initializing the rtc
    if(!RTC.begin()) {
        Serial.println("Couldn't find RTC!");
        Serial.flush();
        while (1) delay(10);
    }

    if (! RTC.isrunning()) {
        Serial.println("RTC is NOT running!");
        // Строка ниже используется для настройки даты и времени часов
        // RTC.adjust(DateTime(__DATE__, __TIME__));

    }

    // Прерывание ручного переключения света
    attachInterrupt(digitalPinToInterrupt(MANUAL_SWITCH_BUTTON), manualChange, FALLING);
    
    // Прерывание ручной синхронизации времени
    attachInterrupt(digitalPinToInterrupt(MANUAL_SYNC_BUTTON), syncTime, FALLING);

    // Инициализация сигнала для реле
    pinMode(OUTPUT_SIGNAL, OUTPUT);
    digitalWrite(OUTPUT_SIGNAL, LOW);

}


/*
* Функция по которой инвертируется состояние света
* и применияется это состояние на реле.
*/
void manualChange() {
    isLightTurnedOn = !isLightTurnedOn;
    digitalWrite(OUTPUT_SIGNAL, isLightTurnedOn);
}

/*
* Синхронизируем время c https://yandex.ru/time/sync.json?geo=51
* @return было ли синхронизировано время или нет
*/
void syncTime () {                                                    
    if (client.connect("yandex.com", 443)) {                                  
        client.println("GET /time/sync.json?geo=" + regionID + " HTTP/1.1"); 
        client.println("Host: yandex.com"); 
        client.println("Connection: close"); 
        delay(200);                           

        char endOfHeaders[] = "\r\n\r\n";                                   
        if (!client.find(endOfHeaders)) {                                      
            Serial.println("Invalid response");                                   
            return ;                                                         
        }

        const size_t capacity = 768;                                            
        DynamicJsonDocument doc(capacity);                                 

        deserializeJson(doc, client);                    
        client.stop();                                                         

        String StringCurrentTime = doc["time"].as<String>().substring(0,9);   
        String StringSunriseTime =  doc["clocks"][regionID]["sunrise"].as<String>();        
        String StringSunsetTime =  doc["clocks"][regionID]["sunset"].as<String>();                
        doc.clear();                                                     

        unsigned long currentTime = toULong(StringCurrentTime);
        SunriseTime = toULong(StringSunriseTime);
        SunsetTime = toULong(StringSunsetTime);
        setTime(currentTime);
        RTC.adjust(currentTime); 
    }
}

/*
* В цикле посимвольно переводим строку в unsigned long
* @return преобразованный String в unsigned long
*/
unsigned long toULong(String Str) { 
    unsigned long ULong = 0;
    for (int i = 0; i < Str.length(); i++) {
        char c = Str.charAt(i);
        if (c < '0' || c > '9') break;
        ULong *= 10;
        ULong += (c - '0');
    }
    return ULong;
}

/*
* Вызов и ожидание ивента через промежутки времени
*/
void loop() {
    awaitEvent();
    if (awaitFlag) {
        if (now() > SunriseTime) {
            syncTime();
            improvedDelay(SunsetTime - now());
            awaitFlag = false;
        } else {
            syncTime();
            improvedDelay(SunriseTime - now());
            awaitFlag = false;
        } 
    }
}

/*
* Ивент происходит по времени заката или рассвета
* При рассвете отключается свет
* При закате свет включается
* 
* При любом ивенте флаг на ожидание ставится в true
*/
void awaitEvent() {
    unsigned long CurrentTime = RTC.now().unixtime();
    if ((CurrentTime >= SunsetTime) && (CurrentTime - SunsetTime <= 100)) {
        digitalWrite(OUTPUT_SIGNAL, isLightTurnedOn);
        awaitFlag = true;
    }

    if ((CurrentTime >= SunriseTime) && (CurrentTime - SunriseTime <= 100)) {
        digitalWrite(OUTPUT_SIGNAL, isLightTurnedOn);
        awaitFlag = true;
    }
}

/*
* Улучшенный метод ожидания
* Использовать только его, чтобы не ломать счёт времени 
*/
void improvedDelay(unsigned int waitTime) {
    globalTimeBufferMillis = millis();
    boolean cooldownState = true;

    while (cooldownState) {
        if (millis() - globalTimeBufferMillis > waitTime) 
            cooldownState = false;
    }
}
