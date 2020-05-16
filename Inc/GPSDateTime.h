#pragma once

#define MAX_GPS_STRING 128

class GPSDateTime {
 public:
  GPSDateTime(): tmplen(0), count_(0), validCode(false), dateMillis(0) {  };
  void commit(void);
  void time(const char * time);
  uint16_t hour();
  uint16_t minute();
  uint16_t second();
  void day(const char * day);
  uint16_t day(void);
  void month(const char * month);
  uint16_t month(void);
  void year(const char * year);
  uint16_t year(void);
  DateTime GPSnow();
  bool addch(char c);
  uint32_t capturedAt() { return dateMillis; };

 protected:
  uint32_t newTime_;
  uint16_t newYear_, newMonth_, newDay_;

  uint32_t time_;
  uint16_t year_, month_, day_;

  DateTime getZDA();
  bool verifyChecksum();

 private:
  char tmp[MAX_GPS_STRING];
  uint8_t tmplen;

  uint8_t count_;
  uint8_t parity_;

  bool isNotChecked;
  bool validCode;
  bool validString;
  bool isUpdated_;
  bool getFlag_;
  uint32_t dateMillis;

  bool debug_;
  DateTime now(void);
};
