#include <BondedHM10.h>
#include <SoftwareSerial.h>

/*
 * BondedHM10_Central_ConsoleMode example of the BondedHM10 Arduino library.
 * (https://github.com/peanutbutterlou/BondedHM10)
 *
 * See how to put the BondedHM10 library into "Console" mode where, as either a Central or 
 * Peripheral, can execute AT commands directly against the local HM-10 module or manually 
 * send messages to the remote HM-10 via the Serial Monitor input. This example will focus 
 * on allowing the sending of message to the Peripheral from text entered in the Serial 
 * Monitor.
 * 
 * Writen by Peanutbutterlou.
 */
 

const char* PERIPHERAL_ADDRESS = "3403DE1C6FB2"; // MAC address of the Peripheral HM-10 module.
const long BAUD_RATE = 9600; // Baud rate of our HM-10's stream.
const bool AUTO_RECONNECT_ENABLED = true; // Whether or not to enable auto-reconnecting to the remote module.
const unsigned long AUTO_RECONNECT_TIMEOUT = 30000; // How often, in milliseconds, the BondedHM10 library will attempt to reconnect to the remote modeul.
const byte RX_PIN = 2;
const byte TX_PIN = 3;
const byte STATE_PIN = 4; // Ardunio pin that hooks up to the PIO2 pin on the HM-10 module.
const byte RESET_PIN = 5; // Arduino pin that hooks up to the P11 pin on the HM-10 module.
const byte CONNECTED_LED_PIN = 6; // Pin of LED to light up when the HM-10 modules are connected.
const byte DISCONNECT_PIN = 7; // Pin of the button/switch we use to trigger a disconnect/re-connect with the remote HM-10.
const byte TRANSMITTED_LED_PIN = 9; // Pin of LED to blink when data is being transmitted to/from the remote HM-10.


// Instantiate a SoftwareSerial stream to have the HM-10 communicate over.
// We could also use HardwareSerial or any other class that derives from Stream.
SoftwareSerial bluetoothSerial(RX_PIN, TX_PIN);

// Instantiate a BondedHM10 object. When calling the BondedHM10 constructor we are providing the BondedHM10 
// library the following vital information:
// - The role the local HM-10 module will perform (in this case, Central).
// - The MAC address of the Peripheral HM-10 module that the local module will be connecting to.
// - The digital pin of the Arduino board that is wired to the PIO2 pin of the local HM-10 module that 
//   tells whether or not the a connection is active between HM-10 modules.
// - The digital pin of the Arduino board that is wired to the P11 pin of the local HM-10 module that,  
//   when held LOW for 100 miliseconds, will restart the local HM-10 module.
BondedHM10 bluetooth(BondedHM10::Role::Central, PERIPHERAL_ADDRESS, STATE_PIN, RESET_PIN);


void setup()
{
  Serial.begin(9600);
  delay(1000);


  // Tell the BondedHM10 library that we want the local Central HM-10 module to automatically try to 
  // reconnect to the remote Peripheral HM-10 module in the event that it becomes disconnected, or if 
  // the attempt to connect to the Peripheral at startup had failed.
  // Also, tell the BondedHM10 library how often to attempt to reconnect.
  bluetooth.setAutoReconnectEnabled(AUTO_RECONNECT_ENABLED);
  bluetooth.setAutoReconnectTimeout(AUTO_RECONNECT_TIMEOUT);

  // Configure the LED pin to light when there is an active connection betwen HM-10 modules, and 
  // then tell the BondedHM10 library what that pin is.
  pinMode(CONNECTED_LED_PIN, OUTPUT);
  bluetooth.setConnectedOutputPin(CONNECTED_LED_PIN);

  // Configure the LED pin to be blinked when data is being transmitted to/from the local HM-10 module, 
  // and then tell the BondedHM10 library what that pin is.
  pinMode(TRANSMITTED_LED_PIN, OUTPUT);
  bluetooth.setDataTransmittedOutputPin(TRANSMITTED_LED_PIN);

  // Configure the pin of the button/switch that we'll be using in this example to trigger a disconnection 
  // and re-connection with the remote HM-10 module.
  pinMode(DISCONNECT_PIN, INPUT_PULLUP);
  bluetooth.setDisconnectReconnectInputPin(DISCONNECT_PIN); // The DISCONNECT_PIN will be read to detect when to trigger disconnect/re-connect.

  // Tell the BondedHM10 library which of our local functions to invoke when a connection has been 
  // established or disconnected.
  bluetooth.setConnectedHandler(onConnected);
  bluetooth.setDisconnectedHandler(onDisconnected);

  // Tell the BondedHM10 library to be in "Console" mode where all text input into the Serial Monitor will 
  // get sent directly to the remote HM-10 module and all incoming text from the remote HM-10 module will 
  // be written out directly to the Serial Monitor. In Console Mode, normal custom message/event processing 
  // is NOT performed. Also, if there is no active connection to a remote HM-10, AT commands can be input 
  // into the Serial Monitor to manually change the configuration of the local HM-10 module. This is useful 
  // in some cases when debugging the BondedHM10 library or when you want to experiment with some of the other 
  // built-in features of the HM-10 module you're using.
  bluetooth.setConsoleModeEnabled(true);

  // **NOTE** While "Console" mode is enabled and there is an active connection to a remote HM-10 module, 
  // any Serial printing will be sent directly to the remote HM-10.


  bluetoothSerial.begin(9600); // Setup the SoftwareSerial stream.
  bluetooth.begin(bluetoothSerial, true); // Setup the BondedHM10, passing it our serial stream and instructing the BondedHM10 library to attempt to attempt to connect to the Peripheral module as soon as setup is complete.
}


void loop()
{
  bluetooth.loop();
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