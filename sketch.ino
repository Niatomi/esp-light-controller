#include <ArduinoJson.h>
#include <ESP8266WiFi.h>              
#include <WiFiClient.h>
#include <TimeLib.h>

#define OUTPUT_SIGNAL 5

const char* ssid = "SSID";
const char* password = "PASSWORD";
WiFiClient client;
volatile unsigned long globalTimeBufferMillis = 0;

// Текущий код региона - Самарская область, Самара
String regionID = "51";

// Время восхода
unsigned long SunriseTime;

// Время заката
unsigned long SunsetTime;

// Ожидание следующего ивента
boolean awaitFlag = false;

/*
* инициализируем подключение к WiFi
* Синхронизируем время
*/
void setup() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
        delay(500);
    while (!syncTime())    
        delay(500);
    Serial.begin(9600);

    pinMode(OUTPUT_SIGNAL, OUTPUT);
    digitalWrite(OUTPUT_SIGNAL, LOW);

}

/*
* Синхронизируем время c https://yandex.ru/time/sync.json?geo=51
* @return было ли синхронизировано время или нет
*/
bool syncTime () {                                                    
    if (client.connect("yandex.com", 443)) {                                  
        client.println("GET /time/sync.json?geo=" + regionID + " HTTP/1.1"); 
        client.println("Host: yandex.com"); 
        client.println("Connection: close"); 
        delay(200);                           

        char endOfHeaders[] = "\r\n\r\n";                                   
        if (!client.find(endOfHeaders)) {                                      
            Serial.println("Invalid response");                                   
            return false;                                                         
        }

        const size_t capacity = 768;                                            
        DynamicJsonDocument doc(capacity);                                 

        deserializeJson(doc, client);                    
        client.stop();                                                         

        String StringCurrentTime = doc["time"].as<String>().substring(0,9);   
        String StringSunriseTime =  doc["clocks"][regionID]["sunrise"].as<String>();        
        String StringSunsetTime =  doc["clocks"][regionID]["sunset"].as<String>();                
        doc.clear();                                                     

        unsigned long CurrentTime = toULong(StringCurrentTime);
        SunriseTime = toULong(StringSunriseTime);
        SunsetTime = toULong(StringSunsetTime);
        setTime(CurrentTime);

        return true;
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
    unsigned long CurrentTime = now();
    if ((CurrentTime >= SunsetTime) && (CurrentTime - SunsetTime <= 100)) {
        digitalWrite(OUTPUT_SIGNAL, HIGH);
        awaitFlag = true;
    }

    if ((CurrentTime >= SunriseTime) && (CurrentTime - SunriseTime <= 100)) {
        digitalWrite(OUTPUT_SIGNAL, LOW);
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
