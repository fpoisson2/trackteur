#ifndef GPS_H
#define GPS_H

#include <Arduino.h>

bool getGpsData(float &lat, float &lon, char* timestampOutput);


#endif
