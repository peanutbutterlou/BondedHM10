#include <BondedHM10.h>
#include <SoftwareSerial.h>

/*
 * BondedHM10_Peripheral example of the BondedHM10 Arduino library.
 * (https://github.com/peanutbutterlou/BondedHM10)
 *
 * See how to operate the HM-10 module in Peripheral mode using the BondedHM10 library. This example will also 
 * demonstrate how to do the following:
 *  - Configure an LED to be lit when there is an active connection.
 *  - Configure an LED to be blinked when data is being transmitted to/from the local HM-10 module.
 *  - Assign local functions to handle special events like when a connection has been established between 
 *    modules, a connection has been disconnected, and when a custom message or event is recieved.
 * 
 * Writen by Peanutbutterlou.
 */


const char* CENTRAL_ADDRESS = "606405CFCA4D"; // MAC address of the Central HM-10 module.
const long BAUD_RATE = 9600; // Baud rate of our HM-10's stream.
const byte RX_PIN = 2;
const byte TX_PIN = 3;
const byte STATE_PIN = 4; // Ardunio pin that hooks up to the PIO2 pin on the HM-10 module.
const byte RESET_PIN = 5; // Arduino pin that hooks up to the P11 pin on the HM-10 module.
const byte CONNECTED_LED_PIN = 6; // Pin of LED to light up when the HM-10 modules are connected.
const byte TRANSMITTED_LED_PIN = 7; // Pin of LED to blink when data is being transmitted to/from the remote HM-10.
const byte SEND_PIN = 8; // Pin of button/switch we use to trigger sending a batch of sample messages.
const long SENDMESSAGE_BUTTON_DEBOUNCE_TIMEOUT = 500;


// Define the unique IDs of the custom events you want to send. 
// It's convenient to define them as an enum.
enum EventID : uint16_t
{
  SensorReading = 0
};


// Instantiate a SoftwareSerial stream to have the HM-10 communicate over.
// We could also use HardwareSerial or any other class that derives from Stream.
SoftwareSerial bluetoothSerial(RX_PIN, TX_PIN);

// Instantiate a BondedHM10 object. When calling the BondedHM10 constructor we are providing the BondedHM10 
// library the following vital information:
// - The role the local HM-10 module will perform (in this case, Peripheral).
// - The MAC address of the Central HM-10 module that will be connecting to the local module.
// - The digital pin of the Arduino board that is wired to the PIO2 pin of the local HM-10 module that 
//   tells whether or not the a connection is active between HM-10 modules.
// - The digital pin of the Arduino board that is wired to the P11 pin of the local HM-10 module that,  
//   when held LOW for 100 miliseconds, will restart the local HM-10 module.
BondedHM10 bluetooth(BondedHM10::Role::Peripheral, CENTRAL_ADDRESS, STATE_PIN, RESET_PIN);


void setup()
{
  Serial.begin(9600);
  delay(1000);


  // Configure the LED pin to light when there is an active connection betwen HM-10 modules, and 
  // then tell the BondedHM10 library what that pin is.
  pinMode(CONNECTED_LED_PIN, OUTPUT);
  bluetooth.setConnectedOutputPin(CONNECTED_LED_PIN);
  
  // Configure the LED pin to be blinked when data is being transmitted to/from the local HM-10 module, 
  // and then tell the BondedHM10 library what that pin is.
  pinMode(TRANSMITTED_LED_PIN, OUTPUT);
  bluetooth.setDataTransmittedOutputPin(TRANSMITTED_LED_PIN);

  // Tell the BondedHM10 library which of our local functions to invoke when a connection has been 
  // established or disconnected.
  bluetooth.setConnectedHandler(onConnected);
  bluetooth.setDisconnectedHandler(onDisconnected);

  // Tell the BondedHM10 library which of our local functions to invoke when our local HM-10 module 
  // has received a full message or event from the remote HM-10 module.
  bluetooth.setMessageReceivedHandler(onMessageReceived);
  bluetooth.setEventReceivedHandler(onEventReceived);

  // Configure the pin of the button/switch that we'll be using in this example to trigger a batch 
  // of messages and events to be sent to the remote HM-10 module.
  pinMode(SEND_PIN, INPUT_PULLUP);


  bluetoothSerial.begin(9600); // Setup the SoftwareSerial stream.
  bluetooth.begin(bluetoothSerial); // Setup the BondedHM10, passing it our serial stream.
}


void loop()
{
  handleSendMessageButton();

  bluetooth.loop(); // Call the BondedHM10's "loop" function. It's what does all the work.
}


void handleSendMessageButton()
{
  if (digitalRead(SEND_PIN) == LOW)
  {
    // Send a batch of 10 custom messages and events to the remote HM-10 module.
    for (uint8_t index = 0; index < 10; index++)
    {
      sendMessage();
      sendEvent();
    }
    
    delay(SENDMESSAGE_BUTTON_DEBOUNCE_TIMEOUT);
  }
}


void sendMessage()
{
  bluetooth.writeMessage(F("This is a message."));
}


void sendEvent()
{
  const char* sensorReading = "100 degrees";
  bluetooth.writeEvent(EventID::SensorReading, sensorReading);
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