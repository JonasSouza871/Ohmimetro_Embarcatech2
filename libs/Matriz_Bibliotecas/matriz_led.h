#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
// Certifique-se que o arquivo ws2812.pio está no diretório correto e
// que o CMakeLists.txt está configurado para gerá-lo.
#include "generated/ws2812.pio.h"

#define PINO_WS2812 7   // Pino GPIO conectado ao DIN dos LEDs
#define NUM_LINHAS    5   // Dimensão da matriz
#define NUM_COLUNAS   5   // Dimensão da matriz
#define NUM_PIXELS    (NUM_LINHAS * NUM_COLUNAS) // Total 25
#define RGBW_ATIVO    false // Se os LEDs são RGBW ou RGB

// Função para inicializar a PIO para controlar os LEDs WS2812
void inicializar_matriz_led();

// Função interna para enviar um único pixel (não precisa ser exposta no .h geralmente)
// void enviar_pixel(uint32_t pixel_grb);

// --- NOVA FUNÇÃO ---
// Mostra as 3 cores das faixas nas linhas 1, 3 e 5 da matriz
// As outras linhas ficam apagadas.
// Parâmetros: Ponteiros para os nomes das cores (ex: "Vermelho")
void mostrar_faixas_cores(const char *cor_faixa1, const char *cor_faixa2, const char *cor_faixa3);

// Função para desligar todos os LEDs da matriz
void desligar_matriz();

#endif // MATRIZ_LED_H