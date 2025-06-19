#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

#include "lib/rele.h"
#define RELE_PIN 18

#include "lib/sensor.h"
#define SENSOR_PIN 28
#define SENSOR_MIN 2330.0
#define SENSOR_MAX 2635.0

#include "lib/ws2812b.h"

#define ADC_RESOLUTION 4095
const uint joystick_y_pin = 26;

uint limite_max = 80;
uint limite_min = 20;
//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5

bool setup = true;
volatile bool flag_switch = false;
volatile bool flag_matriz = true;

void matriz_show_lvl(float leitura_porc)
{
    static uint8_t prev_lines = 0;
    
    Rgb matriz_color = {0, 0, 1};

    uint8_t lines_bruto; // Valor bruto do mapeamento do nível para linhas da matriz
    uint8_t lines; // Total de linhas a serem desenhadas na matriz

    // Mapeia o valor lido para linhas da matriz
    lines_bruto = ((leitura_porc - limite_min) / (limite_max - limite_min)) * 5;
    lines = ceil(lines_bruto); // Arredonda valor obtido

    // Atualiza a matriz APENAS se houver informações diferentes ou se o sistema estiver em inicialização
    if (lines != prev_lines || setup)
    {   
        // Desenha linhas proporcionais ao nível da água atual na matriz, simulando um tanque
        matriz_clear();
        for (int i = 0; i < lines; i++)
        {
            matriz_draw_linha(0, MATRIZ_ROWS - 1 - i, 5, matriz_color);
        }
        matriz_send_data();

        prev_lines = lines;
    }
}

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

    adc_init();
    adc_gpio_init(joystick_y_pin);

    matriz_init();

    // //Inicializa o sensor
    // sensor_init(SENSOR_PIN);

    // //Inicializa o relé
    // rele_init(RELE_PIN);

    // printf("CÓDIGO INICIADO!\n");

    uint16_t valor_adc;
    float porcentagem;
    float anterior;

    while (true) {
        //float nivel = get_nivel(SENSOR_PIN, SENSOR_MAX, SENSOR_MIN);
        // calibra_sensor(SENSOR_PIN);
        // switch_rele(RELE_PIN, flag_switch);
        // sleep_ms(500);

        adc_select_input(0);
        valor_adc = adc_read();
        porcentagem = (valor_adc * 100) / (float) ADC_RESOLUTION;
        
        printf("Nível: %.2f%%\n", porcentagem);

        if (porcentagem != anterior || flag_matriz)
        {
            matriz_show_lvl(porcentagem);

            anterior = porcentagem;
            flag_matriz = false;
        }

        // Encerra estado de inicialização
        if (setup)
        {
            setup = false;
        }

        sleep_ms(500);
    }
}
