
void sdInit(){
	sd_fat.begin(SD_CONFIG);
}

/*-----------------------------------*/

/*
 * This program is a simple binary write/read benchmark.
 */
#define DISABLE_FS_H_WARNING  // Disable warning for type File not defined.
// #include "SdFat.h"
#include "FreeStack.h"
#include "sdios.h"

// Set PRE_ALLOCATE true to pre-allocate file clusters.
const bool PRE_ALLOCATE = true;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
const size_t BUF_SIZE = 512;

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;
//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t* buf = (uint8_t*)buf32;

// Serial output stream
ArduinoOutStream cout(Serial);
//------------------------------------------------------------------------------
// Store error strings in flash to save RAM.
#define error(s) sd_fat.errorHalt(&Serial, F(s))
//------------------------------------------------------------------------------
void cidDmp() {
  cid_t cid;
  if (!sd_fat.card()->readCID(&cid)) {
    error("readCID failed");
  }
  cout << F("\nManufacturer ID: ");
  cout << uppercase << showbase << hex << int(cid.mid) << dec << endl;
  cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
  cout << F("Product: ");
  for (uint8_t i = 0; i < 5; i++) {
    cout << cid.pnm[i];
  }
  cout << F("\nRevision: ") << cid.prvN() << '.' << cid.prvM() << endl;
  cout << F("Serial number: ") << hex << cid.psn() << dec << endl;
  cout << F("Manufacturing date: ");
  cout << cid.mdtMonth() << '/' << cid.mdtYear() << endl;
  cout << endl;
}
//------------------------------------------------------------------------------
void clearSerialInput() {
  uint32_t m = micros();
  do {
    if (Serial.read() >= 0) {
      m = micros();
    }
  } while (micros() - m < 10000);
}
//------------------------------------------------------------------------------
void setupSD() {
  Serial.begin(115200);

  // Wait for USB Serial
  while (!Serial) {
    yield();
  }
  delay(1000);
  cout << F("\nUse a freshly formatted SD for best performance.\n");
  if (!ENABLE_DEDICATED_SPI) {
    cout << F(
        "\nSet ENABLE_DEDICATED_SPI nonzero in\n"
        "SdFatConfig.h for best SPI performance.\n");
  }
  if (!SD_HAS_CUSTOM_SPI && !USE_SPI_ARRAY_TRANSFER && isSpi(SD_CONFIG)) {
    cout << F(
          "\nSetting USE_SPI_ARRAY_TRANSFER nonzero in\n"
          "SdFatConfig.h may improve SPI performance.\n");
  }
  // use uppercase in hex and use 0X base prefix
  cout << uppercase << showbase << endl;
}
//------------------------------------------------------------------------------
void loopSD() {
  float s;
  uint32_t t;
  uint32_t maxLatency;
  uint32_t minLatency;
  uint32_t totalLatency;
  bool skipLatency;

  // Discard any input.
  clearSerialInput();

  // F() stores strings in flash to save RAM
  cout << F("Type any character to start\n");
  while (!Serial.available()) {
    yield();
  }
#if HAS_UNUSED_STACK
  cout << F("FreeStack: ") << FreeStack() << endl;
#endif  // HAS_UNUSED_STACK

  if (!sd_fat.begin(SD_CONFIG)) {
    sd_fat.initErrorHalt(&Serial);
  }
  if (sd_fat.fatType() == FAT_TYPE_EXFAT) {
    cout << F("Type is exFAT") << endl;
  } else {
    cout << F("Type is FAT") << int(sd_fat.fatType()) << endl;
  }

  cout << F("Card size: ") << sd_fat.card()->sectorCount() * 512E-9;
  cout << F(" GB (GB = 1E9 bytes)") << endl;

  cidDmp();

  // open or create file - truncate existing sd_file.
  if (!sd_file.open("bench.dat", O_RDWR | O_CREAT | O_TRUNC)) {
    error("open failed");
  }

  // fill buf with known data
  if (BUF_SIZE > 1) {
    for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
      buf[i] = 'A' + (i % 26);
    }
    buf[BUF_SIZE - 2] = '\r';
  }
  buf[BUF_SIZE - 1] = '\n';

  cout << F("FILE_SIZE_MB = ") << FILE_SIZE_MB << endl;
  cout << F("BUF_SIZE = ") << BUF_SIZE << F(" bytes\n");
  cout << F("Starting write test, please wait.") << endl << endl;

  // do write test
  uint32_t n = FILE_SIZE / BUF_SIZE;
  cout << F("write speed and latency") << endl;
  cout << F("speed,max,min,avg") << endl;
  cout << F("KB/Sec,usec,usec,usec") << endl;
  for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
    sd_file.truncate(0);
    if (PRE_ALLOCATE) {
      if (!sd_file.preAllocate(FILE_SIZE)) {
        error("preAllocate failed");
      }
    }
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      uint32_t m = micros();
      if (sd_file.write(buf, BUF_SIZE) != BUF_SIZE) {
        error("write failed");
      }
      m = micros() - m;
      totalLatency += m;
      if (skipLatency) {
        // Wait until first write to SD, not just a copy to the cache.
        skipLatency = sd_file.curPosition() < 512;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    sd_file.sync();
    t = millis() - t;
    s = sd_file.fileSize();
    cout << s / t << ',' << maxLatency << ',' << minLatency;
    cout << ',' << totalLatency / n << endl;
  }
  cout << endl << F("Starting read test, please wait.") << endl;
  cout << endl << F("read speed and latency") << endl;
  cout << F("speed,max,min,avg") << endl;
  cout << F("KB/Sec,usec,usec,usec") << endl;

  // do read test
  for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++) {
    sd_file.rewind();
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      buf[BUF_SIZE - 1] = 0;
      uint32_t m = micros();
      int32_t nr = sd_file.read(buf, BUF_SIZE);
      if (nr != BUF_SIZE) {
        error("read failed");
      }
      m = micros() - m;
      totalLatency += m;
      if (buf[BUF_SIZE - 1] != '\n') {
        error("data check error");
      }
      if (skipLatency) {
        skipLatency = false;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    s = sd_file.fileSize();
    t = millis() - t;
    cout << s / t << ',' << maxLatency << ',' << minLatency;
    cout << ',' << totalLatency / n << endl;
  }
  cout << endl << F("Done") << endl;
  sd_file.close();
  sd_fat.end();
}
