#include <Arduino.h>
#include <SPI.h>
#include "USB.h"
#include "RadioLib.h"

#define RESET_PIN 17
#define CS_PIN 13
#define DIO0 21
#define DIO1 34

SPIClass spi(HSPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
SX1278 radio = new Module(CS_PIN /*cs*/, DIO0 /*irq*/, RESET_PIN /*rst*/, DIO1 /*gpio*/, spi, spiSettings);

// digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
// digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW

unsigned long t1;
const unsigned long timeout = 3000;

float freq[4] = {433.92, 0.1, 433.92, 433.92};
float bitr[4] = {33.6, 1.2, 33.6, 33.6};
float freqDev[4] = {40, 10, 40, 40};

void get_chip_id(void)
{
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8)
  {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.printf("ESP32 Chip model = %s Rev %d, ", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("%d cores, ", ESP.getChipCores());
  Serial.print("ID: ");
  Serial.println(chipId, HEX);
}

// flag to indicate that a preamble was not detected
volatile bool timeoutFlag = false;

// flag to indicate that a preamble was detected
volatile bool detectedFlag = false;

// flag to indicate if we are currently receiving
bool receiving = false;

// this function is called when no preamble
// is detected within timeout period
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
ICACHE_RAM_ATTR void setFlagTimeout(void)
{
  // we timed out, set the flag
  timeoutFlag = true;
}

// this function is called when LoRa preamble
// is detected within timeout period
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
ICACHE_RAM_ATTR void setFlagDetected(void)
{
  // we got a preamble, set the flag
  detectedFlag = true;
}

void setup()
{
  // initialize built in LED pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  spi.begin(6, 8, 10, CS_PIN); // SCLK, MISO, MOSI, SS

  // Delay for USB Serial
  delay(2000);

  Serial.begin();
  USB.begin();

  get_chip_id();

  /*
    uint8_t setting[255];
    setting[0] = 1;

    spi.beginTransaction(spiSettings);
    digitalWrite(CS_PIN, LOW);
    spi.transfer(setting, 64);

    digitalWrite(CS_PIN, HIGH);
    spi.endTransaction();

    for (int i = 0; i < 64; i++)
    {
      Serial.printf("%02x ", setting[i]);
    }
    Serial.println();
  */
  // initialize SX1278 with default settings
  Serial.print(F("[SX1278] Initializing ... "));
  int state = radio.beginFSK(freq[0], bitr[0], freqDev[0], 100.0, 10, 1, false);

  uint8_t syncWord[] = {0x55, 0xAD};
  radio.setSyncWord(syncWord, 1);
  
  /*
  SX1278::beginFSK(434.0, 4.8, 5.0, 125.0, 10, 16, false);
  SX127x::variablePacketLengthMode(SX127X_MAX_PACKET_LENGTH);
  uint8_t syncWord[] = {0x12, 0xAD};
  SX127x::setSyncWord(syncWord, 2);
  SX1278::setDataShaping(RADIOLIB_SHAPING_NONE);
  SX127x::setCurrentLimit(60);
  SX127x::setEncoding(RADIOLIB_ENCODING_NRZ);
  SX1278::setCRC(true);
  */
  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("success!"));
  }
  else
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
      ;
  }

  // set the function that will be called
  // when LoRa preamble is not detected within CAD timeout period
  radio.setDio0Action(setFlagTimeout, RISING);

  // set the function that will be called
  // when LoRa preamble is detected
  radio.setDio1Action(setFlagDetected, RISING);
  /*
    // start scanning the channel
    Serial.print(F("[SX1278] Starting scan for LoRa preamble ... "));
    state = radio.startChannelScan();
    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println(F("success!"));
    }
    else
    {
      Serial.print(F("failed, code "));
      Serial.println(state);
    }*/

  t1 = millis();
}

void loop()
{
  int state;
  byte byteArr[32];

  if (millis() - t1 > timeout)
  {
    t1 = millis();

    float pref = freq[0];

    freq[0] += freq[1];
    if (freq[0] > freq[3])
    {
      freq[0] = freq[2];

      float pred = freqDev[0];

      freqDev[0] += freqDev[1];
      if (freqDev[0] > freqDev[3])
      {
        freqDev[0] = freqDev[2];

        float prebr = bitr[0];
        bitr[0] += bitr[1];
        if (bitr[0] > bitr[3])
        {
          bitr[0] = bitr[2];
        }

        if (prebr != bitr[0])
        {
          state = radio.setBitRate(bitr[0]);
          if (state != RADIOLIB_ERR_NONE)
          {
            Serial.printf("[SX1278] freq: %f, dev: %f, bitrate: %f, error: %d\n", freq[0], freqDev[0], bitr[0], state);
          }
        }
      }

      if (pred != freqDev[0])
      {
        state = radio.setFrequencyDeviation(freqDev[0]);
        if (state != RADIOLIB_ERR_NONE)
        {
          Serial.printf("[SX1278] freq: %f, dev: %f, bitrate: %f, error: %d\n", freq[0], freqDev[0], bitr[0], state);
        }
      }
    }

    if (pref != freq[0])
    {
      state = radio.setFrequency(freq[0]);
      if (state != RADIOLIB_ERR_NONE)
      {
        Serial.printf("[SX1278] freq: %f, dev: %f, bitrate: %f, error: %d\n", freq[0], freqDev[0], bitr[0], state);
      }
    }
  }

  state = radio.receive(byteArr, 8);
  if (state != RADIOLIB_ERR_RX_TIMEOUT)
  {
    Serial.printf("[SX1278] Receive status: %d\n", state);
  }

  // check if we need to restart channel activity detection
  if (detectedFlag || timeoutFlag)
  {
    state = RADIOLIB_ERR_NONE;

    // check ongoing reception
    if (receiving)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      // DIO triggered while reception is ongoing
      // that means we got a packet

      // reset flags first
      detectedFlag = false;
      timeoutFlag = false;

      // you can read received data as an Arduino String
      // String str;
      // state = radio.readData(str);

      // you can also read received data as byte array

      state = radio.readData(byteArr, 32);

      // Serial.printf("[SX1278] freq: %f, dev: %f\n", freq[0], freqDev[0]);
      //  print RSSI (Received Signal Strength Indicator)


      if (state == RADIOLIB_ERR_NONE && (byteArr[0] != byteArr[1] || byteArr[0] != byteArr[7] || byteArr[0] != byteArr[15] || byteArr[0] != byteArr[23]))
      {
        Serial.printf("[SX1278] freq: %f, dev: %f, bitrate: %f\n", freq[0], freqDev[0], radio.getDataRate());

        // packet was successfully received
        Serial.println(F("[SX1278] Received packet!"));

        // print data of the packet
        Serial.print(F("[SX1278] Data:\t\t"));
        for (int i = 0; i < sizeof(byteArr); i++)
        {
          Serial.printf("%02x ", byteArr[i]);
        }
        Serial.println();

        // print RSSI (Received Signal Strength Indicator)
        Serial.print(F("[SX1278] RSSI:\t\t"));
        Serial.print(radio.getRSSI());
        Serial.println(F(" dBm"));

        // print SNR (Signal-to-Noise Ratio)
        Serial.print(F("[SX1278] SNR:\t\t"));
        Serial.print(radio.getSNR());
        Serial.println(F(" dB"));

        // print frequency error
        Serial.print(F("[SX1278] Frequency error:\t"));
        Serial.print(radio.getFrequencyError());
        Serial.println(F(" Hz"));
      }
      else if (state == RADIOLIB_ERR_CRC_MISMATCH)
      {
        // packet was received, but is malformed
        Serial.println(F("[SX1278] CRC error!"));
      }
      else
      {
        // some other error occurred
        //Serial.print(F("[SX1278] Failed, code "));
        //Serial.println(state);
      }

      // reception is done now
      receiving = false;
    }

    // check if we got a preamble
    if (detectedFlag)
    {
      // LoRa preamble was detected
      //Serial.print(F("[SX1278] Preamble detected, starting reception ... "));
      state = radio.startReceive(0, RADIOLIB_SX127X_RXSINGLE);
      if (state == RADIOLIB_ERR_NONE)
      {
        //Serial.println(F("success!"));
      }
      else
      {
        //Serial.print(F("failed, code "));
        //Serial.println(state);
      }

      // set the flag for ongoing reception
      receiving = true;
    }
    else if (!receiving)
    {
      // nothing was detected
      // do not print anything, it just spams the console
    }

    // reset flags
    timeoutFlag = false;
    detectedFlag = false;
  }

  digitalWrite(LED_BUILTIN, LOW);
}
