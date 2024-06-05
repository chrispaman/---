//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ESP32 Lora Ra-02 Slave 2
// More detailed information about this project, please look at the program code for the Master.

//---------------------------------------- Include Library.
#include <SPI.h>
#include <LoRa.h>
#include "DHT.h"
//---------------------------------------- 

//---------------------------------------- Defines the DHT11 Pin and the DHT type.
#define DHTPIN      15
#define DHTTYPE     DHT11
//---------------------------------------- 
#define button 27
#define LED_PIN  26
//---------------------------------------- Defines LED Pins.
#define LED_1_Pin   25

//---------------------------------------- 

//---------------------------------------- LoRa Pin / GPIO configuration.
#define ss 5
#define rst 14
#define dio0 2
//----------------------------------------

// Initializes the DHT sensor (DHT11).
DHT dht11(DHTPIN, DHTTYPE);

//----------------------------------------String variable for LoRa
String Incoming = "";
String Message = "";
String LED_Num = "";
String LED_State = "";
//----------------------------------------

//---------------------------------------- LoRa data transmission configuration.
////////////////////////////////////////////////////////////////////////////
// PLEASE UNCOMMENT AND SELECT ONE OF THE "LocalAddress" VARIABLES BELOW. //
////////////////////////////////////////////////////////////////////////////

// byte LocalAddress = 0x02;       //--> address of this device (Slave 1).
// byte LocalAddress = 0x03;       //--> address of this device (Slave 2).
byte LocalAddress = 0x03;       //--> address of this device (BUTTON 2).
byte Destination_Master = 0x01; //--> destination to send to Master (ESP32).
//----------------------------------------
unsigned long LED_OnTime = 0;  // When the LED was turned on
const unsigned long LED_OnDuration = 8000; 

//---------------------------------------- Variable declarations for the reading status of the DHT11 sensor, temperature and humidity values.
int Humd = 0;
float Temp = 0.00;
String send_Status_Read_DHT11 = "";
//---------------------------------------- 

//---------------------------------------- Variable declaration for Millis/Timer.
unsigned long previousMillis_UpdateDHT11 = 0;
const long interval_UpdateDHT11 = 1000;

unsigned long previousMillis_RestartLORA = 0;
const long interval_RestartLORA = 1000;
//---------------------------------------- 
unsigned long previousMillis = 0;  // Stores last time the LED was updated
unsigned long previousLoRaMillis =0;  // Stores last time the LoRa packet was sent
unsigned long previousLEDMillis = 0; 
const long ledOffInterval = 20000; 
bool ledOn = false;


// Declaration of variable as counter to restart Lora Ra-02.
byte Count_to_Rst_LORA = 0;

//message counter 
byte msgCount = 0;

//________________________________________________________________________________ Subroutines for sending data (LoRa Ra-02).
void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();             //--> start packet
  LoRa.write(Destination);        //--> add destination address
  LoRa.write(LocalAddress);       //--> add sender address
  LoRa.write(Outgoing.length());  //--> add payload length
  LoRa.print(Outgoing);           //--> add payload
  LoRa.endPacket();               //--> finish packet and send it
}
//________________________________________________________________________________ button controlling


//________________________________________________________________________________ Subroutines for receiving data (LoRa Ra-02).
void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  //---------------------------------------- read packet header bytes:
  int recipient = LoRa.read();        //--> recipient address
  byte sender = LoRa.read();          //--> sender address
  byte incomingLength = LoRa.read();  //--> incoming msg length
  byte master_Send_Mode = LoRa.read();
  //---------------------------------------- 

  //---------------------------------------- Condition that is executed if message is not from Master.
  if (sender != Destination_Master) {
    Serial.println();
    Serial.println("i"); //--> "i" = Not from Master, Ignore.
    //Serial.println("Not from Master, Ignore");

    // Resets the value of the Count_to_Rst_LORA variable.
    Count_to_Rst_LORA = 0;
    return; //--> skip rest of function
  }
  //---------------------------------------- 

  // Clears Incoming variable data.
  Incoming = "";

  //---------------------------------------- Get all incoming data.
  while (LoRa.available()) {
    Incoming += (char)LoRa.read();
  }
  //---------------------------------------- 

  // Resets the value of the Count_to_Rst_LORA variable.
  Count_to_Rst_LORA = 0;

  //---------------------------------------- Check length for error.
  if (incomingLength != Incoming.length()) {
    Serial.println();
    Serial.println("er"); //--> "er" = error: message length does not match length.
    //Serial.println("error: message length does not match length");
    return; //--> skip rest of function
  }
  //---------------------------------------- INDICATOR
    digitalWrite(LED_PIN, HIGH);
    delay(100); // Keep it on for 100 milliseconds
    digitalWrite(LED_PIN, LOW);
    delay(100);

  //---------------------------------------- Checks whether the incoming data or message for this device.
  if (recipient != LocalAddress) {
    Serial.println();
    Serial.println("!");  //--> "!" = This message is not for me.
    //Serial.println("This message is not for me.");
    return; //--> skip rest of function
  } else {
    // if message is for this device, or broadcast, print details:
    Serial.println();
    Serial.println("Rc from: 0x" + String(sender, HEX));
    Serial.println("Message: " + Incoming);

    // Calls the Processing_incoming_data() subroutine.
    if (master_Send_Mode == 1) Processing_incoming_data();
    
    // Calls the Processing_incoming_data_for_Ctrl_LEDs() subroutine.
    if (master_Send_Mode == 2) Processing_incoming_data_for_Ctrl_LEDs();
  }
}
//________________________________________________________________________________ 

//________________________________________________________________________________ Subroutine to process the data to be sent, after that it sends a message to the Master.
void Processing_incoming_data() {
  // Get the last state of the LEDs.
  byte LED_1_State = digitalRead(LED_1_Pin);


  // Fill in the "Message" variable with the DHT11 sensor reading status, humidity value, temperature value, state of LED 1 and LED 2.
  Message = "";
  Message = send_Status_Read_DHT11 + "," + String(Humd) + "," + String(Temp) + "," + String(LED_1_State);
  
  Serial.println();
  Serial.println("Tr to  : 0x" + String(Destination_Master, HEX));
  Serial.println("Message: " + Message);

  // Send a message to Master.
  sendMessage(Message, Destination_Master);
}
//________________________________________________________________________________ 

//________________________________________________________________________________ Subroutines for processing data from incoming messages to Controlling the LEDs.
void Processing_incoming_data_for_Ctrl_LEDs() {

//  - The data received from the Master when the LED control mode is: "LED_number,LED_state".
//  - For example, the Master wants to turn on LED 1. Then the contents of the message received are: "1,t"
//    > "1" = LED number controlled.
//    > "t" = LED state. "t" to turn on and "f" to turn off.
  bool buttonPressed = digitalRead(button) == LOW; 

  LED_Num = GetValue(Incoming, ',', 0);
  LED_State = GetValue(Incoming, ',', 1);


  if (LED_Num == "1") {
    if (LED_State == "t"||buttonPressed ) digitalWrite(LED_1_Pin, HIGH);
    if (LED_State == "f") digitalWrite(LED_1_Pin, LOW);
  }



   
 

}
//________________________________________________________________________________ 

//________________________________________________________________________________ String function to process the data received
// I got this from : https://www.electroniclinic.com/reyax-lora-based-multiple-sensors-monitoring-using-arduino/
String GetValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//________________________________________________________________________________ 

//________________________________________________________________________________ Subroutine to reset Lora Ra-02.
void Rst_LORA() {
  LoRa.setPins(ss, rst, dio0);

  Serial.println();
  Serial.println(F("Restart LoRa..."));
  Serial.println(F("Start LoRa init..."));
  if (!LoRa.begin(433E6)) {             // initialize ratio at 915 or 433 MHz
    Serial.println(F("LoRa init failed. Check your connections."));
    while (true);                       // if failed, do nothing
  }
  Serial.println(F("LoRa init succeeded."));

  // Resets the value of the Count_to_Rst_LORA variable.
  Count_to_Rst_LORA = 0;
}
//________________________________________________________________________________ 

//________________________________________________________________________________ VOID SETUP
void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  
  pinMode(button, INPUT_PULLUP);

  pinMode(LED_1_Pin, OUTPUT);


  pinMode(LED_PIN, OUTPUT);// INDICATOR

   
  // Calls the Rst_LORA() subroutine.
  Rst_LORA();
  
  dht11.begin();
}
//________________________________________________________________________________ 

//________________________________________________________________________________ VOID LOOP
void loop() {


  int sendButtonstate = digitalRead(button);
  unsigned long currentMillis = millis();

  if (sendButtonstate == LOW) {
    // Send LoRa packet immediately when the button is pressed
    LoRa.beginPacket();
    LoRa.print("button pressed");
    LoRa.endPacket();
    Serial.println("Sending packet");

    // Turn on the LED immediately when the button is pressed
    digitalWrite(LED_1_Pin, HIGH);
    previousLEDMillis = currentMillis;  // Reset the timer
    ledOn = true;
    delay(250);
  } else if (ledOn && currentMillis - previousLEDMillis >= ledOffInterval) {
    // Turn off the LED after the specified interval
    digitalWrite(LED_1_Pin, LOW);
    ledOn = false;
  }
  
  Processing_incoming_data_for_Ctrl_LEDs();
 
      
     
  //---------------------------------------- Millis / Timer to update the temperature and humidity values ​​from the DHT11 sensor every 2 seconds (see the variable interval_UpdateDHT11).
  unsigned long currentMillis_UpdateDHT11 = millis();
  
  if (currentMillis_UpdateDHT11 - previousMillis_UpdateDHT11 >= interval_UpdateDHT11) {
    previousMillis_UpdateDHT11 = currentMillis_UpdateDHT11;

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    Humd = dht11.readHumidity();
    // Read temperature as Celsius (the default)
    Temp = dht11.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float f = dht.readTemperature(true);
  
    // Check if any reads failed and exit early (to try again).
    if (isnan(Humd) || isnan(Temp)) {
      Humd = 0;
      Temp = 0.00;
      send_Status_Read_DHT11 = "f";
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    } else {
      send_Status_Read_DHT11 = "s";
    }
  }
  //---------------------------------------- 
  
  //---------------------------------------- Millis/Timer to reset Lora Ra-02.
  // Please see the Master program code for more detailed information about the Lora reset method.
  
  unsigned long currentMillis_RestartLORA = millis();
  
  if (currentMillis_RestartLORA - previousMillis_RestartLORA >= interval_RestartLORA) {
    previousMillis_RestartLORA = currentMillis_RestartLORA;

    Count_to_Rst_LORA++;
    if (Count_to_Rst_LORA > 30) {
      LoRa.end();
      Rst_LORA();
    }
  }
  //---------------------------------------- 

  //---------------------------------------- parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());

  
  //----------------------------------------
}
//________________________________________________________________________________ 
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

