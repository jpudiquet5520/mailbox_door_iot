/*
This Arduino code is designed for an ESP32 board and includes functionality
for handling Wi-Fi connections, MQTT communication, HTTP requests, and time-related functions.
  
Libraries:
  - WiFi.h: Provides functions for connecting to and managing Wi-Fi networks.
  - PubSubClient.h: Enables MQTT communication for sending and receiving messages.
  - HTTPClient.h: Allows making HTTP requests to external servers.
  - Time.h: Provides time-related functions such as getting the current time and date.

External File:
  - wifi_info.h: Contains Wi-Fi credentials and other configurations. Separate this file for security reasons.

Constants and Macros:
  - LED_ENABLED: Controls whether to turn on the onboard LED when connected to Wi-Fi (set to false).
  - MAX_OPENDOOR_TIME: Maximum time to wait while the door is open (30 seconds in milliseconds).
  - MAX_STUCK_BOOT_COUNT: Maximum count of times the door can be stuck open before switching to a different mode.
  - TIMER_SLEEP_MICROSECS: Sleep duration in microseconds when on timer interrupt.

MQTT Topics and Messages:
  - message_door_open: MQTT message for when the door is opened.
  - message_door_closed: MQTT message for when the door is closed (empty string indicates no message).
  - message_door_stuck: MQTT message for when the door is stuck open.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Time.h>


#include "wifi_info.h"  

#define LED_ENABLED false  
#define MAX_OPENDOOR_TIME 30000    
#define MAX_STUCK_BOOT_COUNT 5 
#define TIMER_SLEEP_MICROSECS 10 * 1000000 //180 seconds or 30 minutes


#define  message_door_open "Your Mailbox Has Been Opened"
#define  message_door_closed "Mailbox Closed"
#define  message_door_stuck "Your Mailbox Was Not Shut Correctly!"


/*
This section of the Arduino code initializes various components and variables related to Wi-Fi, MQTT,
HTTP requests, GPIO pins, and persistent data storage using RTC memory on an ESP32 board.

Components:
  - WiFiClient espClient: Represents the ESP32's Wi-Fi client for connecting to Wi-Fi networks.
  - PubSubClient client(espClient): Initializes an MQTT client using the Wi-Fi client for MQTT communication.
  - HTTPClient http: Creates an HTTP client for making HTTP requests.

GPIO Pins and States:
  - sensorPIN: Specifies the GPIO pin connected to the reed switch sensor for detecting door status.
  - GPIO_INPUT_IO_TRIGGER: Alias for the doorSensorPIN, used for GPIO input trigger.
  - GPIO_DOOR_CLOSED_STATE: Defines the logic level (LOW) indicating the door is closed.
  - GPIO_DOOR_OPEN_STATE: Computes the logic level opposite to GPIO_DOOR_CLOSED_STATE for detecting open door.  
*/
WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;   


gpio_num_t  doorSensorPIN = GPIO_NUM_2;  // Reed GPIO PIN:
gpio_num_t GPIO_INPUT_IO_TRIGGER = doorSensorPIN;
int GPIO_DOOR_CLOSED_STATE= LOW;  
int GPIO_DOOR_OPEN_STATE = HIGH;  

/*
  RTC_DATA_ATTR is an attribute used in ESP32 development to mark variables
  that should be stored persistently in the ESP32's Real-Time Clock (RTC) memory.

  Persistence:
    - Variables marked with RTC_DATA_ATTR are stored in the RTC memory.
    - RTC memory retains data even when the microcontroller is powered off or in deep sleep mode.

  Fast Access:
    - Accessing data from RTC memory is faster than accessing data from other types of non-volatile memory.

  Usage:
    - Use RTC_DATA_ATTR for variables that need to retain their values across power cycles or deep sleep periods.
    - Initialize RTC_DATA_ATTR variables during the first boot or explicitly in the code.

  - bootCount: Stores the number of times the device has booted.
  - stuckbootCount: Counts the number of times the door has been stuck open.
  - last_doorState: Records the last known state of the door to detect if it's stuck open.
  - time_awake_millis: Tracks the total awake time in milliseconds for battery usage monitoring.
*/
RTC_DATA_ATTR long bootCount = 0;
RTC_DATA_ATTR long stuckbootCount = 0;  
RTC_DATA_ATTR long last_doorState=-99;   
RTC_DATA_ATTR long time_awake_millis=0;  

long currentMillis;  
int doorState = LOW;    
int start_time = 0;  
char total_time_awake[21]; //used to store a formatted string representing the total time the system has been awake
long rssi = 0; //measures WiFi signal strength. Set to zero to begin
 

//Connect to WiFi
void setup_wifi() 
{
  start_time=millis(); // initialized with the current value of milliseconds since the ESP32 started running. Used to track how long the connection process takes.
  
  //These lines output messages to the serial monitor for debugging purposes. They indicate that the ESP32 is attempting to connect to a WiFi network with the SSID
  //provided in the separate file
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.disconnect(); //ensures that any existing WiFi connection is disconnected before attempting to connect again

  Serial.printf("Trying to connect to WiFi: %s . . .\n",wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);

  int count=0;

  while (WiFi.status() != WL_CONNECTED) 
  {
    
    count++;
    if (count % 40 == 0)
      Serial.print("\n");
    else
      Serial.print(".");

    delay(25);
    
    /*
    If the connection process exceeds the wifi_timeout duration (in milliseconds), 
    it prints a timeout message and calls esp32_sleep() to put the ESP32 into sleep mode
    */
    if (millis()- start_time > wifi_timeout )  //did wifi fail to connect then time out
    {
      Serial.println("Wifi failed to connect....going into timeout");
      esp32_sleep();
    }
  
  }
  
/*
Output messages to the serial monitor after successfully connecting to the WiFi network.
Displays the time taken to connect, the WiFi network's SSID, the received signal strength (RSSI), and the assigned IP address
*/
  rssi = WiFi.RSSI();
  Serial.println("\n WiFi connected in " +(String)(millis()- start_time ) + "ms to AP:"+wifi_ssid+ " Rssi:"+(String)rssi );
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


/*
  Method ensures that the MQTT client makes repeated connection attempts, handles connection failures gracefully 
  by adding delays between retries, and stops attempting to connect if the maximum retry limit is reached
*/
void reconnect() 
{
  // Loop until we're reconnected
  int retry =0; //keeps track of the number of connection retry attempts
  
  //This loop will continue until the MQTT client (client) is connected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    
    //Attempts to connect to the MQTT broker using the MQTT client's connect() method with client ID specified in file
    if (client.connect(mqtt_clientid)) 
    {
      Serial.println("Connection successful to "+String(mqtt_server));
    } 
    else 
    {
      retry++;
      Serial.print("failed, rc="); //includes the return code (rc) from the MQTT client's state() method, providing information about the connection failure reason
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(3000); //delay of 3000 milliseconds (3 seconds) before attempting the connection again, allowing time for potential network issues to resolve

      //if the maximum retries limit is reached, the function returns, ending further connection attempts.
      if (retry > mqtt_max_retries)
        return;  //end stop trying to connect 
    }
  }
}

// This function is responsible for establishing connections to both the WiFi network and the MQTT broker
int connect_WIFI_MQTT()
{
   //Now Setup WIFI and MQTT connections -   
   setup_wifi(); //Calls the setup_wifi() function to initialize and connect to the WiFi network
   client.setServer(mqtt_server, 1883); //Sets the MQTT server and port using client.setServer(mqtt_server, 1883)

  //If the client is not connected, it calls the reconnect() function to attempt reconnecting to the MQTT broker
  if (!client.connected()) 
  {
    reconnect();
  }
  /*
  Runs the MQTT client's loop function (client.loop()), which is essential for maintaining communication with the 
  MQTT broker, handling incoming messages, and managing the connection.
  */
  client.loop();
  
  return true; //indicates that the WiFi and MQTT connections were successfully established or re-established
}

/* 
Formatting function to convert millis into h:m:s 
Takes an unsigned long integer parameter ms, which represents time in milliseconds.
Stored in the total_time_awake character array
*/
void runtime(unsigned long ms)
{
  unsigned long runMillis = ms; 
  unsigned long allSeconds = runMillis/1000; //converting milliseconds to seconds
  int runHours = allSeconds / 3600; //converts milliseconds to get the number of hours
  int secsRemaining = allSeconds % 3600; //represents remaining seconds after calculating runHours
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;
  sprintf(total_time_awake,"%02d:%02d:%02d", runHours, runMinutes, runSeconds);
}

void esp32_sleep()
{
  /*
  Calculates the total time the ESP32 has been awake by adding the difference between the current time (millis()) and the 
  time when the function started (currentMillis) to the existing time_awake_millis variable
  */
  time_awake_millis = time_awake_millis + (millis() - currentMillis);

  last_doorState = digitalRead(doorSensorPIN);  //stores the last known door state. This SHOULD be CLOSED
  //Serial.printf("\n DoorState:  %d Last Door State:   %d  Awake ms: %ld", doorState, last_doorState, time_awake_millis);
  Serial.println("\nPutting Device to Sleep");  


  /*
  When a wakeup event occurs, the ESP32 wakes up from deep sleep and resumes execution from the point
  immediately after the esp_deep_sleep_start() call 
  */
  esp_deep_sleep_start(); //Enter deep sleep

  // serves as an example to demonstrate that code following the deep sleep call won't run because 
  // the ESP32 will have entered deep sleep mode and stopped executing instructions
  Serial.println("You will never see this");
}



/*******START OF MAIN CODE HERE*******/
void setup() 
{
  Serial.begin(115200);
  delay(50);
  
  //Starts a timer (currentMillis) to measure how long the microcontroller has been awake since the start of the program
  currentMillis = millis();
  
  //pull-up resistor ensures that the pin reads a logical high (usually represented as '1') when no external 
  //signal is connected, preventing the pin from floating and providing a default state.
  pinMode(GPIO_INPUT_IO_TRIGGER,INPUT_PULLUP);
  gpio_pullup_en(GPIO_INPUT_IO_TRIGGER); //ensures that the pull-up resistor is actively turned on for the specified pin
  gpio_pulldown_dis(GPIO_INPUT_IO_TRIGGER); //disabling the pull-down resistor ensures that the pin is not pulled down when it's in the input mode with pull-up enabled

 //What state is door in
  doorState = digitalRead(doorSensorPIN);

  ++bootCount; //keeps track of how many times the device has been booted since it was last powered on or reset

  /*
  print out various diagnostic messages to the Serial Monitor. They display the current boot count, the number of times the door has been stuck open (stuckbootCount), 
  the last known state of the door (last_doorState), and the current door state (doorState).
  */
  //Serial.println("Boot counter: " + String(bootCount));
  //Serial.println("Boot Stuck Counter: " + String(stuckbootCount));
  //Serial.println("Last Door: "+ String(last_doorState));
  //Serial.println("DoorState: "+ String(doorState));
  
  // If they are the same, it implies that the door might be stuck open, triggering an interrupt.
  if (last_doorState == doorState) 
  {
    stuckbootCount++;

    if (stuckbootCount > MAX_STUCK_BOOT_COUNT)  //door is still stuck open we need to sleep on a timer
    {
      client.publish("/sensor/door_status", message_door_stuck, false);
      Serial.println("Max stuck open count door reached ");
      Serial.printf("\nPutting device into a TIMER WAKEUP of %d secs. \n",(TIMER_SLEEP_MICROSECS/1000000));

      // configures the ESP32 to sleep for a specified duration (TIMER_SLEEP_MICROSECS) using a timer wakeup interrupt
      esp_sleep_enable_timer_wakeup(TIMER_SLEEP_MICROSECS); //configured to wake up in 30 minutes 
      esp32_sleep();
    }
    else 
    {
      delay(5000);  //delay of 5 seconds    
      
      esp_sleep_enable_ext0_wakeup(GPIO_INPUT_IO_TRIGGER,GPIO_DOOR_OPEN_STATE); //configures ESP32 to wake up from sleep when door sensor detects open state
      esp32_sleep();
    }
  }

  //checking if the door was stuck open but is now closed, and if so, it resets the door status and sends a message to indicate that the door is closed
  if (stuckbootCount > MAX_STUCK_BOOT_COUNT)
  {
    // returns true, indicating that the ESP32 has successfully connected to the Wi-Fi and MQTT broker
    if (connect_WIFI_MQTT())
    {
      client.publish("/sensor/door", String(doorState).c_str(), false);  //false means don't retain messages
      client.publish("/sensor/door_status",message_door_closed, false);  //text version of door
    }
        
    stuckbootCount = 0; // indicates that the door has been successfully closed after being stuck open for multiple boot cycles
  }

     
  //Wake up when it goes high - may be inverted for your reed door sensors
  esp_sleep_enable_ext0_wakeup(GPIO_INPUT_IO_TRIGGER,GPIO_DOOR_OPEN_STATE); //1 = High, 0 = Low  wake door OPEN (magnet away sensor)
 
  
  //Print the wakeup reason for ESP32
  //print_wakeup_reason();
 
  if(doorState != GPIO_DOOR_CLOSED_STATE) //asking if the door is not closed
  {
   
  
    connect_WIFI_MQTT(); //establishes a connection to the WiFi in order to connect to the MQTT broker
   
    //THIS IS THE MAIN MESSAGE TELLING US THAT THE DOOR WAS OPENED
    client.publish("/sensor/door", String(doorState).c_str(), false);  //false signifies to not retain the messages
    client.publish("/sensor/door_status",message_door_open, false);

    //client.publish("/sensor/bootcount",String(bootCount).c_str(), false); 
    //client.publish("/sensor/name", mqtt_clientid , false);  //client id
    //client.publish("/sensor/rssi",String(rssi).c_str() , false);   // wifi rssi = signal strenght 0 - 100 , <-80 is poor


    
    long  n = 0; //loop counter while door is opened
    while (doorState == GPIO_DOOR_OPEN_STATE)  //while where open
    {
      n++;
      client.loop();  //maintains the connection to the MQTT broker
       
      doorState = digitalRead(doorSensorPIN); //checks whether the door is still open
      
      //prints the current state of the door sensor to the serial monitor
      Serial.print(doorState);
      
      //calculates the elapsed time since the loop started running. It's used to determine if the door has been open for too long
      long elapsed_time = (millis() - start_time); 
      
      //Every 40 cycles (if (n % 40 == 0)), the code prints the elapsed time to the serial monitor. This helps in tracking how long the door has been open
      if (n % 40 == 0)      
        Serial.printf("%ld ms \n", elapsed_time);  

      delay(50); //set up a delay to maintain stability


      //If elapsed time exceeds threshold, assumes that the door is stuck open and sends a message
      if (elapsed_time > MAX_OPENDOOR_TIME)
      {
        Serial.printf("\n Door STUCK OPEN for %ld ms > %d ms .. ending loop. ", elapsed_time, MAX_OPENDOOR_TIME); //Prints to serial monitor
        client.publish("/sensor/door_status", message_door_stuck, false);  //text version of door
              
        break;
      }
    }

    //calculate total_time_awake  how long the esp32 board was running
    runtime(millis() - currentMillis + time_awake_millis); 
    Serial.println("ESp32 was awake for total ::" + (String)total_time_awake);     

    //this is insurance to check if the door is really closed. We might have fallen out of while loop   
    //if (doorState == LOW)  
      //client.publish("/sensor/door_status",message_door_closed,false);  //text version of /sensor/door
    
    //client.publish("/sensor/total_time_awake",total_time_awake,false);
    //int result = client.publish("/sensor/door", String(doorState).c_str(), false);  //door status
    
    delay(250); //gives the MQTT broker some time to receive and process the messages before disconnecting from the WiFi network and MQTT broker
   
    client.disconnect();   //Disconnect MQTT
    WiFi.disconnect();    //Disconnect from the WIFi
    Serial.println("\n Wifi Disconnected from : "+ (String)wifi_ssid);
  
     
  }
  else
    Serial.println("\n State of the door has not changed. Did not attempt to connect to WiFi");

  //Go to sleep now
  client.publish("/sensor/door_status",message_door_closed, false);
  esp32_sleep();
  Serial.println("This will never be printed"); //Insurance to show that the device did in fact go to sleep
 
}  //END OF SETUP CODE


void loop() 
{

}  //END OF LOOP CODE