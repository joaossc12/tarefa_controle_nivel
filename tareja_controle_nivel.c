#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"

#include "lib/rele.h"
#define RELE_PIN 18

#include "lib/sensor.h"
#define SENSOR_PIN 28
#define SENSOR_MIN 1930.0
#define SENSOR_MAX 2268.0

#include "lib/ws2812b.h"

#include "lib/ssd1306.h"
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

#define LED_GREEN_PIN 11

#define BUZZER_PIN 21
#define WRAP 1000
#define DIV_CLK 250
static uint slice;
static struct repeating_timer timer;
volatile static bool buzzer_on = false;

static float limite_max = 80.0f;
static float limite_min = 20.0f;
#define TOLERANCIA 8 // Diferença aceita para ultrapassar os limites

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
#define botaoA 5

volatile static bool setup = true;
volatile static bool flag_switch = false;
volatile static bool flag_atualizar = true;
volatile static uint8_t status_nivel = 0;

const static Rgb std_color = {0, 0, 1};
const static Rgb alarme_color = {1, 0, 0};
volatile static bool alarme_on = false;

bool is_same_color(Rgb color1, Rgb color2)
{
    return color1.r == color2.r && color1.g == color2.g && color1.b == color2.b;
}

int checar_estado_nivel(float leitura)
{
    if (leitura - limite_max >= TOLERANCIA)
    {
        return 1; // Risco de transbordamento
    }
    else if (limite_min - leitura >= TOLERANCIA)
    {
        return -1; // Nível abaixo do esperado
    }
    else
    {
        return 0; // Nível OK
    }
}

void matriz_show_lvl(float leitura)
{
    static uint8_t prev_lines = 0;
    static Rgb prev_matriz_color = {0, 0, 1};
    static uint8_t min_lines = 1; // Define o valor mínimo de linhas a serem desenhadas na matriz

    // Limitar leitura ao intervalo
    leitura = fmaxf(limite_min, fminf(leitura, limite_max));
   
    // Mapeia o valor lido para o total de linhas adicionais na matriz
    uint8_t lines_bruto = ((leitura - limite_min) / (limite_max - limite_min)) * (MATRIZ_ROWS - min_lines);
    uint8_t lines = round(lines_bruto) + min_lines; // Arredonda valor obtido

    // Define a cor baseado no estado do programa
    Rgb matriz_color = alarme_on ? alarme_color : std_color;

    // Atualiza a matriz APENAS se houver informações novas a mostrar
    if (lines != prev_lines || !is_same_color(matriz_color, prev_matriz_color) || setup)
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

void atualizar_display(ssd1306_t *ssd, float leitura, float nivel)
{
    ssd1306_fill(ssd, false);

    // Desenha frame do menu
    ssd1306_rect(ssd, 3, 3, 122, 60, true, false);
    ssd1306_line(ssd, 3, 37, 123, 37, true);   
    
    // Escreve os valores dos limites e o valor lido
    char str_x[8];
    char str_y[8];
    snprintf(str_x, 8, "%.1f", leitura);
    snprintf(str_y, 8, "%.1f%%", nivel);
    
    ssd1306_draw_string(ssd, str_x, 8, 6);  // Desenha a cor da primeira faixa do resistor
    ssd1306_draw_string(ssd, str_y, 8, 16);  // Desenha a cor da segunda faixa do resistor
    // ---------------------------------------------

    ssd1306_send_data(ssd);
}

bool buzzer_callback(struct repeating_timer *t)
{   
    if (!alarme_on)
    {   
        pwm_set_gpio_level(BUZZER_PIN, 0);
        buzzer_on = false;
        return false;
    }

    if (buzzer_on)
    {
        pwm_set_gpio_level(BUZZER_PIN, WRAP / 2);
    }
    else
    {
        pwm_set_gpio_level(BUZZER_PIN, 0);
    }
    buzzer_on = !buzzer_on;

    return true;
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

    // Inicializa LED verde
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0); // Começa desligado
    bool led_on = false;


    // Inicializa a Matriz
    matriz_init();

    // Inicializa o sensor
    sensor_init(SENSOR_PIN);

    // Inicializa o relé
    rele_init(RELE_PIN);

    // Inicializa o Display OLED
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Iniciando Wi-Fi", 0, 0);
    ssd1306_draw_string(&ssd, "Aguarde...", 0, 30);    
    ssd1306_send_data(&ssd);
    // ------------------------

    // Inicializa o Buzzer
        // Obter slice e definir pino como PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_PIN);

    // Configurar frequência
    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV_CLK); 
    pwm_set_gpio_level(BUZZER_PIN, 0);
    pwm_set_enabled(slice, true);

    float adc;
    float nivel;
    float anterior = 0;

    printf("CÓDIGO INICIADO!\n");

    while (true) {
        // nivel = get_nivel(SENSOR_PIN, SENSOR_MAX, SENSOR_MIN);
        // calibra_sensor(SENSOR_PIN);

        adc = read_sensor(RELE_PIN);
        nivel = (float)(adc - SENSOR_MIN)/ (SENSOR_MAX - SENSOR_MIN) * 100;
        switch_rele(RELE_PIN, flag_switch);

        led_on = gpio_get(LED_GREEN_PIN);
        if (flag_switch)
        {   
            if (!led_on)
            {
                gpio_put(LED_GREEN_PIN, 1);
                led_on = true;
            }
        }
        else
        {   
            if (led_on)
            {
                gpio_put(LED_GREEN_PIN, 0);
                led_on = false;
            }
        }

        // Aciona o alarme com base no estado atual do reservatório
        status_nivel = checar_estado_nivel(nivel);
        if (status_nivel != 0)
        {
            if (!alarme_on)
            {   
                buzzer_on = true;
                add_repeating_timer_ms(200, buzzer_callback, NULL, &timer);
                alarme_on = true;
            }
        }
        else if (alarme_on)
        {   
            alarme_on = false;
        }

        if (nivel != anterior || flag_atualizar)
        {
            matriz_show_lvl(nivel);
            atualizar_display(&ssd, adc, nivel);

            anterior = nivel;
            flag_atualizar = false;
        }

        // Encerra estado de inicialização
        if (setup)
        {
            setup = false;
        }

        sleep_ms(500);
    }
}
