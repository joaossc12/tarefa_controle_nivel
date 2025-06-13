#ifndef RELE_H
#define RELE_H

#include <stdlib.h>
#include "pico/stdlib.h"

void rele_init(uint pin);
void switch_rele(uint pin, bool state);

#endif