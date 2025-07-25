#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Arduino.h>

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
String bleAddress = "FFFFFFFFFFFF"; // CONFIGURATION: < Use the real device BLE address here.
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

int vibration;

// at what vibration level the motor stalls, will be treated as zero
// RS-555SH at 12V stalls at 3
int stallLevel = 3;

// for driving heavier motors, which require a short burst of high power to start moving
// enables once the vibration is set to 0
bool softStartRequired = true;
// RS-555SH works well on 50ms
int softStartDuration = 50; // in ms

// Motor A connections
int enA = 14;
int in1 = 27;
int in2 = 16;

// unused, for now
// Motor B connections
//int enB = 3;
//int in3 = 5;
//int in4 = 4;

#define SERVICE_UUID           "50300001-0023-4bd4-bbd5-a6920e4c5653"
#define CHARACTERISTIC_RX_UUID "50300002-0023-4bd4-bbd5-a6920e4c5653"
#define CHARACTERISTIC_TX_UUID "50300003-0023-4bd4-bbd5-a6920e4c5653"

// vibration control function
void UpdateVibrate(void) {
  Serial.print("vibration");
  Serial.println(vibration);

  // stall vibration level
  if (vibration <= stallLevel) {
    softStartRequired = true;
  }

  int power = map(vibration, 0 , 20 , 0, 255);
  power = constrain(power, 0, 255);
  Serial.print("power");
  Serial.println(power);

  if ( vibration > stallLevel) {
    // Turn on motor
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);

    if (softStartRequired) {
      Serial.println("Soft starting");
      ledcWrite(enA, 127);
      delay(softStartDuration);

      softStartRequired = false;
    }
    
  } else {
    // Now turn off motors
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    //digitalWrite(in3, LOW);
    //digitalWrite(in4, LOW);
  }
  
  // set speed
  ledcWrite(enA, power);
}



class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



class MySerialCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      static uint8_t messageBuf[64];
      assert(pCharacteristic == pRxCharacteristic);
      String rxValue = pRxCharacteristic->getValue();
      
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }

      
      if (rxValue == "DeviceType;") {
        Serial.println("$Responding to Device Enquiry");
        memmove(messageBuf, "S:37:FFFFFFFFFFFF;", 18);
        // CONFIGURATION:               ^ Use a BLE address of the Lovense device you're cloning.
        pTxCharacteristic->setValue(messageBuf, 18);
        pTxCharacteristic->notify();
      } else if (rxValue == "Battery;") {
        memmove(messageBuf, "69;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue == "PowerOff;") {
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue == "RotateChange;") {
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue.lastIndexOf("Status:", 0) == 0) {
        memmove(messageBuf, "2;", 2);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
      } else if (rxValue.lastIndexOf("Vibrate:", 0) == 0) {
        vibration = std::atoi(rxValue.substring(8).c_str());
        memmove(messageBuf, "OK;", 3);
        pTxCharacteristic->setValue(messageBuf, 3);
        pTxCharacteristic->notify();
        UpdateVibrate();
      } else if (rxValue.lastIndexOf("Vibrate1:", 0) == 0) {
        // potential vibrator 2 control
        //vibration = std::atoi(rxValue.substring(9).c_str());
        //memmove(messageBuf, "OK;", 3);
        //pTxCharacteristic->setValue(messageBuf, 3);
        //pTxCharacteristic->notify();
        //UpdateVibrate();
      } else if (rxValue.lastIndexOf("Vibrate2:", 0) == 0) {
        // potential vibrator 3 control
        //vibration = std::atoi(rxValue.substring(9).c_str());
        //memmove(messageBuf, "OK;", 3);
        //pTxCharacteristic->setValue(messageBuf, 3);
        //pTxCharacteristic->notify();
        //UpdateVibrate();
      } else {
        Serial.println("$Unknown request");        
        memmove(messageBuf, "ERR;", 4);
        pTxCharacteristic->setValue(messageBuf, 4);
        pTxCharacteristic->notify();
      }
    }
};

void setup() {
  Serial.begin(115200);

  // Set all the motor control pins to outputs
  // L298N is too slow to use ultrasonic switching frequencies
  ledcAttach(enA, 1000, 8);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);

  //pinMode(enB, OUTPUT);
  //pinMode(in3, OUTPUT);
  //pinMode(in4, OUTPUT);

  // Turn off motors - Initial state
  ledcWrite(enA, 0);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  //digitalWrite(in3, LOW);
  //digitalWrite(in4, LOW);

  // buzz test, run motor at 20% speed
  delay(100);
  vibration = 4;
  UpdateVibrate();
  delay(100);
  vibration = 0;
  UpdateVibrate();
  delay(100);

  // Create the BLE Device
  BLEDevice::init("LVS-ESP"); // CONFIGURATION: The name doesn't actually matter, The app identifies it by the reported id.

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristics
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_TX_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_RX_UUID,
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pRxCharacteristic->setCallbacks(new MySerialCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(100); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
