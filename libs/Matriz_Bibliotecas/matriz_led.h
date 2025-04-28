#ifndef MATRIZ_LED_H
#define MATRIZ_LED_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "generated/ws2812.pio.h"

#define PINO_WS2812 7   // Pino GPIO 
#define NUM_LINHAS    5   // Dimensão da matriz
#define NUM_COLUNAS   5   // Dimensão da matriz
#define NUM_PIXELS    (NUM_LINHAS * NUM_COLUNAS) // Total 25
#define RGBW_ATIVO    false // Se os LEDs são RGBW ou RGB

void inicializar_matriz_led();
// Parâmetros: Ponteiros para os nomes das cores (ex: "Vermelho")
void mostrar_faixas_cores(const char *cor_faixa1, const char *cor_faixa2, const char *cor_faixa3);
// Função para desligar todos os LEDs da matriz
void desligar_matriz();
#endif // MATRIZ_LED_H