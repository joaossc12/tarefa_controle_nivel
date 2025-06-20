#include <stdio.h>
#include <stdlib.h>
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
#define TOLERANCIA 5 // Diferença aceita para ultrapassar os limites

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5

bool setup = true;
volatile bool flag_switch = false;
volatile bool flag_matriz = true;

bool is_same_color(Rgb color1, Rgb color2)
{
    return color1.r == color2.r && color1.g == color2.g && color1.b == color2.b;
}

void matriz_show_lvl(float leitura_porc)
{
    static uint8_t prev_lines = 0;
    static Rgb prev_matriz_color = {0, 0, 1};
    
    // Define o valor mínimo de linhas a serem desenhadas na matriz
    uint8_t min_lines = limite_min > 0 ? 1 : 0;
    uint8_t lines = min_lines;

    Rgb matriz_color;

    // Limitar valor lido pelos limites máximo e mínimo
    if (leitura_porc > limite_max)
    {   
        lines = MATRIZ_ROWS; // Matriz deve exibir o maior valor possível

        // Indicar risco de transbordamento se a diferença ultrapassar a tolerância
        if (leitura_porc - limite_max >= TOLERANCIA)
        {
            matriz_color = {1, 0, 0};
        }
    }
    else if (leitura_porc < limite_min)
    {   
        lines = min_lines; // Matriz deve exibir o menor valor possível

        // Indicar que nível de água abaixo do ideal se a diferença ultrapassar a tolerância
        if (limite_min - leitura_porc >= TOLERANCIA)
        {
            matriz_color = {1, 0, 0};
        }
    else
    {
        // Mapeia o valor lido para o total de linhas adicionais na matriz
        uint8_t lines_bruto = ((leitura_porc - limite_min) / (limite_max - limite_min)) * (MATRIZ_ROWS - min_lines);
        lines += round(lines_bruto); // Arredonda valor obtido

        matriz_color = {0, 0, 1};
    }

    // Atualiza a matriz APENAS se houver informações novas a mostrar
    if (lines != prev_lines || is_same_color(matriz_color, prev_matriz_color) || setup)
    {   
        // Desenha linhas proporcionais ao nível da água atual na matriz, simulando um tanque
        matriz_clear();
        for (int i = 0; i < lines; i++)
        {
            matriz_draw_linha(0, MATRIZ_ROWS - 1 - i, 5, matriz_color);
        }
        matriz_send_data();

        prev_lines = lines;
        prev_matriz_color = matriz_color;
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
