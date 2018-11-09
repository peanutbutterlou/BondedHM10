#include <BondedHM10.h>
#include <SoftwareSerial.h>
 
 /*
 * BondedHM10_Central_Provision example of the BondedHM10 Arduino library.
 * (https://github.com/peanutbutterlou/BondedHM10)
 *
 * See how to provision an HM-10 module so that it can operate in "Central" mode and 
 * print out to the Serial Monitor the module's MAC address.
 * 
 * Writen by Peanutbutterlou.
 */


const byte RX_PIN = 2;
const byte TX_PIN = 3;
const byte STATE_PIN = 4; // Ardunio pin that hooks up to the PIO2 pin on the HM-10 module.
const byte RESET_PIN = 5; // Arduino pin that hooks up to the P11 pin on the HM-10 module.


// Instantiate a SoftwareSerial stream to have the HM-10 communicate over.
// We could also use HardwareSerial or any other class that derives from Stream.
SoftwareSerial bluetoothSerial(RX_PIN, TX_PIN);

// Instantiate a BondedHM10 object. We don't need to provide the address of the Peripheral module since 
// all we're going to be doing is provisioning.
BondedHM10 bluetooth(BondedHM10::Role::Central, NULL, STATE_PIN, RESET_PIN);


void setup()
{
  Serial.begin(9600);
  delay(1000);


  bluetoothSerial.begin(9600); // Setup the SoftwareSerial stream.
  bluetooth.begin(bluetoothSerial, true); // Setup the BondedHM10, passing it our serial stream.


  // Perform the provisioning. If it is successful, print out to the Serial Monitor the MAC 
  // address of the local HM-10 module.
  if (bluetooth.provision(BondedHM10::BaudRate::Baud_9600))
  {
    Serial.println("Provisioning successful.");

    Serial.print("MAC Address of local HM-10 Module = ");

    char* addressStr = (char*)calloc(32, sizeof(char));
    if (bluetooth.getAddress(addressStr))
    {
      Serial.println(addressStr);
    }
    else
    {
      Serial.println("FAILED");
    }
  }
  else
  {
    Serial.println("Provisioning FAILED");
  }
}


void loop()
{
  
}