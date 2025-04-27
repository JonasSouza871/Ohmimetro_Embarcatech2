/*
 * Ohmímetro E24 com interface otimizada no OLED SSD1306
 * Placa: Raspberry Pi Pico
 */

 #include <stdio.h>
 #include <math.h>
 #include <float.h>
 #include <string.h>
 #include "pico/stdlib.h"
 #include "hardware/adc.h"
 #include "hardware/i2c.h"
 #include "libs/Display_Bibliotecas/ssd1306.h"
 #include "libs/Display_Bibliotecas/font.h" // Assume fonte 8x8
 
 // Definições de hardware
 #define I2C_PORT      i2c1
 #define I2C_SDA_PIN   14
 #define I2C_SCL_PIN   15
 #define OLED_ADDR     0x3C
 #define ADC_PIN       28       // GPIO28 → ADC2
 #define RESISTOR_CONHECIDO 10000.0f // 10 kΩ
 #define RESOLUCAO_ADC 4095.0f  // 12-bit
 
 // Constantes da Interface
 #define LARGURA_OLED    128
 #define ALTURA_OLED     64
 #define LARGURA_FONTE   8 // Largura real de um caractere 8x8
 #define ALTURA_FONTE    8  // Altura da fonte
 #define ESPACAMENTO     2  // Espaçamento da borda
 #define ESPACO_LINHA    ALTURA_FONTE
 #define SIMBOLO_OHM     127 // Caractere usado para representar Ω (Ohm)
 #define POSICAO_VALOR_X 80  // Posição X fixa para início dos valores
 
 // Série E24
 static const float VALORES_E24[24] = {
     10, 11, 12, 13, 15, 16, 18, 20,
     22, 24, 27, 30, 33, 36, 39, 43,
     47, 51, 56, 62, 68, 75, 82, 91
 };
 
 // Nomes das cores
 static const char *NOMES_CORES[10] = {
     "Prata", "Ouro", "Preto", "Marrom",
     "Vermelho", "Laranja", "Amarelo",
     "Verde", "Azul", "Violeta"
 };
 
 // Função para inicializar o hardware
 void inicializar_hardware() {
     stdio_init_all(); // Inicializa a comunicação serial
     i2c_init(I2C_PORT, 400000); // Inicializa a comunicação I2C
     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C); // Configura o pino SDA para I2C
     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C); // Configura o pino SCL para I2C
     gpio_pull_up(I2C_SDA_PIN); // Habilita o pull-up no pino SDA
     gpio_pull_up(I2C_SCL_PIN); // Habilita o pull-up no pino SCL
     adc_init(); // Inicializa o ADC
     adc_gpio_init(ADC_PIN); // Configura o pino do ADC
 }
 
 // Função para ler o valor do ADC
 float ler_adc() {
     adc_select_input(2); // Seleciona o canal ADC
     float soma = 0;
     for (int i = 0; i < 100; ++i) {
         soma += adc_read(); // Lê o valor do ADC
         sleep_us(100); // Espera 100 microsegundos
     }
     return soma / 100.0f; // Retorna a média dos valores lidos
 }
 
 // Função para calcular a resistência medida
 float calcular_resistencia(float valor_adc) {
     if (valor_adc < RESOLUCAO_ADC - 1) {
         return (RESISTOR_CONHECIDO * valor_adc) / (RESOLUCAO_ADC - valor_adc);
     } else {
         return INFINITY; // Retorna infinito se o valor do ADC estiver no máximo
     }
 }
 
 // Função para aproximar a resistência medida ao valor mais próximo da série E24
 void aproximar_e24(float resistencia, float *melhor_valor, int *melhor_exp, int *melhor_idx) {
     float melhor_diferenca = FLT_MAX;
     bool encontrado = false;
 
     if (isfinite(resistencia) && resistencia > 0) {
         for (int e = 0; e <= 6; ++e) {
             for (int i = 0; i < 24; ++i) {
                 float candidato = VALORES_E24[i] * powf(10, e);
                 float diferenca = fabsf(logf(candidato) - logf(resistencia));
                 if (diferenca < melhor_diferenca) {
                     melhor_diferenca = diferenca;
                     *melhor_valor = candidato;
                     *melhor_exp = e;
                     *melhor_idx = i;
                     encontrado = true;
                 }
             }
         }
     }
 
     if (!encontrado) {
         *melhor_valor = 0;
         *melhor_exp = 0;
         *melhor_idx = 0;
     }
 }
 
 // Função para determinar as cores das faixas do resistor
 void determinar_cores(int d1, int d2, int multiplicador, const char **cores) {
     cores[0] = (d1 >= 0 && d1 <= 7) ? NOMES_CORES[d1 + 2] : "---";
     cores[1] = (d2 >= 0 && d2 <= 7) ? NOMES_CORES[d2 + 2] : "---";
     cores[2] = (multiplicador >= 0 && multiplicador <= 7) ? NOMES_CORES[multiplicador + 2] : "---";
 }
 
 // Função para atualizar o display OLED
 void atualizar_display(ssd1306_t *oled, float valor_adc, float resistencia, float melhor_valor_e24, const char **cores) {
     ssd1306_fill(oled, false); // Limpa o display
 
     char buffer[25];
     uint8_t y = ESPACAMENTO;
 
     // Exibir valor do ADC
     snprintf(buffer, sizeof(buffer), "%.0f", valor_adc);
     ssd1306_draw_string(oled, "ADC:", ESPACAMENTO, y, false);
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;
 
     // Exibir resistor conhecido
     snprintf(buffer, sizeof(buffer), "%.0f", RESISTOR_CONHECIDO);
     ssd1306_draw_string(oled, "R Fixo:", ESPACAMENTO, y, false);
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;
 
     // Exibir resistencia medida
     ssd1306_draw_string(oled, "R Medido:", ESPACAMENTO, y, false);
     if (isinf(resistencia)) {
         strcpy(buffer, "Aberto");
     } else if (resistencia < 1.0f && resistencia > 0.0f) {
         snprintf(buffer, sizeof(buffer), "%.2f %c", resistencia, SIMBOLO_OHM);
     } else if (resistencia == 0.0f) {
         snprintf(buffer, sizeof(buffer), "0 %c", SIMBOLO_OHM);
     } else {
         snprintf(buffer, sizeof(buffer), "%.0f %c", resistencia, SIMBOLO_OHM);
     }
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;
 
     // Exibir resistencia comercial E24
     ssd1306_draw_string(oled, "R E24:", ESPACAMENTO, y, false);
     if (melhor_valor_e24 > 0) {
         snprintf(buffer, sizeof(buffer), "%.0f %c", melhor_valor_e24, SIMBOLO_OHM);
     } else {
         strcpy(buffer, "---");
     }
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;
 
     // Separador
     if (y > ALTURA_OLED - 3 - (3 * ALTURA_FONTE)) {
         y = ALTURA_OLED - 3 - (3 * ALTURA_FONTE);
     }
     ssd1306_hline(oled, ESPACAMENTO, LARGURA_OLED - 1 - ESPACAMENTO, y, true);
     y += 3;
 
     // Exibir faixas de cor
     const char *rotulos[3] = {"1a:", "2a:", "3a:"};
     int posicao_nome_cor = ESPACAMENTO + (strlen(rotulos[0]) * LARGURA_FONTE) + 2;
 
     for (int i = 0; i < 3 && (y + ALTURA_FONTE) <= ALTURA_OLED; ++i) {
         ssd1306_draw_string(oled, rotulos[i], ESPACAMENTO, y, false);
         ssd1306_draw_string(oled, cores[i], posicao_nome_cor, y, false);
         y += ESPACO_LINHA;
     }
 
     ssd1306_send_data(oled); // Envia os dados para o display
 }
 
 int main(void) {
     inicializar_hardware(); // Inicializa o hardware
 
     ssd1306_t oled;
     ssd1306_init(&oled, LARGURA_OLED, ALTURA_OLED, false, OLED_ADDR, I2C_PORT); // Inicializa o display OLED
     ssd1306_config(&oled); // Configura o display OLED
 
     while (true) {
         float valor_adc = ler_adc(); // Lê o valor do ADC
         float resistencia = calcular_resistencia(valor_adc); // Calcula a resistência medida
 
         float melhor_valor_e24;
         int melhor_exp, melhor_idx;
         aproximar_e24(resistencia, &melhor_valor_e24, &melhor_exp, &melhor_idx); // Aproxima a resistência ao valor E24
 
         const char *cores[3];
         if (melhor_valor_e24 > 0) {
             int base = (int)VALORES_E24[melhor_idx];
             int d1 = base / 10, d2 = base % 10;
             determinar_cores(d1, d2, melhor_exp, cores); // Determina as cores das faixas
         } else {
             cores[0] = cores[1] = cores[2] = "---";
         }
 
         atualizar_display(&oled, valor_adc, resistencia, melhor_valor_e24, cores); // Atualiza o display
         sleep_ms(200); // Espera 200 milissegundos
     }
 
     return 0;
 }
 