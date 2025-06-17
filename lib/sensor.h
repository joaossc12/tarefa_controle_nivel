#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#ifndef SENSOR_H
#define SENSOR_H


void sensor_init(int gpio);
float read_sensor(int gpio);
void calibra_sensor(int gpio);
float get_nivel(int gpio, float MAX, float MIN);


#endif