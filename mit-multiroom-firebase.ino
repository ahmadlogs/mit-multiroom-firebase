/*  *********************************************************************************
 *  Created By: Tauseef Ahmad
 *  Created On: 10 April, 2022
 *  
 *  YouTube Video: https://youtu.be/DshR6Y9aTSs
 *  My Channel: https://www.youtube.com/channel/UCOXYfOHgu-C-UfGyDcu5sYw/
 *  
 *  *********************************************************************************
 *  Preferences--> Aditional boards Manager URLs : 
 *  For ESP32: (Version 1.0.6)
 *  https://dl.espressif.com/dl/package_esp32_index.json
 *  
 *  For ESP8266: (Version 3.0.2)
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  *********************************************************************************
 *  Firebase ESP Client Library (Version 3.1.5)
 *  https://github.com/mobizt/Firebase-ESP-Client
 *  *********************************************************************************/
 
 //-----------------------------------------------------------------------
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Firebase_ESP_Client.h>
//-----------------------------------------------------------------------
//Provide the token generation process info.
#include <addons/TokenHelper.h>

//Provide the RTDB payload printing 
//info and other helper functions.
#include <addons/RTDBHelper.h>
//-----------------------------------------------------------------------
/* 1. Define the WiFi credentials */
#define WIFI_SSID "ENTER_YOUR_WIFI_SSID"
#define WIFI_PASSWORD "ENTER_YOUR_WIFI_PASSWORD"
//-----------------------------------------------------------------------
/* 2. Define the API Key */
#define API_KEY "REPLACE_WITH_YOUR_FIREBASE_PROJECT_API_KEY"
//-----------------------------------------------------------------------
/* 3. Define the RTDB URL */
#define DATABASE_URL "REPLACE_WITH_YOUR_FIREBASE_DATABASE_URL"
//-----------------------------------------------------------------------
String room_no = "room1";
//-----------------------------------------------------------------------
// define the GPIO connected with Relays and switches
#define Relay1  16 //D0
#define Relay2   5 //D1
#define Relay3   4 //D2
#define Relay4   0 //D3

#define Switch1 14 //D5
#define Switch2 12 //D6
#define Switch3  9 //SD2
#define Switch4 10 //SD3

#define WIFI_LED 2

int stateRelay1=0, stateRelay2=0, stateRelay3=0, stateRelay4=0;
//-----------------------------------------------------------------------

/* Uncomment only if, you have selected the email authentication from 
firebase authentication settings */
//#define USER_EMAIL "ENTER_USER_EMAIL"
//#define DATABASE_URL "USER_PASSWORD"
//-----------------------------------------------------------------------

/***********************************************************
 0. complete_path = /room1/L1 or /room1/L2 etc.
    this is the complete path to firebase database
 1. stream_path = /room1
    this is the top nodes of firebase database
***********************************************************/
String stream_path = "";
/***********************************************************
 2. event_path = /L1 or /L2 or /L3 or /L4
    this is the data node in firbase database
***********************************************************/
String event_path = "";
/***********************************************************
 3. stream_data - use to store the current command to
    turn ON or OFF the relay 
***********************************************************/
String stream_data = "";
/***********************************************************
 5. jsonData - use to store "all the relay states" from 
    firebase database jsonData object used only once when 
    you reset the nodemcu or esp32 check the following:
    else if(event_path == "/") in the loop() function
***********************************************************/
FirebaseJson jsonData;

/***********************************************************
 it becomes TRUE when data is changed in Firebase
 used in streamCallback function
***********************************************************/
volatile bool dataChanged = false;

/***********************************************************
 this variable is based on the authentication method you 
 have selected while making firebase database project.
 authentication method: anonymus user
***********************************************************/
bool signupOK = false;

/***********************************************************
 resetPressed variable is used only once when you
 pressed the reset button. it is used to send test data to
 Firebase database. If we will not test send data then the 
 project will not work. used in the loop function.
***********************************************************/
bool resetPressed = true;

/***********************************************************
 when there is some data to upload to theFirebase
 then the value of uploadBucket will TRUE. This
 variable is used in listenSwitches() function
***********************************************************/
bool uploadBucket = false;

//i.e bucketData = "1" or "0"
//i.e bucketPath = "L1" or "L2" etc.
String bucketData = "", bucketPath = "";

//-----------------------------------------------------------------------
//Define Firebase Data object
FirebaseData stream;
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;


/******************************************************************************************
 * void streamCallback(FirebaseStream data)
 * This function execute automatically 
 * 1. when you press the reset button of the microcontroller. 
 * 2. when any data is changed in the firebase basebase.
 * microcontroller. 
 ******************************************************************************************/
void streamCallback(FirebaseStream data)
{
  stream_path = data.streamPath().c_str();
  event_path = data.dataPath().c_str();
  
  if(String(data.dataType().c_str()) == "json"){
    jsonData = data.to<FirebaseJson>(); 
  }else{
    //intData(), floatData()
    stream_data = data.stringData();
  }
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                stream_path,
                event_path,
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  //This is the size of stream payload received (current and max value)
  //Max payload size is the payload size under the stream path since 
  //the stream connected and read once and will not update until 
  //stream reconnection takes place. This max value will be zero 
  //as no payload received in case of ESP8266 which
  //BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", 
                data.payloadLength(), data.maxPayloadLength());

  //Due to limited of stack memory, do not perform any task that 
  //used large memory here especially starting connect to server.
  //Just set this flag and check it status later.
  dataChanged = true;
}

/******************************************************************************************
 * void streamTimeoutCallback(bool timeout)
 ******************************************************************************************/
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), 
                  stream.errorReason().c_str());
}


/******************************************************************************************
 * void setup()
 ******************************************************************************************/
void setup()
{
  Serial.begin(115200);
  //-----------------------------------------------------------------------
  pinMode(Relay1, OUTPUT); digitalWrite(Relay1, LOW);  
  pinMode(Relay2, OUTPUT); digitalWrite(Relay2, LOW);
  pinMode(Relay3, OUTPUT); digitalWrite(Relay3, LOW);
  pinMode(Relay4, OUTPUT); digitalWrite(Relay4, LOW);
  
  pinMode(WIFI_LED, OUTPUT); 
  
  pinMode(Switch1, INPUT_PULLUP);
  pinMode(Switch2, INPUT_PULLUP);
  pinMode(Switch3, INPUT_PULLUP);
  pinMode(Switch4, INPUT_PULLUP);
  
  //-----------------------------------------------------------------------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  //-----------------------------------------------------------------------
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;
  
  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  //-----------------------------------------------------------------------
  /*Or Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Firebase signUp ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  //-----------------------------------------------------------------------

  /*Or Assign the user sign in credentials */
  //auth.user.email = USER_EMAIL;
  //auth.user.password = USER_PASSWORD;
  
  //Or use legacy authenticate method
  //config.database_url = DATABASE_URL;
  //config.signer.tokens.legacy_token = "<database secret>";
  //-----------------------------------------------------------------------
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  //-----------------------------------------------------------------------
  //Recommend for ESP8266 stream, adjust the buffer size to match your stream data size
  #if defined(ESP8266)
    stream.setBSSLBufferSize(2048 /* Rx in bytes, 512 - 16384 */, 512 /* Tx in bytes, 512 - 16384 */);
  #endif
  //-----------------------------------------------------------------------
  if (!Firebase.RTDB.beginStream(&stream, room_no))
    Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());
  //-----------------------------------------------------------------------
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  //-----------------------------------------------------------------------
}


/******************************************************************************************
 * void loop()
 ******************************************************************************************/
void loop()
{
  //--------------------------------------------------------------------------
  if (WiFi.status() != WL_CONNECTED){ 
    //Serial.println("WiFi Not Connected");
    digitalWrite(WIFI_LED, HIGH); //Turn off WiFi LED
  }
  else{
    //Serial.println("WiFi Connected");
    digitalWrite(WIFI_LED, LOW); //Turn on WiFi LED
  }
  //--------------------------------------------------------------------------
  listenSwitches();
  if (Firebase.ready() && signupOK) {
    if(uploadBucket == true){
      uploadBucket = false;
      String URL = room_no + "/" + bucketPath;
      Serial.println(URL);
      Serial.printf("Set String... %s\n\n", Firebase.RTDB.setString(&fbdo, URL, bucketData) ? "ok" : fbdo.errorReason().c_str());
    }
    if(resetPressed == true){
      resetPressed = false;
      Serial.printf("Set Test String... %s\n\n", Firebase.RTDB.setString(&fbdo, "/test", "0") ? "ok" : fbdo.errorReason().c_str());
    }
    bucketPath = "";
    bucketData = "";

  }
  //--------------------------------------------------------------------------
  if (dataChanged)
  {
    dataChanged = false;
    Serial.println("dataChanged");
    //When stream data is available, do anything here...
    //____________________________________________________________________
    //delete \ and " from the data_stream string
    // \"2\"
    stream_data.replace("\\\"", "");
    //____________________________________________________________________
    if(event_path == "/L1"){
      Serial.println("relay1:"+stream_data);
      stateRelay1 = stream_data.toInt();
      digitalWrite(Relay1, stateRelay1);
    }
    //____________________________________________________________________
    else if(event_path == "/L2"){
      Serial.println("relay2:"+stream_data);
      stateRelay2 = stream_data.toInt();
      digitalWrite(Relay2, stateRelay2);
    }
    //____________________________________________________________________
    else if(event_path == "/L3"){
      Serial.println("relay3:"+stream_data);
      stateRelay3 = stream_data.toInt();
      digitalWrite(Relay3, stateRelay3);
    }
    //____________________________________________________________________
    else if(event_path == "/L4"){
      Serial.println("relay4:"+stream_data);
      stateRelay4 = stream_data.toInt();
      digitalWrite(Relay4, stateRelay4);
    }
    //____________________________________________________________________
    else if(event_path == "/"){
      //this if statement executes only once if you reset the nodemcu or esp32.
      //The datachange event automatically occurs when you reset microcontroller
      //jsonData object is used in the below called function
      reloadRelayStates();
    }
    //____________________________________________________________________
    stream_data = "";
  }
  //--------------------------------------------------------------------------
}


/******************************************************************************************
 * void reloadRelayStates()
 * This function execute
 * 1. when you press the reset button of the microcontroller. OR
 * 2. when data is received in json format
 ******************************************************************************************/
void reloadRelayStates(){
  //jsonData.toString(Serial, true);
  String strKey, strVal;
  //_________________________________________________________
  size_t count = jsonData.iteratorBegin();
  for (size_t i = 0; i < count; i++){
    FirebaseJson::IteratorValue value = jsonData.valueAt(i);
    //-----------------------------------
    strVal = value.value.c_str();
    strVal.replace("\\", "");
    strVal.replace("\"", "");
    strKey = value.key.c_str();
    Serial.println("strKey:"+strKey);
    Serial.println("strVal:"+strVal);
    //-----------------------------------
    if(strKey == "L1"){
      stateRelay1 = strVal.toInt();
      digitalWrite(Relay1, stateRelay1);
      //Serial.print("relay1:");Serial.println(stateRelay1);
    }
    else if(strKey == "L2"){
      stateRelay2 = strVal.toInt();
      digitalWrite(Relay2, stateRelay2);
      //Serial.print("relay2:");Serial.println(stateRelay2);
    }
    else if(strKey == "L3"){
      stateRelay3 = strVal.toInt();
      digitalWrite(Relay3, stateRelay3);
      //Serial.print("relay3:");Serial.println(stateRelay3);
    }
    else if(strKey == "L4"){
      stateRelay4 = strVal.toInt();
      digitalWrite(Relay4, stateRelay4);
      //Serial.print("relay4:");Serial.println(stateRelay4);
    }
  }
  //required for free the used memory in iteration (node data collection)
  jsonData.iteratorEnd();
  jsonData.clear();
  //_________________________________________________________
}



void listenSwitches()
{
  //String URL = room_no;
  if(digitalRead(Switch1) == LOW){
    stateRelay1 = !stateRelay1;
    digitalWrite(Relay1, stateRelay1);
    uploadBucket = true;
    bucketData = String(stateRelay1);
    bucketPath = "L1";
    
  }
  else if(digitalRead(Switch2) == LOW){
    stateRelay2 = !stateRelay2;
    digitalWrite(Relay2, stateRelay2);
    uploadBucket = true;
    bucketData = String(stateRelay2);
    bucketPath = "L2";
  }
  else if(digitalRead(Switch3) == LOW){
    stateRelay3 = !stateRelay3;
    digitalWrite(Relay3, stateRelay3);
    uploadBucket = true;
    bucketData = String(stateRelay3);
    bucketPath = "L3";
  }
  else if(digitalRead(Switch4) == LOW){
    stateRelay4 = !stateRelay4;
    digitalWrite(Relay4, stateRelay4);
    uploadBucket = true;
    bucketData = String(stateRelay4);
    bucketPath = "L4";
  }
}



void FirebaseWrite(String URL, int data)
{
  /********************************************************************************************
   * Data at a specific node in Firebase RTDB can be read through the following functions: 
   * get, getInt, getFloat, getDouble, getBool, getString, getJSON, getArray, getBlob, getFile
   ********************************************************************************************/
  if (!Firebase.ready() && !signupOK)
  {
    Serial.println("Write Failed: Firebase not ready OR signup not OK");
    return;
  }
  
  URL = room_no + "/" + URL;
  Serial.println(URL);
  Serial.println(String(data));
  // Write an Int number on the database path (URL) room1/L1, room1/L2 etc.
  if (Firebase.RTDB.setString(&fbdo, URL, String(data))){
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }

  //FirebaseJson json;
  //json.add(URL, String(data));

  //URL = room_no;
  //Serial.printf("Set json... %s\n\n", Firebase.RTDB.setJSON(&fbdo, URL, &json) ? "ok" : fbdo.errorReason().c_str());
}
