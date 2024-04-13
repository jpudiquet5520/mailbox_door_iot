#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <time.h>
/*
 * wifi_info.h includs wifi ssid, passwords, mqtt broker info, customize included wifi_info_sample.h and
 * save as wifi_info.h in this same folder.
 */
#include "wifi_info.h"  // contains ssid and access credentials,sms, mqtt broker configs separate file for GitHub

#define LED_ENABLED false  //do we want to turn on the on-board LED when connected to WiFi?
#define MAX_OPENDOOR_TIME 30000   //default 30s in milliseconds how long to wait while door is open to consider it stuck open
#define MAX_STUCK_BOOT_COUNT 5  // If the door is stuck for more than x times let's switch to timer interrupt to save battery
#define TIMER_SLEEP_MICROSECS 1800 * 1000000  //when on timer interrupt how long to sleep in seconds * microseconds

// /sensor/door_status messages for open closed and stuck
#define  message_door_open "Mailbox Opened!"
#define  message_door_closed ""
#define  message_door_stuck "Mailbox_STUCK_OPEN!"

// Define the the MQTT and WiFI client variables
WiFiClient espClient;
PubSubClient client(espClient);
HTTPClient http;  //for posting
//Define the door Sensor PIN and initial state (Depends on your reed sensor N/C  or N/O )
gpio_num_t  doorSensorPIN = GPIO_NUM_12;  // Reed GPIO PIN: //CHANGE TO WHATEVER PINOUT WE END UP USING
gpio_num_t GPIO_INPUT_IO_TRIGGER = doorSensorPIN;
int GPIO_DOOR_CLOSED_STATE= LOW ;  //Default state when the reed and magent are next to each other (depends on reed switch)
int GPIO_DOOR_OPEN_STATE = !GPIO_DOOR_CLOSED_STATE;  //Open state is oposite f closed

//RTC Memory attribute is retained across resets
RTC_DATA_ATTR long bootCount = 0;
RTC_DATA_ATTR long stuckbootCount = 0;  //if the door is stuck open increment this counter
RTC_DATA_ATTR long last_doorState=-99;  // will store last door state to detect stuck open
RTC_DATA_ATTR long time_awake_millis=0;  //total awake time in milliseconds - useful for tracking how long battery lasts

long currentMillis;  //timer start of awake
int doorState=LOW;   //maintains  the door sensor state usually LOW= 0 or 1
int start_time=0;  //timing for when esp32 is active (not sleeping)
char total_time_awake[21];  //timing holds string h:m:s for time door is opened.
long rssi =0;

void setup_wifi() {
    start_time=millis();
 
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(wifi_ssid);
    WiFi.disconnect();

    Serial.printf("Trying to connec to WiFi: %s, p/w: %s ... \n",wifi_ssid,wifi_password);
    WiFi.begin(wifi_ssid, wifi_password);

    int count=0;

    while (WiFi.status() != WL_CONNECTED) {
    
        count++;
    if ( count % 40==0 )
        Serial.print("\n");
    else
        Serial.print(".");

    delay(25);
    
    if (millis()- start_time > wifi_timeout )  //did wifi fail to connect then time out
    {
        Serial.println("Wifi Failed to connect ..timeout..");
        //esp32_sleep();
    }
    
}
 
    rssi= WiFi.RSSI();
    Serial.println( "\n WiFi connected in " +(String)(millis()- start_time ) + "ms to AP:"+wifi_ssid+ " Rssi:"+(String)rssi );
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


void setup() {
    // Your setup code here
    Serial.begin(115200); // Set the baud rate (data rate over serial communication) to match baud rate in serial monitor
    delay(1000); //Delay for the serial communication to stabalize
    //TODO: NEED TO COMMENT THIS OUT, FOR TROUBLESHOOTING PURPOSES
    //Needed to call the setup_wifi() function that is why we couldn't 
    //setup_wifi();  
    
    Serial.println("Hello World");
    Serial.println("Serial initialized."); // Check if Serial communication is working
    //setup_wifi();
}

void loop() {
    // Your loop code here
}