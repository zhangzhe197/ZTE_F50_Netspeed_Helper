#include <GyverOLED.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <assert.h>
#include <math.h>



char ssid[] = "";       // your network SSID (name)
char pass[] = "";           // your network password
String serverName = "http://192.168.0.1/goform/goform_get_cmd_process?multi_data=1&isTest=false&sms_received_flag_flag=0&sts_received_flag_flag=0&cmd=sms_received_flag,realtime_tx_thrpt,realtime_rx_thrpt,signalbar&_=1725783330322";
char res[5000];
char js[256];
GyverOLED<SSH1106_128x64> oled;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiClient client;
uint32_t lastTime = millis();
uint32_t timerDelay = 2000;
IPAddress server(192,168,0,1); 

class WifiInfo
{
  public:
    int realRX, realTX , signalBar;
    float dataUsed;
    int RXhistory[60], TXhistory[60];
    const char *sms;
  void getWifiInfo(bool syncDataUse);
  void serialPrintMembers();
  void flushNetSpeedHistory();
  void printNetSpeed(int speedType, char* number);
};

class OLEDdisplay
{
  public:
    WifiInfo info;
  void refresh_info();
  void initData(WifiInfo input);
  void displayTime(int x, int y,int size);
  void displayNetSpeed(int x, int y, int size,int spacing);
  void displayHistoryBar(int x ,int y, int resolution,int spacing);
  void displayMessages(int x, int y);
  void displaysignleBar(int x, int y,int resolution, int* history);
  void displaysignalBar(int x, int y);
  void displayDataBar(int x, int y, int length, int width);
};


WifiInfo APinfo;
OLEDdisplay oledDisplay;

void setup() {
    Serial.begin(115200);
    oled.init();  
    oled.clear();
    oled.update();
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    delay(1000);
    timeClient.begin();
    timeClient.update();
    timeClient.setTimeOffset(8 * 3600);
    oledDisplay.initData(APinfo);
    delay(1000);
    client.connect(server, 80);
    
}


void loop() {

  while(true)
  {
        lastTime = millis();
        oledDisplay.refresh_info();
        oled.clear();
        oledDisplay.displayTime(0, 0, 3);
        oledDisplay.displayHistoryBar(0, 51, 12, 64);
        oledDisplay.displayNetSpeed(0,38, 1, 64);
        oledDisplay.displayMessages(110,4);
        oledDisplay.displaysignalBar(95,6);
   //     oledDisplay.displayDataBar(0, 30 ,100,5);
        oled.update();
        if(millis() - lastTime < timerDelay) delay(timerDelay - millis() + lastTime);
  }
}








void WifiInfo::getWifiInfo(bool syncDataUse)
  {
    realRX = realTX = -1;
    sms = "0";
    signalBar = 0;

    unsigned i = 0;
    if (client.connected()) {
        Serial.println("connected to server");
        // Make a HTTP request:
        if(!syncDataUse) client.println("GET /goform/goform_get_cmd_process?multi_data=1&isTest=false&sms_received_flag_flag=0&sts_received_flag_flag=0&cmd=sms_received_flag,realtime_tx_thrpt,realtime_rx_thrpt,signalbar&_=1725783330322 HTTP/1.1");
        else client.println("GET /goform/goform_get_cmd_process?multi_data=1&isTest=false&sms_received_flag_flag=0&sts_received_flag_flag=0&cmd=sms_received_flag,realtime_tx_thrpt,realtime_rx_thrpt,monthly_tx_bytes,monthly_rx_bytes,signalbar&_=1725783330322 HTTP/1.1");
        client.println("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
        client.println("Accept-Encoding: gzip, deflate");
        client.println("Accept-Language: en,zh-CN;q=0.9,zh;q=0.8");
        client.println("Cache-Control: max-age=0\nConnection: keep-alive\nHost: 192.168.0.1\nUpgrade-Insecure-Requests: 1\nreferer:http://192.168.0.1/index.html");
        client.println();
    }
    else client.connect(server, 80);
    unsigned long timeout = millis();
    while (client.available() == 0 && millis() - timeout < 2000) {
        delay(1);  // 等待最多1秒
    }
    while (client.available()) {
        if (i < sizeof(res) - 1) {
            res[i++] = client.read();
        }
    }
    res[i] = '\0';

    unsigned j = i;
    while(res[j] != '{' && j != 0) j--;
    strcpy(js, &res[j]);
    js[i - j] = '\0';
    Serial.println(js);
    if(i > 50){
        JSONVar jsonObject = JSON.parse(js);
        if(jsonObject.hasOwnProperty("sms_received_flag")) sms = (const char*)jsonObject["sms_received_flag"];
        if(jsonObject.hasOwnProperty("signalbar")) signalBar = (int)jsonObject["signalbar"];
        if(jsonObject.hasOwnProperty("realtime_tx_thrpt")) realTX = (int)jsonObject["realtime_tx_thrpt"];
        if(jsonObject.hasOwnProperty("realtime_rx_thrpt")) realRX = (int)jsonObject["realtime_rx_thrpt"];
        if(jsonObject.hasOwnProperty("monthly_tx_bytes")) dataUsed = (double)jsonObject["monthly_tx_bytes"];
       if(jsonObject.hasOwnProperty("monthly_rx_bytes")) dataUsed += (double)jsonObject["monthly_rx_bytes"];
    }

   // serialPrintMembers();
    if(realTX == -1)
    { 
      Serial.print(res);
      for(int line = 0; line < 20; line ++)
      {
        for(int k = 0; k < 25 ; k ++)
        {
          Serial.print(res[line * 25 + k]);
        }
        Serial.println();
      }
    }
    if(timeClient.getSeconds() == 0 && timeClient.getMinutes() % 30 == 0)
    {
      client.stop();
      delay(100);
      client.connect(server , 80);
    }

  }

void WifiInfo::printNetSpeed(int speedType, char* number)
{
  int rawSpeed = -1;
  const char* units = "";
  if(speedType == 1) rawSpeed = realTX;
  else rawSpeed = realRX;
  if(rawSpeed < 1000) units = "B/s";
  else if(rawSpeed < 1000 * 1000)
  {
    rawSpeed /= 1000;
    units = "KB/s";
  }
  else
  {
    rawSpeed /= 1000000;
    units = "MB/s";
  }
  itoa(rawSpeed, number,10);
  strcat(number, units);
}

void WifiInfo::flushNetSpeedHistory()
{
  int secondNow = timeClient.getSeconds();
  TXhistory[secondNow] = realTX;
  RXhistory[secondNow] = realRX;
}

void WifiInfo::serialPrintMembers()
{
  Serial.print("sms = ");
  Serial.print(sms);
  Serial.print("  realTX = ");
  Serial.print(realTX);
  Serial.print("  realRX = ");
  Serial.print(realRX);
  Serial.print("  signalBar = ");
  Serial.println(signalBar);
}

void OLEDdisplay::refresh_info()
{
  info.getWifiInfo(0);
  info.flushNetSpeedHistory();
  if(timeClient.getSeconds() == 0) 
  {
    Serial.println("sync time");
    timeClient.update();
  }
}

void OLEDdisplay::displayTime(int x, int y,int size)
{
  oled.setScale(size);
  oled.setCursor(x,y);
  oled.print(timeClient.getFormattedTime().substring(0,5));
}

void OLEDdisplay::displayNetSpeed(int x, int y, int size, int spacing)
{
  oled.setScale(size);
  oled.setCursor(x,y);
  oled.print("r:");
  char strings[15];
  info.printNetSpeed(0, strings);
  oled.print(strings);

  oled.setCursor(x + spacing , y);
  oled.print("t:");
  info.printNetSpeed(1, strings);
  oled.print(strings);
}

void OLEDdisplay::displayHistoryBar(int x, int y, int resolution, int spacing)
{
  displaysignleBar(x , y ,resolution, info.RXhistory);
  displaysignleBar(x + spacing, y ,resolution , info.TXhistory);
}

void OLEDdisplay::displaysignleBar(int x, int y,int resolution, int *history)
{
  int minNetSpeed = 300000;
  int secondNow = timeClient.getSeconds();
  for(int i = 2; i < 60 ; i++)
  {
    if(minNetSpeed <  history[i] + history[i - 1] + history[i - 2])  minNetSpeed  =  history[i] + history[i - 1] + history[i - 2];
  }
  for(int i = 0; i < 58 ;i++)
  {
    oled.dot(x + i, y + resolution - (history[(60 - i + secondNow) % 60] + \
                                      history[(59 - i + secondNow) % 60] + \
                                      history[(58 - i + secondNow) % 60] ) * resolution / minNetSpeed);
  }
}
void OLEDdisplay::displayMessages(int x, int y)
{
  if(info.sms[0] == '1')
  {
    oled.setScale(1);
    oled.setCursor(x,y);
    oled.print("M");
  }
}
void OLEDdisplay::displaysignalBar(int x, int y)
{
  uint8_t signal = info.signalBar;
  while(signal--)
  {
    oled.rect(x + signal * 3 , y + 5 , x + signal * 3 + 1, y + 5 - signal * 2);
  }
}
void OLEDdisplay::initData(WifiInfo input)
{
  info = input;
}
void OLEDdisplay::displayDataBar(int x, int y, int length , int width)
{
  float allData = 429496729600.0;
  oled.fastLineH(y, x , x + length);
  oled.fastLineH(y + width, x , x + length);
  oled.fastLineV(x, y, y + width);
  oled.fastLineV(x + length, y, y + width);
}
