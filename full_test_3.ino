#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// DEVICE ID
// ============================================================
//
// ESP32 1:
// #define DEVICE_ID 1
//
// ESP32 2:
// #define DEVICE_ID 2
//
// ============================================================

#define DEVICE_ID 1

// ============================================================
// LORA PINS
// ============================================================

#define LORA_CS    15
#define LORA_RST   27
#define LORA_DIO0  26

#define LORA_SCK   14
#define LORA_MISO  12
#define LORA_MOSI  13

// ============================================================
// BLE UUIDs
// ============================================================

#define BLE_SERVICE_UUID \
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e"

#define BLE_RX_UUID \
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

#define BLE_TX_UUID \
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// ============================================================
// SPI
// ============================================================

SPIClass spi(VSPI);

// ============================================================
// LORA
// ============================================================

Module module(
  LORA_CS,
  LORA_DIO0,
  LORA_RST,
  -1,
  spi
);

SX1278 radio(&module);

// ============================================================
// LORA RECEIVE FLAG
// ============================================================

volatile bool receivedFlag = false;

void setReceiveFlag()
{
  receivedFlag = true;
}

// ============================================================
// BLE VARIABLES
// ============================================================

BLEServer* bleServer = nullptr;

BLECharacteristic* bleRxCharacteristic = nullptr;
BLECharacteristic* bleTxCharacteristic = nullptr;

bool bleDeviceConnected = false;

// ============================================================
// BLE SERVER CALLBACKS
// ============================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* server) override
  {
    bleDeviceConnected = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println("BLE CLIENT CONNECTED");
    Serial.println("========================================");
    Serial.println();
  }

  void onDisconnect(BLEServer* server) override
  {
    bleDeviceConnected = false;

    Serial.println();
    Serial.println("========================================");
    Serial.println("BLE CLIENT DISCONNECTED");
    Serial.println("========================================");
    Serial.println();

    delay(100);

    BLEDevice::startAdvertising();

    Serial.println("BLE advertising restarted.");
  }
};

// ============================================================
// BLE RECEIVE CALLBACK
// PHONE → ESP32 → LORA
// ============================================================

class RxCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* characteristic) override
  {
    String receivedData =
      characteristic->getValue();

    if (receivedData.length() == 0)
    {
      return;
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("DATA RECEIVED FROM PHONE");
    Serial.println("========================================");

    Serial.print("Data: ");
    Serial.println(receivedData);

    Serial.print("Length: ");
    Serial.println(receivedData.length());

    Serial.println();

    // --------------------------------------------------------
    // PHONE → LORA
    // --------------------------------------------------------

    Serial.println(
      "Forwarding phone data through LoRa..."
    );

    int state =
      radio.transmit(receivedData);

    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println(
        "PHONE → LORA SUCCESS"
      );
    }
    else
    {
      Serial.print(
        "PHONE → LORA FAILED. ERROR: "
      );

      Serial.println(state);
    }

    Serial.println();

    // --------------------------------------------------------
    // Return to receive mode
    // --------------------------------------------------------

    radio.startReceive();
  }
};

// ============================================================
// START BLE
// ============================================================

void setupBLE()
{
  Serial.println();
  Serial.println(
    "Initializing Bluetooth Low Energy..."
  );

  // ----------------------------------------------------------
  // DEVICE NAME
  // ----------------------------------------------------------

#if DEVICE_ID == 1

  BLEDevice::init(
    "ESP 32 1"
  );

#elif DEVICE_ID == 2

  BLEDevice::init(
    "ESP 32 2"
  );

#else

  BLEDevice::init(
    "ESP 32"
  );

#endif

  // ----------------------------------------------------------
  // CREATE SERVER
  // ----------------------------------------------------------

  bleServer =
    BLEDevice::createServer();

  bleServer->setCallbacks(
    new ServerCallbacks()
  );

  // ----------------------------------------------------------
  // CREATE SERVICE
  // ----------------------------------------------------------

  BLEService* service =
    bleServer->createService(
      BLE_SERVICE_UUID
    );

  // ----------------------------------------------------------
  // RX
  // PHONE → ESP32
  // ----------------------------------------------------------

  bleRxCharacteristic =
    service->createCharacteristic(
      BLE_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR
    );

  bleRxCharacteristic->setCallbacks(
    new RxCallbacks()
  );

  // ----------------------------------------------------------
  // TX
  // ESP32 → PHONE
  // ----------------------------------------------------------

  bleTxCharacteristic =
    service->createCharacteristic(
      BLE_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE
    );

  bleTxCharacteristic->addDescriptor(
    new BLE2902()
  );

  // ----------------------------------------------------------
  // START SERVICE
  // ----------------------------------------------------------

  service->start();

  // ----------------------------------------------------------
  // ADVERTISING
  // ----------------------------------------------------------

  BLEAdvertising* advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(
    BLE_SERVICE_UUID
  );

  advertising->setScanResponse(
    true
  );

  advertising->setMinPreferred(
    0x06
  );

  advertising->setMinPreferred(
    0x12
  );

  BLEDevice::startAdvertising();

  // ----------------------------------------------------------
  // INFORMATION
  // ----------------------------------------------------------

  Serial.println(
    "BLE initialized successfully."
  );

  Serial.println();

  Serial.print(
    "BLE DEVICE: ESP 32 "
  );

  Serial.println(
    DEVICE_ID
  );

  Serial.println();

  Serial.println(
    "BLE SERVICE UUID:"
  );

  Serial.println(
    BLE_SERVICE_UUID
  );

  Serial.println();

  Serial.println(
    "BLE RX UUID:"
  );

  Serial.println(
    BLE_RX_UUID
  );

  Serial.println();

  Serial.println(
    "BLE TX UUID:"
  );

  Serial.println(
    BLE_TX_UUID
  );

  Serial.println();

  Serial.println(
    "BLE advertising started."
  );
}

// ============================================================
// SEND DATA TO PHONE
// LORA → ESP32 → BLE → PHONE
// ============================================================

void sendBLEData(
  const String& data
)
{
  if (!bleDeviceConnected)
  {
    Serial.println(
      "No BLE phone connected."
    );

    return;
  }

  if (bleTxCharacteristic == nullptr)
  {
    Serial.println(
      "BLE TX characteristic unavailable."
    );

    return;
  }

  bleTxCharacteristic->setValue(
    data.c_str()
  );

  bleTxCharacteristic->notify();

  Serial.println();
  Serial.println("----------------------------------------");

  Serial.println(
    "LORA DATA SENT TO PHONE"
  );

  Serial.print(
    "BLE Data: "
  );

  Serial.println(data);

  Serial.println("----------------------------------------");
  Serial.println();
}

// ============================================================
// CHECK LORA RECEIVE
// LORA → ESP32 → BLE → PHONE
// ============================================================

void checkReceive()
{
  if (!receivedFlag)
  {
    return;
  }

  receivedFlag = false;

  String message;

  int state =
    radio.readData(message);

  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.print(
      "LORA RX FAILED. ERROR: "
    );

    Serial.println(state);

    radio.startReceive();

    return;
  }

  // ==========================================================
  // PACKET RECEIVED
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("           LORA PACKET RECEIVED");
  Serial.println("========================================");

  Serial.print(
    "Message: "
  );

  Serial.println(message);

  // ----------------------------------------------------------
  // SIGNAL INFORMATION
  // ----------------------------------------------------------

  int rssi =
    radio.getRSSI();

  float snr =
    radio.getSNR();

  Serial.print(
    "RSSI: "
  );

  Serial.print(rssi);

  Serial.println(
    " dBm"
  );

  Serial.print(
    "SNR: "
  );

  Serial.print(snr, 2);

  Serial.println(
    " dB"
  );

  Serial.println();

  // ----------------------------------------------------------
  // CHECK WHETHER PACKET IS FROM THIS DEVICE
  // ----------------------------------------------------------

  String ownID =
    "\"deviceId\":" +
    String(DEVICE_ID);

  String ownIDAlternative =
    "ID=" +
    String(DEVICE_ID);

  if (
    message.indexOf(ownID) >= 0 ||
    message.indexOf(ownIDAlternative) >= 0
  )
  {
    Serial.println(
      "Packet appears to belong to this device."
    );
  }
  else
  {
    // ========================================================
    // REMOTE PACKET
    // ========================================================

    Serial.println(
      "REMOTE DEVICE PACKET!"
    );

    Serial.println();
    Serial.println(
      "*** LORATRACK LINK ACTIVE ***"
    );

    // --------------------------------------------------------
    // Wrap the received LoRa data for Flutter
    // --------------------------------------------------------

    String bleMessage =
      "{\"type\":\"lora_rx\","
      "\"data\":" +
      String("\"") +
      message +
      String("\"") +
      ",\"rssi\":" +
      String(rssi) +
      ",\"snr\":" +
      String(snr, 2) +
      "}";

    // --------------------------------------------------------
    // LORA → PHONE
    // --------------------------------------------------------

    sendBLEData(
      bleMessage
    );
  }

  Serial.println(
    "========================================"
  );

  Serial.println();

  // ----------------------------------------------------------
  // Return to receive mode
  // ----------------------------------------------------------

  radio.startReceive();
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1500);

  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "           LORATRACK ESP32"
  );

  Serial.println(
    "        BLE + SX1278 LORA"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "DEVICE ID: "
  );

  Serial.println(
    DEVICE_ID
  );

  Serial.println();

  // ==========================================================
  // SPI
  // ==========================================================

  spi.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_CS
  );

  Serial.println(
    "SPI started."
  );

  Serial.println();

  // ==========================================================
  // SX1278
  // ==========================================================

  Serial.println(
    "Initializing SX1278..."
  );

  int state =
    radio.begin(
      433.0
    );

  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.println();
    Serial.println(
      "**************************************"
    );

    Serial.println(
      "SX1278 INITIALIZATION FAILED!"
    );

    Serial.print(
      "ERROR CODE: "
    );

    Serial.println(state);

    Serial.println(
      "**************************************"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "SX1278 initialized successfully."
  );

  Serial.println();

  // ==========================================================
  // LORA CONFIGURATION
  // ==========================================================

  radio.setBandwidth(
    125.0
  );

  radio.setSpreadingFactor(
    7
  );

  radio.setCodingRate(
    5
  );

  radio.setOutputPower(
    17
  );

  radio.setPreambleLength(
    8
  );

  radio.setSyncWord(
    0x12
  );

  Serial.println(
    "LoRa configuration:"
  );

  Serial.println(
    "Frequency : 433 MHz"
  );

  Serial.println(
    "Bandwidth : 125 kHz"
  );

  Serial.println(
    "SF        : 7"
  );

  Serial.println(
    "CR        : 4/5"
  );

  Serial.println(
    "Power     : 17 dBm"
  );

  Serial.println(
    "Sync Word : 0x12"
  );

  Serial.println();

  // ==========================================================
  // LORA INTERRUPT
  // ==========================================================

  radio.setDio0Action(
    setReceiveFlag,
    RISING
  );

  state =
    radio.startReceive();

  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.print(
      "RX START FAILED: "
    );

    Serial.println(state);

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "LoRa RX mode started."
  );

  Serial.println();

  // ==========================================================
  // BLE
  // ==========================================================

  setupBLE();

  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "              SYSTEM READY"
  );

  Serial.println(
    "========================================"
  );

  Serial.println();

  Serial.print(
    "ESP 32 "
  );

  Serial.print(
    DEVICE_ID
  );

  Serial.println(
    " is ready."
  );

  Serial.println();

  Serial.println(
    "Waiting for phone BLE connection..."
  );

  Serial.println(
    "Waiting for LoRa packets..."
  );

  Serial.println();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // Check incoming LoRa packets
  // ----------------------------------------------------------

  checkReceive();

  delay(5);
}