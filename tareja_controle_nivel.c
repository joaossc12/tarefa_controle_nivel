#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "lib/rele.h"
#define RELE_PIN 18

#include "lib/sensor.h"
#define SENSOR_PIN 28
#define SENSOR_MIN 2330.0
#define SENSOR_MAX 2635.0


uint limite_max = 80;
uint limite_min = 20;
//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5


volatile bool flag_switch = false;


void gpio_irq_handler(uint gpio, uint32_t events)
{
    static absolute_time_t last_time_A = 0;
    static absolute_time_t last_time_B = 0;
    absolute_time_t now = get_absolute_time();
    if (gpio == botaoA){
        if (absolute_time_diff_us(last_time_A, now) > 200000) { //Debouncing
            flag_switch = !flag_switch;
            last_time_A = now;
        }
    }else if (gpio == botaoB){
        if (absolute_time_diff_us(last_time_B, now) > 200000) { //Debouncing
            reset_usb_boot(0,0);
            last_time_B = now;
        }
}
}


int main()
{
    stdio_init_all();

      // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_init(botaoA);
    gpio_set_dir(botaoA, GPIO_IN);
    gpio_pull_up(botaoA);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(botaoA, GPIO_IRQ_EDGE_FALL, true);
    //Aqui termina o trecho para modo BOOTSEL com botão B

    //Inicializa o sensor
    sensor_init(SENSOR_PIN);

    //Inicializa o relé
    rele_init(RELE_PIN);

    printf("CÓDIGO INICIADO!\n");

    while (true) {
        float nivel = get_nivel(SENSOR_PIN, SENSOR_MAX, SENSOR_MIN);
        printf("Nível: %.2f%%\n", nivel);

        if (nivel >= limite_max) {
            // Desliga o relé porque atingiu o máximo
            flag_switch = false;
            switch_rele(RELE_PIN, false);
            printf("Desligando bomba: nível acima do limite máximo\n");
        }
        else if (nivel <= limite_min) {
            // Liga automaticamente porque atingiu o mínimo
            flag_switch = true;
            switch_rele(RELE_PIN, true);
            printf("Ligando bomba: nível abaixo do limite mínimo\n");
        }
        else {
            // Entre os limites, respeita o botão
            switch_rele(RELE_PIN, flag_switch);
            printf("Controle manual: %s\n", flag_switch ? "ligado" : "desligado");
        }

        sleep_ms(100);
    }
}
