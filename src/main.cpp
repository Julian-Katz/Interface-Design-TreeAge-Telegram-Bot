#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <string.h>
#include <ctype.h>

// Measuring length
long int rotValue=0, swValue=0;
float rotations = 0;
uint8_t state=0;

#define ROTARY_PINA 32
#define ROTARY_PINB 25
#define ROTARY_PINSW 21

#define ONE_ROTATION 74

portMUX_TYPE gpioMux = portMUX_INITIALIZER_UNLOCKED;


// Telegram Bot Data
// Wifi network station credentials
#define WIFI_SSID "x"
#define WIFI_PASSWORD "x"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "x"
// Use @myidbot to find out the chat ID
#define CHAT_ID "x"

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;          // last time messages' scan has been done

const char* threes [4]  = { "Eiche", "Linde", "Eibe", "Kastanie" };
const float threesFactor [4]  = { 0.8, 0.8, 0.7, 0.7 };

boolean started = false;
boolean choseMode = false;
boolean manualMode = false;
boolean espMode = false;
int threeType;
float threeDiameterCm = 0;
int threeAge = 0;

// Measuring length 
void IRAM_ATTR isrAB() {
   uint8_t s = state & 3;

  portENTER_CRITICAL_ISR(&gpioMux);
    if (digitalRead(ROTARY_PINA)) s |= 4;
    if (digitalRead(ROTARY_PINB)) s |= 8;
    switch (s) {
      case 0: case 5: case 10: case 15:
        break;
      case 1: case 7: case 8: case 14:
        rotValue++; break;
      case 2: case 4: case 11: case 13:
        rotValue--; break;
      case 3: case 12:
        rotValue += 2; break;
      default:
        rotValue -= 2; break;
    }
    state = (s >> 2);
   portEXIT_CRITICAL_ISR(&gpioMux);
 
}

void IRAM_ATTR isrSWAll() {

 portENTER_CRITICAL_ISR(&gpioMux);
 swValue++;
 portEXIT_CRITICAL_ISR(&gpioMux);

}


// Telegram Bot
boolean isValidNumber(String str){
  for(byte i=0;i<str.length();i++)
  {
  if(isDigit(str.charAt(i))) return true;
  }
  return false;
}

void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (chat_id != CHAT_ID)
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    if (espMode) {
      if(text == "/bestätigen") {
        threeAge = threeDiameterCm * threesFactor[threeType];
        Serial.println(threeDiameterCm);
        const String message = "Das Alter des Baumes beträgt ungefähr " + String(threeAge) + " Jahre.";
        espMode = false;
        bot.sendMessage(chat_id, message, "");
      }
      else
      {
      rotValue=0;
      swValue=0;
      }
      
    }

    if (manualMode)
    {
      if (isValidNumber(text))
      {
        threeDiameterCm = text.toFloat();
        threeAge = threeDiameterCm * threesFactor[threeType];
        Serial.println(threeDiameterCm);
        const String message = "Das Alter des Baumes beträgt ungefähr " + String(threeAge) + " Jahre.";
        bot.sendMessage(chat_id, message, "");
      }
      else
      {
        bot.sendMessage(chat_id, "Du musst eine Nummer eingeben", ""); 
      }
      
    }
    

    if (choseMode)
    {
      Serial.println("step1");
      Serial.print(threes[threeType]);
      if(text == "/messenStarten")
      {
        choseMode = false;
        espMode = true;
        bot.sendMessage(chat_id, "Messe jetzt mit dem Gerät");
      } 
      else if (text == "/manuelleEingabe")
      {
        choseMode = false;
        manualMode = true;
        bot.sendMessage(chat_id, "Wie ist der Durchmesser des Baums in cm?");
      }

    }

    if (started)
    {
      Serial.println("started");
      for (size_t i = 0; i < 4; i++)
      {
        if (text == threes[i]) {
          threeType = i;
          bot.sendMessage(chat_id, "Um das Alter des Baumes zu bestimmen brauche ich den Durchmesser\nWähle zwischen:\n/messenStarten\n/manuelleEingabe");
          bot.sendMessage(chat_id, "Damit dein Ergebnis möglichst genau wird, messe den Durchmesser des Stamms in ca. 1m Höhe vom Boden aus.");
          String keyboardJson = "[\"/manuelleEingabe\", \"/messenStarten\"]";
          bot.sendMessageWithReplyKeyboard(chat_id, "Choose from one of the following options", "", keyboardJson, true, false, false);
          started = false;
          choseMode = true;
        }
      }
      if (!choseMode && started)
      {
        String threesString;
        for (size_t i = 0; i < 4; i++)
        {
          threesString = threesString + "\n" + threes[i];
        }
      bot.sendMessage(chat_id, "Baumart nicht gefunden! Versuche es erneut. Folgende Baumarten unterstütze ich:" + threesString);
      }
    }

    if (text == "/start")
    {
      String welcome = "Wilkommen!\nUm das Alter eines Baumes zu bestimmen benötige ich einige Informationen. Zuerst muss ich wissen um welche Baumart es sich handelt. Gebe den Namen der Baumart ein.";
      bot.sendMessage(chat_id, welcome);
      started = true;
    }
  }
}

void setup()
{
  // Measuring length setup
  pinMode(ROTARY_PINA, INPUT_PULLUP);
  pinMode(ROTARY_PINB, INPUT_PULLUP);
  pinMode(ROTARY_PINSW, INPUT_PULLUP);

  attachInterrupt(ROTARY_PINA, isrAB, CHANGE);
  attachInterrupt(ROTARY_PINB, isrAB, CHANGE);
  attachInterrupt(ROTARY_PINSW, isrSWAll, CHANGE);

  Serial.begin(115200);
  Serial.println();

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop()
{
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  if (espMode) {
  Serial.print("isrSWAll ");
  Serial.print(swValue);
  Serial.print(" rotValue ");
  Serial.println(rotValue);
  rotations = (float) rotValue / ONE_ROTATION;
  threeDiameterCm = rotations * 2 * 3.14 * 2.5;
  Serial.println(rotations);
  Serial.println(rotations * 2 * 3.14 * 2.5);
  bot.sendMessage(CHAT_ID, String(threeDiameterCm) + "cm Umfang", "");
  bot.sendMessage(CHAT_ID, "/bestätigen", "");
  bot.sendMessage(CHAT_ID, "/zurücksetzten", "");
  delay(100);
  }
}