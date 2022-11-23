#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <string.h>
#include <ctype.h>

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

// data
const char* threes [4]  = { "Eiche", "Linde", "Eibe", "Kastanie" };
const float threesFactor [4]  = { 0.8, 0.8, 0.7, 0.7 };

boolean started = false;
boolean choseMode = false;
boolean manualMode = false;
boolean espMode = false;
int threeType;
int threeDiameterCm = 0;
int threeAge = 0;

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

    if (manualMode)
    {
      if (isValidNumber(text))
      {
        threeDiameterCm = text.toInt();
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
        bot.sendMessage(chat_id, "/messenStarten Messe jetzt mit dem Gerät");
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
}