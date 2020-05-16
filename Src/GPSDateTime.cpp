// from https://raw.githubusercontent.com/DennisSc/PPS-ntp-server/master/src/GPS.cpp

extern "C" {
#include "main.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "uart.h"
};
#include "DateTime.h"
#include "GPSDateTime.h"

#define GPS_CODE_ZDA "GPZDA"
#define GPS_CODE2_ZDA "GNZDA"

/**
 * Save new date and time to private variables
 */
void GPSDateTime::commit() {
  time_ = newTime_;
  year_ = newYear_;
  month_ = newMonth_;
  day_ = newDay_;
  dateMillis = HAL_GetTick();
}

void GPSDateTime::time(const char *time) {
  newTime_ = atof(time) * 100;
}

uint16_t GPSDateTime::hour() {
  return time_ / 1000000;
}

uint16_t GPSDateTime::minute() {
  return (time_ / 10000) % 100;
}

uint16_t GPSDateTime::second() {
  return (time_ / 100) % 100;
}

void GPSDateTime::day(const char *day) {
  newDay_ = atoi(day);
}
uint16_t GPSDateTime::day(void) { return day_; };

void GPSDateTime::month(const char * month) {
  newMonth_ = atoi(month);
}
uint16_t GPSDateTime::month(void) { return month_; };

void GPSDateTime::year(const char * year) {
  newYear_ = atoi(year);
}
uint16_t GPSDateTime::year(void) { return year_; };

bool GPSDateTime::verifyChecksum() {
  char checkParity[12];

  utoa(parity_, checkParity, 16);
  char *s = checkParity;
  while(*s) {
    *s = toupper(*s);
    s++;
  }

  return strcmp(checkParity, tmp) == 0;
}

/**
 * Decode NMEA line to date and time
 * $GPZDA,174304.36,24,11,2015,00,00*66
 * $0    ,1        ,2 ,3 ,4   ,5 ,6 *7  <-- pos
 * @return line decoded
 */
bool GPSDateTime::addch(char c) {
  if (c == '$') {
    tmplen = 0;
    tmp[tmplen] = '\0';
    count_ = 0;
    parity_ = 0;
    validCode = true;
    isNotChecked = true;
    isUpdated_ = false;
    return false;
  }

  if (!validCode) {
    return false;
  }
  if (c == ',' || c == '*') {
    // determinator between values
    switch (count_) {
      case 0: // ID
        if ((strcmp(tmp, GPS_CODE_ZDA) == 0) || (strcmp(tmp, GPS_CODE2_ZDA) == 0)) {
          validCode = true;
        } else {
          validCode = false;
        }
        break;
      case 1: // time
        this->time(tmp);
        break;
      case 2: // day
        this->day(tmp);
        break;
      case 3: // month
        this->month(tmp);
        break;
      case 4: // year
        this->year(tmp);
        break;
      case 5: // timezone fields are ignored
      case 6:
      default:
        break;
    }
    if (c == ',') {
      parity_ ^= (uint8_t) c;
    }
    if (c == '*') {
      isNotChecked = false;
    }
    tmplen = 0;
    tmp[tmplen] = '\0';
    count_++;
  } else if (c == '\r' || c == '\n') {
    // carriage return, so check
    validString = this->verifyChecksum();

    if (validString) {
      this->commit();
      isUpdated_ = true;
      // commit datetime
    }
    tmplen = 0;
    tmp[tmplen] = '\0';
    count_ = 0;
    parity_ = 0;
    validCode = false;
    return true;
  } else {
    // ordinary char
    if(tmplen < MAX_GPS_STRING-1) {
      tmp[tmplen] = c;
      tmplen++;
      tmp[tmplen] = '\0';
    }
    if (isNotChecked) {
      // XOR of all characters from $ to *
      parity_ ^= (uint8_t) c;
    }
  }

  return false;
}



/**
 * Return instance of DateTime class
 * @return DateTime
 */
DateTime GPSDateTime::GPSnow() {
  return DateTime(this->year(), this->month(), this->day(), this->hour(), this->minute(), this->second());
}
