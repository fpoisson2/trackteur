#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <stdio.h>   // snprintf
#include <string.h>  // strstr, memset, strncpy, strchr, sscanf
#include <stdlib.h>  // dtostrf, atof
#include <ctype.h>   // isprint

// --- Définition des Pins
const uint8_t powerPin = 2;
const uint8_t swRxPin  = 3;
const uint8_t swTxPin  = 4;
const uint8_t SD_CS    = 8;

// --- SoftwareSerial pour le module LTE
SoftwareSerial moduleSerial(swRxPin, swTxPin);

// --- Baud Rate
const unsigned long moduleBaudRate = 9600UL;

// --- APN
const char* APN = "em";

// --- Traccar
const char* TRACCAR_HOST = "trackteur.ve2fpd.com";
const uint16_t TRACCAR_PORT = 5055;
const char* DEVICE_ID = "212910";

// --- Timing
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 60000UL; // 60 s

// --- Buffers
#define RESPONSE_BUFFER_SIZE 64
static char responseBuffer[RESPONSE_BUFFER_SIZE];
static uint8_t responseBufferPos = 0;

#define CMD_BUFFER_SIZE 32
static char cmdBuffer[CMD_BUFFER_SIZE];

#define HTTP_REQUEST_BUFFER_SIZE 128
static char httpRequestBuffer[HTTP_REQUEST_BUFFER_SIZE];

// --- GPS
float currentLat = 0.0f;
float currentLon = 0.0f;
#define GPS_TIMESTAMP_BUF_SIZE 25
static char gpsTimestampTraccar[GPS_TIMESTAMP_BUF_SIZE];

// --- Flags
bool setupSuccess = false;
bool sdAvailable  = false;

// --- Prototypes SD
void storeDataOffline(float lat, float lon, const char* timestamp);
void replayOfflineData();

// --- Fonctions AT/GNSS/Traccar (identiques) ---
void readSerialResponse(unsigned long waitMillis) {
  unsigned long start = millis();
  memset(responseBuffer, 0, RESPONSE_BUFFER_SIZE);
  responseBufferPos = 0;
  bool any = false;
  while (millis() - start < waitMillis) {
    while (moduleSerial.available()) {
      any = true;
      if (responseBufferPos < RESPONSE_BUFFER_SIZE - 1) {
        char c = moduleSerial.read();
        if (c && isprint(c)) responseBuffer[responseBufferPos++] = c;
      } else moduleSerial.read();
    }
    if (!moduleSerial.available()) delay(5);
  }
  responseBuffer[responseBufferPos] = '\0';
  if (responseBufferPos) {
    Serial.print(F("Rcvd: ["));
    for (uint8_t i=0;i<responseBufferPos;i++){
      char c=responseBuffer[i];
      if (c=='\r') continue;
      else if (c=='\n') Serial.print(F("<LF>"));
      else Serial.print(c);
    }
    Serial.println(F("]"));
  } else if (any) {
    Serial.println(F("Rcvd: [Empty]"));
  }
}

bool executeSimpleCommand(const char* cmd, const char* expect,
                          unsigned long timeout, uint8_t retries) {
  for (uint8_t i=0; i<retries; i++) {
    Serial.print(F("Send: ")); Serial.println(cmd);
    moduleSerial.println(cmd);
    readSerialResponse(timeout);
    if (strstr(responseBuffer, expect)) {
      Serial.println(F(">> OK"));
      return true;
    }
    delay(200);
  }
  Serial.println(F(">> FAILED"));
  return false;
}

bool waitForInitialOK(uint8_t tries) {
  for (uint8_t i=0; i<tries; i++) {
    moduleSerial.println(F("AT"));
    readSerialResponse(500);
    if (strstr(responseBuffer, "OK")) return true;
    delay(200);
  }
  return false;
}

bool getGpsData(float &lat, float &lon, char* tsOut) {
  moduleSerial.println(F("AT+CGNSINF"));
  readSerialResponse(2000);
  char* r = strstr(responseBuffer, "+CGNSINF:");
  if (!r) return false;
  char* p = r+10;
  uint8_t run, fix;
  if (sscanf(p, "%hhu,%hhu", &run, &fix)<2 || run!=1 || fix!=1) return false;

  // timestamp
  char tmpTS[20]={0}, tmpF[16]={0};
  char* q=p;
  for (uint8_t i=0;i<3;i++){
    q=strchr(q, ','); if(!q) return false; q++;
  }
  char* e=strchr(q, ','); if(!e) return false;
  uint8_t L=e-q; if(L>=sizeof(tmpTS))L=sizeof(tmpTS)-1;
  strncpy(tmpTS,q,L); tmpTS[L]=0;

  // lat
  q=e+1; e=strchr(q, ','); if(!e) return false;
  L=e-q; if(L>=sizeof(tmpF))L=sizeof(tmpF)-1;
  strncpy(tmpF,q,L); tmpF[L]=0; lat=atof(tmpF);

  // lon
  q=e+1; e=strchr(q, ','); if(!e) return false;
  L=e-q; if(L>=sizeof(tmpF))L=sizeof(tmpF)-1;
  strncpy(tmpF,q,L); tmpF[L]=0; lon=atof(tmpF);

  // format URL timestamp
  int Y,Mo,D,h,mi,s;
  if (sscanf(tmpTS,"%4d%2d%2d%2d%2d%2d",&Y,&Mo,&D,&h,&mi,&s)<6) return false;
  snprintf(tsOut, GPS_TIMESTAMP_BUF_SIZE,
           "%04d-%02d-%02d%%20%02d:%02d:%02d",
           Y,Mo,D,h,mi,s);
  return true;
}

bool sendGpsToTraccar(const char* host, uint16_t port,
                      const char* devId, float lat, float lon,
                      const char* ts) {
  bool conn=false, sent=false;
  snprintf(cmdBuffer,CMD_BUFFER_SIZE,
           "AT+CIPSTART=\"TCP\",\"%s\",%u",host,port);
  if (executeSimpleCommand(cmdBuffer,"OK",10000,1)) {
    readSerialResponse(500);
    if (strstr(responseBuffer,"CONNECT OK")||
        strstr(responseBuffer,"ALREADY CONNECT")) {
      conn=true;
      Serial.println(F("Connected"));
    }
  }
  if (conn) {
    char lats[15],lons[15];
    dtostrf(lat,4,6,lats);
    dtostrf(lon,4,6,lons);
    snprintf(httpRequestBuffer,HTTP_REQUEST_BUFFER_SIZE,
             "GET /?id=%s&lat=%s&lon=%s&timestamp=%s HTTP/1.1\r\n"
             "Host: %s\r\n\r\n",
             devId,lats,lons,ts,host);
    int L=strlen(httpRequestBuffer);
    snprintf(cmdBuffer,CMD_BUFFER_SIZE,"AT+CIPSEND=%d",L);
    moduleSerial.println(cmdBuffer);
    readSerialResponse(500);
    if (strchr(responseBuffer,'>')) {
      moduleSerial.print(httpRequestBuffer);
      readSerialResponse(5000);
      if (strstr(responseBuffer,"SEND OK")) {
        sent=true;
        Serial.println(F("SEND OK"));
      }
    }
  }
  if (conn) executeSimpleCommand("AT+CIPCLOSE=0","CLOSE OK",500,1);
  else     executeSimpleCommand("AT+CIPSHUT","SHUT OK",5000,1);
  delay(500);
  return conn && sent;
}

// --- Stockage Offline ---
void storeDataOffline(float lat, float lon, const char* ts) {
  if (!sdAvailable) return;
  File f=SD.open("pending.txt",FILE_WRITE);
  if (!f) return;
  f.print(ts);f.print(',');f.print(lat,6);
  f.print(',');f.println(lon,6);
  f.close();
  Serial.println(F("Stored offline"));
}

void replayOfflineData() {
  if (!sdAvailable||!SD.exists("pending.txt")) return;
  File f=SD.open("pending.txt",FILE_READ);
  if (!f) return;
  bool allOK=true;
  char line[80];
  while(f.available()) {
    int len=f.readBytesUntil('\n',line,sizeof(line)-1);
    if (len<=0) continue;
    line[len]=0;
    char ts[GPS_TIMESTAMP_BUF_SIZE];
    float la,lo;
    if (sscanf(line,"%24[^,],%f,%f",ts,&la,&lo)==3) {
      Serial.print(F("Replaying: "));Serial.println(ts);
      if (!sendGpsToTraccar(TRACCAR_HOST,TRACCAR_PORT,DEVICE_ID,la,lo,ts)) {
        allOK=false;break;
      }
      delay(200);
    }
  }
  f.close();
  if (allOK) {
    SD.remove("pending.txt");
    Serial.println(F("Offline cleared"));
  }
}

// --- Setup
void setup() {
  Serial.begin(9600);
  while(!Serial);

  Serial.println(F("--- Arduino Initialized ---"));
  sdAvailable = SD.begin(SD_CS);
  Serial.print(F("SD ")); Serial.println(sdAvailable?F("OK"):F("FAIL"));

  pinMode(powerPin,OUTPUT);
  digitalWrite(powerPin,LOW);
  moduleSerial.begin(moduleBaudRate);

  Serial.println(F("Power ON module"));
  digitalWrite(powerPin, HIGH);
  delay(5000);

  if (!waitForInitialOK(15)) {
    Serial.println(F("Module unresponsive"));
    return;
  }
  Serial.println(F("Initial OK"));
  executeSimpleCommand("ATE0","OK",1000,2);
  executeSimpleCommand("AT+CMEE=2","OK",1000,2);

  // --- STEP 1: Network Settings
  executeSimpleCommand("AT+CIPSHUT","SHUT OK",500,2);
  executeSimpleCommand("AT+CFUN=0","OK",500,3);
  executeSimpleCommand("AT+CNMP=38","OK",500,3);
  executeSimpleCommand("AT+CMNB=1","OK",500,3);
  moduleSerial.println("AT+CFUN=1,1");
  delay(500);
  waitForInitialOK(15);

  // --- STEP 2: SIM PIN puis Network Registration
  {
    Serial.println(F("--- Waiting SIM READY ---"));
    for (uint8_t i=0; i<15; i++) {
      moduleSerial.println("AT+CPIN?");
      readSerialResponse(1000);
      if (strstr(responseBuffer,"+CPIN: READY")) {
        Serial.println(F("SIM READY"));
        break;
      }
      Serial.println(F("SIM NOT READY"));
      delay(2000);
    }

    Serial.println(F("--- Waiting network reg ---"));
    bool registered=false;
    for (uint8_t i=0; i<20 && !registered; i++) {
      moduleSerial.println("AT+CREG?");
      readSerialResponse(1000);
      char* cr=strstr(responseBuffer,"+CREG:");
      if (cr) {
        int n,st;
        if (sscanf(cr,"+CREG: %d,%d",&n,&st)==2 && (st==1||st==5)) {
          Serial.println(F("Registered"));
          registered=true;
          break;
        }
      }
      Serial.print(F("Not registered ("));Serial.print(i+1);
      Serial.println(F("/20)"));
      delay(2000);
    }
    if (!registered) {
      Serial.println(F("ERROR: Registration failed"));
      return;
    }
  }

  // --- STEP 3: PDP context
  snprintf(cmdBuffer,CMD_BUFFER_SIZE,"AT+CGDCONT=1,\"IP\",\"%s\"",APN);
  executeSimpleCommand(cmdBuffer,"OK",500,3);
  executeSimpleCommand("AT+CGATT=1","OK",500,3);
  executeSimpleCommand("AT+CGACT=1,1","OK",1000,2);

  // --- STEP 4: Enable GNSS
  executeSimpleCommand("AT+CGNSPWR=1","OK",500,3);

  setupSuccess = true;
  lastSendTime = millis();
}

// --- Loop
void loop() {
  if (!setupSuccess) return;
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    if (getGpsData(currentLat,currentLon,gpsTimestampTraccar)) {
      if (sendGpsToTraccar(TRACCAR_HOST,TRACCAR_PORT,DEVICE_ID,
                           currentLat,currentLon,gpsTimestampTraccar)) {
        replayOfflineData();
      } else {
        storeDataOffline(currentLat,currentLon,gpsTimestampTraccar);
      }
    }
  }
  readSerialResponse(50);
  delay(100);
}
