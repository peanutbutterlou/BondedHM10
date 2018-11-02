#include <BondedHM10.h>
#include <SoftwareSerial.h>
 

const char* PERIPHERAL_ADDRESS = "3403DE1C6FB2";
const long BAUD_RATE = 9600;
const bool AUTO_RECONNECT_ENABLED = true;
const unsigned long AUTO_RECONNECT_TIMEOUT = 30000;
const byte RX_PIN = 2;
const byte TX_PIN = 3;
const byte STATE_PIN = 4;
const byte RESET_PIN = 5;
const byte CONNECTED_LED_PIN = 6;
const byte DISCONNECT_PIN = 7;
const byte SEND_PIN = 8;
const byte TRANSMITTED_LED_PIN = 9;
const long SENDTESTMESSAGE_BUTTON_DEBOUNCE_TIMEOUT = 500;

enum EventID : uint16_t
{
  SensorReading = 0
};


SoftwareSerial btSerial(RX_PIN, TX_PIN);
BondedHM10 bt(BondedHM10::Role::Central, PERIPHERAL_ADDRESS, STATE_PIN, RESET_PIN);


void setup()
{
  Serial.begin(9600);
  delay(1000);


  bt.setAutoReconnectEnabled(AUTO_RECONNECT_ENABLED);
  bt.setAutoReconnectTimeout(AUTO_RECONNECT_TIMEOUT);

  pinMode(CONNECTED_LED_PIN, OUTPUT);
  bt.setConnectedOutputPin(CONNECTED_LED_PIN);

  pinMode(TRANSMITTED_LED_PIN, OUTPUT);
  bt.setDataTransmittedOutputPin(TRANSMITTED_LED_PIN);

  pinMode(DISCONNECT_PIN, INPUT_PULLUP);
  bt.setDisconnectReconnectInputPin(DISCONNECT_PIN); // The DISCONNECT_PIN will be read to detect when to trigger disconnect/re-connect.

  bt.setConnectedHandler(onConnected);
  bt.setDisconnectedHandler(onDisconnected);

  bt.setMessageReceivedHandler(onMessageReceived);
  bt.setEventReceivedHandler(onEventReceived);


  pinMode(SEND_PIN, INPUT_PULLUP);


  btSerial.begin(9600);

  if (bt.begin(btSerial, true))
  {
    //bt.provision(BondedHM10::BaudRate::Baud_9600);

    #ifdef DEBUG
      bt.printDeviceConfig();
    #endif
  }
}


void loop()
{
  handleSendTestMessageButton();

  bt.loop();
}


void handleSendTestMessageButton()
{
  if (digitalRead(SEND_PIN) == LOW)
  {
    for (uint8_t index = 0; index < 10; index++)
    {
      sendTestMessage();
      sendTestEvent();
    }
    
    delay(SENDTESTMESSAGE_BUTTON_DEBOUNCE_TIMEOUT);
  }
}


void sendTestMessage()
{
  bt.writeMessage(F("This is a test message."));
}


void sendTestEvent()
{
  const char* sensorReading = "100 degrees";
  bt.writeEvent(EventID::SensorReading, sensorReading);
}


void onConnected(bool reconnected)
{
  if (reconnected)
  {
    Serial.println("Reconnected!!!");
  }
  else
  {
    Serial.println("Connected!!!");
  }
}


void onDisconnected()
{
  Serial.println("Disconnected!!!");
}


void onMessageReceived(const char* content, const uint16_t length)
{
  Serial.print("Message received. Content = ");
  Serial.write(content, length);
  Serial.println();
}


void onEventReceived(const uint16_t id, const char* content, const uint16_t length)
{
  Serial.print("Event received. Event ID = ");
  Serial.print(id);
  Serial.print(", Content = ");
  Serial.write(content, length);
  Serial.println();
}