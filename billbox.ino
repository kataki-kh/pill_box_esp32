
#include <WiFi.h>
#include "time.h"
#include "sys/time.h"
#include "esp_system.h" ///needed for the timer
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
 #include <Arduino_JSON.h>

///sync server
// Set these to your desired credentials.
const char *ssid = "bill_box";
const char *password = "00000000";
const char* PARAM_INPUT_1 = "reffirance_time";
const char* PARAM_INPUT_2 = "first_bill_name";
const char* PARAM_INPUT_3 = "first_bill_dose";
const char* PARAM_INPUT_4 = "first_bill_time[]";
const char* PARAM_INPUT_5 = "second_bill_name";
const char* PARAM_INPUT_6 = "second_bill_dose";
const char* PARAM_INPUT_7 = "second_bill_time[]";
const char* PARAM_INPUT_8 = "first_bill_count";
const char* PARAM_INPUT_9 = "second_bill_count";

RTC_DATA_ATTR int on_sync =0;
AsyncWebServer server(80);
unsigned int Sync_Timeout=600;///10min =600;
 hw_timer_t *Sync_timer = NULL;///sync timer
 hw_timer_t *Receive_bill_timer = NULL;
 hw_timer_t *Receive_bill_in_sync_routine_timer = NULL;



//
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR int TIME_TO_SLEEP=0;
RTC_DATA_ATTR int Bill_TIME_TO_SLEEP=0;
RTC_DATA_ATTR int Last_Time_Up;/* Time ESP32 will go to sleep (in seconds) default set to one year 31104000*/ 
#define LED_BUILTIN 2 
#define wake_up_button 15
const int Receive_bill_Timeout = 15000; //time in ms to bush the button to recive bills
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int syncTimes = 0;
RTC_DATA_ATTR int no_off_missed_time =0;
RTC_DATA_ATTR int TIME_ON_BOOT;
struct bills{
  String name;
  int douse;
  int count=0;
  int time[];
  
};
struct missed{
  char name[];
  int douse[];
  int time[];
  
};
RTC_DATA_ATTR int reffirance_time=0;///reffirance time
RTC_DATA_ATTR int first_bill_index=0;
RTC_DATA_ATTR int second_bill_index=0;
RTC_DATA_ATTR int missed_bill_index=0;

RTC_DATA_ATTR int bill_caused_to_wakeup_room=0;
RTC_DATA_ATTR int bill_caused_to_wakeup_douse=0;
RTC_DATA_ATTR int bill_caused_to_wakeup_douse_second=0;

RTC_DATA_ATTR int date;//1 or 2
struct  bills first_bill;
RTC_DATA_ATTR struct first_bill;
struct  bills second_bill;
RTC_DATA_ATTR struct second_bill;
struct  missed missed_bill;
RTC_DATA_ATTR struct missed_bill;

/*Hardware Connections for waking it up with the push button
======================for the wake up and controll button
Push Button to GPIO 15 pulled down with a 10K Ohm
resistor to wake up

NOTE:
======
Only RTC IO can be used as a source for external wake
source. They are pins: 0,2,4,12-15,25-27,32-39.
======================for buzzer
buzzer to GPIO 23 pulled down with a 10K Ohm
resistor.
*/
#define BUZZER_BUILTIN 2
#define fast_beep 250
#define slow_beep 500
int beep(int count,int delay_time){
   Serial.println("=============================buzzer " );
 pinMode(BUZZER_BUILTIN, OUTPUT);
 for(int i=0;i<count;i++){
digitalWrite(BUZZER_BUILTIN, HIGH);
//Serial.println("beep: " + String(delay_time));
    delay(delay_time);
digitalWrite(BUZZER_BUILTIN, LOW);
pinMode(BUZZER_BUILTIN, INPUT);
  
  
  
 }

 

  
}
/*
======================for servo
servo terminals(vcc,gnd)connected to the power supply and control connected to GPIO 14 with
a 10K Ohm resistor.

*/
#define SERVO_PIN 14
///servo lib should be here #include <Servo.h>  
#include <Servo_ESP32.h>
#define open_room_1 60 ///to open room 1 how much dose the servo need to rotate
#define open_room_2 120 ///to open room 2 how much dose the servo need to rotate
int servo_open_close_room(int room,int delay_time){
  Serial.println("=============================servo " );
 Servo_ESP32 servo;
 int angle =0;
int angleStep = 5;

int angleMin =0;
int angleMax = 180;
servo.attach(SERVO_PIN);
if(room==1){
  beep(bill_caused_to_wakeup_douse,fast_beep);
servo.write(open_room_1);

  delay(delay_time);
}else if(room==2){
  beep(bill_caused_to_wakeup_douse,fast_beep);
  servo.write(open_room_2);
  
  delay(delay_time);
}else{
  beep(bill_caused_to_wakeup_douse,fast_beep);
  servo.write(open_room_1);
  
 delay(delay_time);
 beep(bill_caused_to_wakeup_douse_second,fast_beep);
servo.write(angleMin);
  servo.write(open_room_2);
  delay(delay_time);

  
}
servo.write(angleMin);
  
  
 
 

  
}
/*
======================for battery moniter
the (+) of the battery connected to R1=100K Ohm resistor and 
R1 connected to GPIO 13 and also connected to R2=100K Ohm resistor that
is connected to the common gnd or the board gnd.

*/
#define BATTERY_PIN 13 
RTC_DATA_ATTR int battery = 100;
void read_battery(){
 
 //devide the battery by the adc 
 Serial.println("=============================battery " );
float battery_voltage=analogRead(BATTERY_PIN);
//Serial.println("pin : " + String(BATTERY_PIN));

//Serial.println("pin read: " + String(battery_voltage));
battery_voltage=battery_voltage* (3.7 / 4095.0);
//Serial.println("battery read: " + String(battery_voltage));
Serial.println("battery percient: " + String((battery_voltage/3.7 )*100));


 battery=  (battery_voltage/3.7 )*100;
//return (battery_voltage/3.7 )*100;
  
  
 
 

  
}
/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
///helper functions
int print_wakeup_reason(int wakeup_reason){
 

  switch(wakeup_reason)
  {
    
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}
/*
 * functions that run with timers
 */
 void set_next_alarm(){

  ///set the reffirance
   int previus_bill_time=0;
  if(Bill_TIME_TO_SLEEP==0){
    
    previus_bill_time=reffirance_time;
  }else{
  previus_bill_time=Bill_TIME_TO_SLEEP;

  }
  ///check if we already reched all the bills
  if((first_bill_index>(sizeof(first_bill.time)) and second_bill_index>(sizeof(second_bill.time)))
  or (first_bill.time[first_bill_index]==0 and second_bill.time[second_bill_index]==0)){
    //something big or forever
    TIME_TO_SLEEP=604800;//sleep for a wake
  }
  else if((first_bill_index>(sizeof(first_bill.time)) and second_bill_index<=(sizeof(second_bill.time)))
  or (first_bill.time[first_bill_index]==0 and second_bill.time[second_bill_index]!=0)){
      Bill_TIME_TO_SLEEP=second_bill.time[first_bill_index];
   
    bill_caused_to_wakeup_room=2;
    bill_caused_to_wakeup_douse=second_bill.douse;
    second_bill_index=second_bill_index+1;
        TIME_TO_SLEEP=Bill_TIME_TO_SLEEP-(((system_get_time()-325114)/1000000)-TIME_ON_BOOT)-previus_bill_time;

  }
    else if((first_bill_index<=(sizeof(first_bill.time)) and second_bill_index>(sizeof(second_bill.time)))
    or (first_bill.time[first_bill_index]!=0 and second_bill.time[second_bill_index]==0)){
 Bill_TIME_TO_SLEEP=first_bill.time[second_bill_index];
    bill_caused_to_wakeup_room=1;
    bill_caused_to_wakeup_douse=first_bill.douse;
    first_bill_index=first_bill_index+1;
    TIME_TO_SLEEP=Bill_TIME_TO_SLEEP-(((system_get_time()-325114)/1000000)-TIME_ON_BOOT)-previus_bill_time;

    }
    else{
  if(first_bill.time[first_bill_index]>second_bill.time[second_bill_index]){
    Bill_TIME_TO_SLEEP=second_bill.time[first_bill_index];
   
    bill_caused_to_wakeup_room=2;
    bill_caused_to_wakeup_douse=second_bill.douse;
    second_bill_index=second_bill_index+1;
  }else if(first_bill.time[first_bill_index]<second_bill.time[second_bill_index]){
    Bill_TIME_TO_SLEEP=first_bill.time[second_bill_index];
    bill_caused_to_wakeup_room=1;
    bill_caused_to_wakeup_douse=first_bill.douse;
    first_bill_index=first_bill_index+1;
  }else{
    Bill_TIME_TO_SLEEP=first_bill.time[first_bill_index];
     bill_caused_to_wakeup_room=3;
  bill_caused_to_wakeup_douse=first_bill.douse;
  bill_caused_to_wakeup_douse_second=second_bill.douse;
  second_bill_index=second_bill_index+1;
  first_bill_index=first_bill_index+1;
  }
    TIME_TO_SLEEP=Bill_TIME_TO_SLEEP-(((system_get_time()-325114)/1000000)-TIME_ON_BOOT)-previus_bill_time;

    }
 
 
}
void IRAM_ATTR MissedBill() {
  ets_printf("\n");

 // ets_printf("reboot\n");
 // esp_restart();
 //here we should start the miss bill sequance
 //first we find witch room cousing the timer 1 or 2 or 3=both // bill_caused_to_wakeup_room//bill_caused_to_wakeup_douse//TIME_TO_SLEEP
 //then adding the bill name and douses and times to missed struct
   //find the time
  int missed_time=Bill_TIME_TO_SLEEP;
  String missed_first_name=first_bill.name;
   String missed_second_name=second_bill.name;
  int missed_first_douse=first_bill.douse;
  int missed_second_douse=second_bill.douse;

 
     missed_bill.time[missed_bill_index]= missed_time;
     missed_bill_index+1;
    
  
 
if(bill_caused_to_wakeup_room==3){

 
 remove_bill(1,1);
  remove_bill(2,1);
 
}else if(bill_caused_to_wakeup_room==1){
   remove_bill(1,1);
 
}else if(bill_caused_to_wakeup_room==2){

  remove_bill(2,1);
  
}
 
 //reareanging the bill structures removing the first item in the array of time and deciding which alarm next room1 or room2 or both and set the sleep time and missed times and put the device to sleep
set_next_alarm();
no_off_missed_time+1;
ets_printf("bill missed now going to sleep");
Last_Time_Up=(system_get_time()-325114)/1000000;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  ets_printf("Setup ESP32 to sleep ");

  esp_deep_sleep_start();
}

/// messed=1,0
void remove_bill(int room,int messed){
  if(room==1){
  if(messed==1){
    first_bill.count=first_bill.count-first_bill.douse;

}
  first_bill_index+1;
    
  }else if(room==2){
 if(messed==1){
  second_bill.count=second_bill.count-second_bill.douse;

}
  second_bill_index+1;
  
}


}
//the sync server is here
void IRAM_ATTR Stop_sync(){
 
   ets_printf("sync stop\n");
  //ets_printf("Going to sleep now");
 
 

on_sync=0;

}
 
  ///the real server goes here
  void sync_routine(int waiting_time){
    ///timer for recive bill inside sync routine
    if(Sync_Timeout>(TIME_TO_SLEEP-TIME_ON_BOOT) and TIME_TO_SLEEP>0 and syncTimes>0){
      
   
 Receive_bill_in_sync_routine_timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(Receive_bill_in_sync_routine_timer, &receive_bill_in_sync, true);  //attach callback
  timerAlarmWrite(Receive_bill_in_sync_routine_timer,(TIME_TO_SLEEP-TIME_ON_BOOT)* 1000, false); //set time in us
  timerAlarmEnable(Receive_bill_in_sync_routine_timer);
 
  timerWrite(Receive_bill_in_sync_routine_timer, 0);
 }
    ///
    Serial.println("Configuring access point...");

on_sync=1;


WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();
    Serial.println("Server started");
   
    if(waiting_time==0){
      Sync_Timeout=604800;
    }
     

 Sync_timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(Sync_timer, &Stop_sync, true);  //attach callback
  timerAlarmWrite(Sync_timer, Sync_Timeout * 1000, false); //set time in us
  timerAlarmEnable(Sync_timer);
 
  timerWrite(Sync_timer, 0);
  
   server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request){
    String inputMessage;
    String inputParam;
    String response="this is the first sync";
    if(syncTimes>0 or first_bill_index>0 or second_bill_index>0){
      ///missed_bill_time
      ///missed_bill_douse
      ///missed_bill_name
       JSONVar myObject;
       myObject["missed_bill_time"] = missed_bill.time;
         myObject["missed_bill_douse"] = missed_bill.douse;
        myObject["missed_bill_name"] = missed_bill.name;
        myObject["battery"] =battery;
        String jsonString = JSON.stringify(myObject);
     response=jsonString;
    }else{
      response="this is the first sync";
    }
   
      request->send(200, "text/json",response);
      if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
     reffirance_time= request->getParam(PARAM_INPUT_1)->value().toInt();
               Serial.println("reffirance_time"+String(reffirance_time));

    }
    if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
      inputParam = PARAM_INPUT_2;
      
       first_bill_index=0;
     first_bill.name=inputMessage;
     
        
    }
    if (request->hasParam(PARAM_INPUT_3)) {
      inputMessage = request->getParam(PARAM_INPUT_3)->value();
      inputParam = PARAM_INPUT_3;
     first_bill.douse=request->getParam(PARAM_INPUT_3)->value().toInt();
        
    }
    if (request->hasParam(PARAM_INPUT_4)) {
      ///the array
      inputMessage =request->getParam(PARAM_INPUT_4)->value();
     JSONVar myArray = JSON.parse(inputMessage);
      for(int i=0;i<sizeof(first_bill.time);i++){
        if(i<=myArray.length()){
          first_bill.time[i]=(int) myArray[i];
        }else{
          first_bill.time[i]=0;
        }
     
        
      }
      ///add bills if any more
      if(first_bill.count!=request->getParam(PARAM_INPUT_8)->value().toInt()){
        first_bill.count=request->getParam(PARAM_INPUT_8)->value().toInt();
        beep(1,slow_beep);
        beep(3,fast_beep);
        beep(1,slow_beep);
        servo_open_close_room(1,30000);//set to 5 seconds =5000
      }
//              / Serial.println("first_bill.time[i]"+String((int) myArray[0]));

    }
    if (request->hasParam(PARAM_INPUT_5)) {
      inputMessage = request->getParam(PARAM_INPUT_5)->value();
      inputParam = PARAM_INPUT_5;
      second_bill_index=0;
       
     second_bill.name=inputMessage;
        
    }
    if (request->hasParam(PARAM_INPUT_6)) {
      inputMessage = request->getParam(PARAM_INPUT_6)->value();
      inputParam = PARAM_INPUT_6;
    second_bill.douse=request->getParam(PARAM_INPUT_6)->value().toInt();
        
    }
    
     if (request->hasParam(PARAM_INPUT_7)) {
      ///the array
            inputMessage =request->getParam(PARAM_INPUT_7)->value();

      JSONVar myArray = JSON.parse(inputMessage);
      for(int i=0;i>myArray.length();i++){

         if(i<=myArray.length()){
          second_bill.time[i]=(int) myArray[i];
        }else{
          second_bill.time[i]=0;
        }
       
      }
      ///add bills if any more
      ///add bills if any more
      if(first_bill.count!=request->getParam(PARAM_INPUT_9)->value().toInt()){
        first_bill.count=request->getParam(PARAM_INPUT_9)->value().toInt();
        beep(1,slow_beep);
        beep(3,fast_beep);
        beep(1,slow_beep);
        servo_open_close_room(2,30000);//set to 5 seconds =5000
      }
      // Serial.println("second_bill.time[i]"+String((int) myArray[0]));
    }
   
   
    ///empty the missed array
    
    missed_bill_index=0;
    
    syncTimes=syncTimes+1;
          Serial.println("sync done");
          timerStop(Sync_timer);
          //timerStop(Receive_bill_in_sync_routine_timer);
        
         Stop_sync();
         
   
  });
   
  
 
  
}


//
void receive_bill_routine(){
   /*
 * timers definition
 */

 
Receive_bill_timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(Receive_bill_timer, &MissedBill, true);  //attach callback
  timerAlarmWrite(Receive_bill_timer, Receive_bill_Timeout * 1000, false); //set time in us
  timerAlarmEnable(Receive_bill_timer);  


 ///on the device wake up one long beep to indecate it't waiting
    beep(1,slow_beep);
    beep(3,fast_beep);
    beep(1,slow_beep);
     ///just say we got a new bill every wake up changing the next wake up time by next bill seconds seconds
     //wait for the user to press the button if not then just flagthis bill as missed
     //Iam wasing wachdog timer to check if the button is pressed within the allowed time
     timerWrite(Receive_bill_timer, 0);
      Serial.println("now bill="+String(bill_caused_to_wakeup_room));
   

    pinMode(wake_up_button, INPUT);  
      while (!digitalRead(wake_up_button)==1) {
    Serial.println("button not pressed");
   // delay(500); no need for the delay actually
        }
        //here we should stop the timer so that the button is pushed no need for it
       Serial.println("time took to push the button"+String(timerReadSeconds(Receive_bill_timer)));
       timerStop(Receive_bill_timer);
       Serial.println("timer should be stoped here");
       ///here we should initiate the empty sequance first alert then open room for 10 seconds then remove bill then set_next_alarm
     //servo_open_close_room
    if(bill_caused_to_wakeup_room==1 or bill_caused_to_wakeup_room==2){
      servo_open_close_room(bill_caused_to_wakeup_room,5000);//set to 5 seconds =5000
     remove_bill(bill_caused_to_wakeup_room,0);
     set_next_alarm();
    }else if(bill_caused_to_wakeup_room==3){
      servo_open_close_room(1,5000);//set to 5 seconds =5000
     remove_bill(1,0);
     servo_open_close_room(2,5000);//set to 5 seconds =5000
     remove_bill(2,0);
     set_next_alarm();
    }
     

  /*
   * 
   */
}

void receive_bill_in_sync(){
  Stop_sync();
  timerStop(Receive_bill_in_sync_routine_timer);
  receive_bill_routine();
}
void setup(){
     //should be removed
   /*  first_bill.name="test";
     first_bill.douse=2;
     first_bill.time[0]=7;
     first_bill.time[1]=8;
      second_bill.name="test2";
     second_bill.douse=3;
     second_bill.time[0]=5;
     second_bill.time[1]=10;
     TIME_TO_SLEEP=5;*/
     //
  ///get time now
  int seconds=(system_get_time()-325114)/1000000;//325114
  TIME_ON_BOOT=seconds;
 
  Serial.begin(115200);///put it to 500000 in production
   Serial.println("time slept:"+String(seconds));
   esp_sleep_wakeup_cause_t wakeup_reason;
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_15,1);//0 = low 1 ==high
   
  wakeup_reason = esp_sleep_get_wakeup_cause();
   read_battery();
  pinMode(LED_BUILTIN, OUTPUT);

 
  
  delay(500); //Take some time to open up the Serial Monitor ///could be stoped in production
 
 if(bootCount%2==0){
    digitalWrite(LED_BUILTIN, HIGH);
    
  }else{
    digitalWrite(LED_BUILTIN, LOW);
    
  }
  ///if it's the first boot then we inter sync mode with wifi until the sync happen
  if(bootCount==0 and syncTimes==0){
     Serial.println("first boot");
    ///here create server and send box id and wait to request to fill the structures
    //syncTimes+1;
        sync_routine(Sync_Timeout);
        while(on_sync>0){///we are just looping until sync is done or the sync timeoute
       Serial.println("sync_on="+String(on_sync));
       //delay(100);
    }
///make sure to stop wifi and bluetooth

 WiFi.mode(WIFI_OFF);
btStop();

    ///go back to sleep after setting the next alarm
    if(syncTimes>0){
      
   
 set_next_alarm();
   }else{
    TIME_TO_SLEEP=604800;//sleep for a week
   }
    Last_Time_Up=(system_get_time()-325114)/1000000;

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
 Serial.println("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +" Seconds");
 Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
         

      

  }
  //Increment boot number and print it every reboot
  ++bootCount;
 
    Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason(wakeup_reason);
  ///if it wake up because of the timmer then we go to the empty proccess
  if(wakeup_reason==ESP_SLEEP_WAKEUP_TIMER and bootCount>1){




      receive_bill_routine();
   
    

    
  }
  ///if it wake up because of the timmer then we go to the connection&sync proccess
  else if(wakeup_reason==ESP_SLEEP_WAKEUP_EXT0){
    ///on the device wake up one long beep to indecate it't waiting
    beep(1,slow_beep);
    sync_routine(Sync_Timeout);
    while(on_sync>0){///we are just looping until sync is done or the sync timeoute
       Serial.println("sync_on="+String(on_sync));
       //delay(100);
    }
   
  }///wake up was not coused by sleep so the battery down
  else if(wakeup_reason==0 and seconds==0 and bootCount==1  ){
    Serial.println("shittt:"+String(seconds));
///do something to stop it from go to sleep and indecate that you need to connected to the app
      sync_routine(Sync_Timeout);
        while(on_sync>0){///we are just looping until sync is done or the sync timeoute
       Serial.println("sync_on="+String(on_sync));
       //delay(100);
    }
///make sure to stop wifi and bluetooth
 WiFi.mode(WIFI_OFF);
btStop();

    ///go back to sleep after setting the next alarm
    if(syncTimes>0){
      
   
 set_next_alarm();
   }else{
    TIME_TO_SLEEP=604800;//sleep for a wake
   }
    Last_Time_Up=(system_get_time()-325114)/1000000;

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
 Serial.println("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +" Seconds");
 Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start(); 
  }
  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
 
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +" Seconds");

  /*
  Next we decide what all peripherals to shut down/keep on
  By default, ESP32 will automatically power down the peripherals
  not needed by the wakeup source, but if you want to be a poweruser
  this is for you. Read in detail at the API docs
  http://esp-idf.readthedocs.io/en/latest/api-reference/system/deep_sleep.html
  Left the line commented as an example of how to configure peripherals.
  The line below turns off all RTC peripherals in deep sleep.
  */
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //Serial.println("Configured all RTC Peripherals to be powered down in sleep");

 
  
  if(no_off_missed_time<3 and on_sync!=1){
    
  Serial.println("Going to sleep now");
  Serial.flush(); 
 ///make sure to stop wifi and bluetooth
WiFi.mode(WIFI_OFF);
btStop();
Last_Time_Up=(system_get_time()-325114)/1000000;
  esp_deep_sleep_start();
  }else if(no_off_missed_time==3 and on_sync!=0){
    //run the wifi waiting for connection and beep beep
    Serial.println("no_off_missed_time =3");
     beep(5,fast_beep/2);
     ///here we should open wifi
     sync_routine(0);
  }
  //Serial.println("This will never be printed");
}

void loop(){
  //This is not going to be called
}
