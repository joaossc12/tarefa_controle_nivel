/*----------------------------- MONITORAMENTO DE NÍVEL -----------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

// Macros e bibliotecas para Wi-Fi
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lib/html.h"
#define WIFI_SSID "NOME_WIFI"
#define WIFI_PASS "SENHA_WIFI"

// GPIO dos botões
#define BOTAO_A 5
#define BOTAO_B 6
#define BOTAO_JOY 22

// GPIO do LED VERDE
#define LED_GREEN_PIN 11

// GPIO e macros para o Buzzer e PWM
#define BUZZER_PIN 21
#define WRAP 1000
#define DIV_CLK 250
static uint slice;
static struct repeating_timer timer;
volatile static bool buzzer_on = false;

// Biblioteca e pino do Relé
#include "lib/rele.h"
#define RELE_PIN 18

// Biblioteca, GPIO e macros do Sensor/Potenciômetro
#include "lib/sensor.h"
#define SENSOR_PIN 28
#define SENSOR_MIN 1862.0
#define SENSOR_MAX 2150.0

// Biblioteca para Matriz de LEDs
#include "lib/ws2812b.h"

// Bibliotecas e macros I2C para Display OLED
#include "lib/ssd1306.h"
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

// Trecho para modo BOOTSEL com botão Joystick
#include "pico/bootrom.h"

// Limites do reservatório
static float limite_max = 80.0f;
static float limite_min = 20.0f;
#define TOLERANCIA 8 // Diferença aceita para ultrapassar os limites
#define PUMP_WARN 10 // O quão abaixo do limite máximo a bomba deve ser desligada

// Valor do nível da água (em porcentagem)
static float nivel;

// Variáveis de estado e flags
volatile static bool flag_atualizar = true; // Pede atualização do Display e Matriz
volatile static bool pump_state = false; // Estado da bomba
volatile static bool alarme_on = false; // Estado do alarme
volatile static bool led_on = false; // Estado do LED

// Status atual do nível do reservatório
#define NIVEL_OK 0
#define NIVEL_ACIMA 1
#define NIVEL_ABAIXO -1
volatile static uint8_t status_nivel = 0;

// Estado AUTOMÁTICO ou MANUAL do programa
#define AUTO 1
#define MANUAL 0
volatile static bool modo = MANUAL; // Inicia no modo MANUAL

// Cores da Matriz para cada estado do programa
const static Rgb std_color = {0, 0, 1};
const static Rgb alarme_color = {1, 0, 0};

// Buffer para armazenar IP do Pico
static char ip_str[24];

/*-----------------------------------------------------------------------------------*/

/*------------------------------------- STRUCTS --------------------------------------*/

struct http_state
{
    char response[4096];
    size_t len;
    size_t sent;
};

/*-----------------------------------------------------------------------------------*/

/*------------------------- FUNÇÕES PERIFÉRICOS (Protótipo) -------------------------*/

bool is_same_color(Rgb color1, Rgb color2); // Identifica se duas cores são iguais
int checar_estado_nivel(float leitura); // Retorna estado do nível atual (OK, ACIMA, ABAIXO)
void matriz_show_lvl(float leitura); // Mostra nível de água na matriz de forma proporcional ao tanque
void atualizar_display(ssd1306_t *ssd, float nivel); // Atualiza o Display OLED
bool buzzer_callback(struct repeating_timer *t); // Callback para temporizador voltado a ligar/desligar buzzer
void gpio_irq_handler(uint gpio, uint32_t events); // Callback da interrupção dos botões

/*-----------------------------------------------------------------------------------*/

/*------------------------ FUNÇÕES INTERFACE WEB (Protótipo) ------------------------*/

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);

/*-----------------------------------------------------------------------------------*/

int main()
{   
     // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(BOTAO_JOY);
    gpio_set_dir(BOTAO_JOY, GPIO_IN);
    gpio_pull_up(BOTAO_JOY);
    gpio_set_irq_enabled_with_callback(BOTAO_JOY, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    //Aqui termina o trecho para modo BOOTSEL com botão B

    // Inicializa os botões A e B
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // --------------------------

    //Inicializa o Potenciometro
    sensor_init(SENSOR_PIN);

    //Inicializa o Rele
    rele_init(RELE_PIN);

    // Inicializa LED verde
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0); // Começa desligado

    // Inicializa a Matriz
    matriz_init();

    // Inicializa o sensor
    sensor_init(SENSOR_PIN);

    // Inicializa o relé
    rele_init(RELE_PIN);

    // Inicializa o Buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_PIN);

    pwm_set_wrap(slice, WRAP);
    pwm_set_clkdiv(slice, DIV_CLK); 
    pwm_set_gpio_level(BUZZER_PIN, 0);
    pwm_set_enabled(slice, true);
    // ---------------------

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

    stdio_init_all();

    // Aguarda alguns segundos para estabilização do Wi-Fi
    sleep_ms(2000);

    // Inicializa o Wi-Fi e o servidor web
    if (cyw43_arch_init())
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
        ssd1306_send_data(&ssd);
        return 1;
    }

    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);

    sleep_ms(500); // Intervalo para visualização do aviso

    start_http_server();
    // ------------------------------------------

    float anterior = -1.0f;

    printf("CÓDIGO INICIADO!\n");

    while (true) {
        // Polling para manter conexão
        cyw43_arch_poll();

        // Obter nível da água (em porcentagem) do reservatório
        nivel = get_nivel(SENSOR_PIN, SENSOR_MAX, SENSOR_MIN);
        calibra_sensor(SENSOR_PIN);

        if (modo == AUTO)
        {
            if (nivel >= limite_max - PUMP_WARN) {
                pump_state = false; // Desiga a bomba se o nível estiver próximo do nível máximo
            } else if (nivel <= limite_min) {
                pump_state = true; // Liga a bomba se o nível estiver abaixo do mínimo
            }
        }
        switch_rele(RELE_PIN, pump_state);

        led_on = gpio_get(LED_GREEN_PIN);
        if (pump_state)
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
        if (status_nivel != NIVEL_OK)
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
            atualizar_display(&ssd, nivel);

            anterior = nivel;
            flag_atualizar = false;
        }

        sleep_ms(500);
    }

    cyw43_arch_deinit();
    return 0;
}

bool is_same_color(Rgb color1, Rgb color2)
{
    return color1.r == color2.r && color1.g == color2.g && color1.b == color2.b;
}

int checar_estado_nivel(float leitura)
{
    if (leitura - limite_max >= TOLERANCIA)
    {
        return NIVEL_ACIMA; // Risco de transbordamento
    }
    else if (limite_min - leitura >= TOLERANCIA)
    {
        return NIVEL_ABAIXO; // Nível abaixo do esperado
    }
    else
    {
        return NIVEL_OK; // Nível OK
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
    if (lines != prev_lines || !is_same_color(matriz_color, prev_matriz_color))
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

void atualizar_display(ssd1306_t *ssd, float nivel)
{
    ssd1306_fill(ssd, false);

    // Desenha frame do menu
    ssd1306_rect(ssd, 3, 3, 122, 60, true, false);
    
    // Escreve os valores dos limites e o valor lido
    char str_a[11];
    char str_b[12];
    char str_c[11];
    char str_d[13];

    snprintf(str_a, 11, "MIN: %.1f%%", limite_min);
    snprintf(str_b, 12, "CUR: %.1f%%", nivel);
    snprintf(str_c, 11, "MAX: %.1f%%", limite_max);
    snprintf(str_d, 13, "Modo: %s", modo == AUTO ? "AUTO" : "MANUAL");
    
    ssd1306_draw_string(ssd, str_a, 8, 6);   // Desenha limite mínimo atual
    ssd1306_draw_string(ssd, str_b, 8, 16);  // Desenha valor lido (em porcentagem)
    ssd1306_draw_string(ssd, str_c, 8, 28);  // Desenha limite máximo atual
    ssd1306_draw_string(ssd, str_d, 8, 41);  // Desenha modo de execução atual
    ssd1306_draw_string(ssd, ip_str, 4, 52); // Desenha o IP para acesso ao servidor

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
    static absolute_time_t last_time_joy = 0;

    absolute_time_t now = get_absolute_time();
    if (gpio == BOTAO_A && modo == MANUAL){ // Botão A só deve ser considerado no modo MANUAL
        if (absolute_time_diff_us(last_time_A, now) > 200000) { //Debouncing
            // Liga/Desliga manualmente o relé da bomba
            pump_state = !pump_state;
            last_time_A = now;
        }
    }else if (gpio == BOTAO_B){
        if (absolute_time_diff_us(last_time_B, now) > 200000) { //Debouncing
            // Alterna entre os modos AUTOMÁTICO e MANUAL
            modo = modo == AUTO ? MANUAL : AUTO;
            flag_atualizar = true;
            last_time_B = now;
        }
    }else if (gpio == BOTAO_JOY){
        if (absolute_time_diff_us(last_time_joy, now) > 200000) { //Debouncing
            // Entra no modo BOOTSEL da placa
            reset_usb_boot(0, 0);
            last_time_joy = now;
        }
    }
}

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /estado")) {
        // adc_select_input(0);
        // uint16_t leitura = adc_to_percentage(adc_read()); // nível da água

        char json_payload[128];
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"level\":%.1f,\"pump\":%d,\"min\":%.1f,\"max\":%.1f}",
            nivel, pump_state, limite_min, limite_max);

        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", json_len, json_payload);
    }
    else if (strstr(req, "GET /config?")) {
        // extrai os parâmetros min e max da query string
        char *min_str = strstr(req, "min=");
        char *max_str = strstr(req, "max=");

        if (min_str && max_str) {
            limite_min = atof(min_str + 4);
            limite_max = atof(max_str + 4);

            if (limite_max < limite_min) {
                limite_max = limite_min + 1;
            }

            char json_config[64];
            int len = snprintf(json_config, sizeof(json_config),
                "{\"min\":%d,\"max\":%d}", limite_min, limite_max);

            hs->len = snprintf(hs->response, sizeof(hs->response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s", len, json_config);
        } else {
            const char *erro = "Parametros invalidos";
            hs->len = snprintf(hs->response, sizeof(hs->response),
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "\r\n"
                "%s", (int)strlen(erro), erro);
        }
    }
    else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}


static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}
