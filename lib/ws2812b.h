#ifndef MATRIZ
#define MATRIZ

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// Tamanho da matriz de LEDs
#define MATRIZ_ROWS 5
#define MATRIZ_COLS 5
#define MATRIZ_SIZE MATRIZ_ROWS * MATRIZ_COLS

// Estrutura para armazenar um pixel com valores RGB
typedef struct Rgb
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} Rgb;

void matriz_init();
void matriz_clear();
void matriz_send_data();

void matriz_draw_linha(uint pontoInit, uint linha, uint n, Rgb cor);
void matriz_draw_coluna(uint pontoInit, uint coluna, uint n, Rgb cor);
void matriz_draw_frame(Rgb frame[MATRIZ_ROWS][MATRIZ_COLS]);

#endif