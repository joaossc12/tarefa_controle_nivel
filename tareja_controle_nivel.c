#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"

#include "lib/rele.h"
#define RELE_PIN 18

#include "lib/sensor.h"
#define POTENCIOMETRO_PIN 28
#define LIMITE_MIN 2150
#define LIMITE_MAX 2500



//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5

float nivel = 0;

volatile bool flag = false;

void gpio_irq_handler(uint gpio, uint32_t events)
{
    static absolute_time_t last_time_A = 0;
    static absolute_time_t last_time_B = 0;
    absolute_time_t now = get_absolute_time();
    if (gpio == botaoA){
        if (absolute_time_diff_us(last_time_A, now) > 200000) { //Debouncing
            flag = !flag;
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

    //Inicializa o ADC
    adc_init();
    adc_gpio_init(POTENCIOMETRO_PIN);


    //rele_init(RELE_PIN);


    float nvl = 0;
    printf("CÓDIGO INICIADO!\n");
    while (true) {
        float soma = 0;
        adc_select_input(2);
        for (int i =0; i <500; i++){
            soma += (float)adc_read();
        }
        nivel = soma/500.0f;
        
        
        printf("Nivel: %f\n", nivel);
        //switch_rele(RELE_PIN, flag);
        printf("START LOOP\n");   
        sleep_ms(1000);
    }
}
