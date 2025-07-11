#include "rele.h"


void rele_init(uint pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT); // Define o pino como saída
    gpio_put(pin, 1); // Garante que o relé comece desligado 
}

void switch_rele(uint pin, bool state){
    gpio_put(pin, !state);
}