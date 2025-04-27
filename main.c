/*
 * Ohmímetro E24 com interface otimizada no OLED SSD1306 e Matriz LED WS2812
 * Placa: Raspberry Pi Pico
 */

 #include <stdio.h>
 #include <math.h>
 #include <float.h>
 #include <string.h>           // strlen
 #include "pico/stdlib.h"
 #include "hardware/adc.h"
 #include "hardware/i2c.h"
 #include "libs/Display_Bibliotecas/ssd1306.h"
 #include "libs/Display_Bibliotecas/font.h" // Assume fonte 8x8
 #include "libs/Matriz_Bibliotecas/matriz_led.h" // <<< INCLUÍDO

 // Definições de hardware
 #define I2C_PORT      i2c1
 #define I2C_SDA_PIN   14
 #define I2C_SCL_PIN   15
 #define OLED_ADDR     0x3C
 #define ADC_PIN       28       // GPIO28 → ADC2
 #define RESISTOR_CONHECIDO 10000.0f // 10 kΩ
 #define RESOLUCAO_ADC 4095.0f  // 12-bit

 // Constantes da Interface OLED
 #define LARGURA_OLED    128
 #define ALTURA_OLED     64
 #define LARGURA_FONTE   8 // Largura real de um caractere 8x8
 #define ALTURA_FONTE    8  // Altura da fonte
 #define ESPACAMENTO     2  // Espaçamento da borda OLED
 #define ESPACO_LINHA    ALTURA_FONTE
 #define SIMBOLO_OHM     127 // Caractere usado para representar Ω (Ohm)
 #define POSICAO_VALOR_X 80  // Posição X fixa para início dos valores no OLED

 // Série E24
 static const float VALORES_E24[24] = {
     10, 11, 12, 13, 15, 16, 18, 20,
     22, 24, 27, 30, 33, 36, 39, 43,
     47, 51, 56, 62, 68, 75, 82, 91
 };

 // Nomes das cores (usado por OLED e Matriz)
 static const char *NOMES_CORES[10] = {
     "Prata", "Ouro", "Preto", "Marrom",
     "Vermelho", "Laranja", "Amarelo",
     "Verde", "Azul", "Violeta"
 };

 // Função para inicializar o hardware
 void inicializar_hardware() {
     stdio_init_all();
     // I2C
     i2c_init(I2C_PORT, 400000);
     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA_PIN);
     gpio_pull_up(I2C_SCL_PIN);
     // ADC
     adc_init();
     adc_gpio_init(ADC_PIN);
     // Matriz LED <<< ADICIONADO
     inicializar_matriz_led();
 }

 // Função para ler o valor do ADC
 float ler_adc() {
     adc_select_input(2);
     float soma = 0;
     for (int i = 0; i < 100; ++i) {
         soma += adc_read();
         sleep_us(100);
     }
     return soma / 100.0f;
 }

 // Função para calcular a resistência medida
 float calcular_resistencia(float valor_adc) {
     if (valor_adc < RESOLUCAO_ADC - 1) {
         return (RESISTOR_CONHECIDO * valor_adc) / (RESOLUCAO_ADC - valor_adc);
     } else {
         return INFINITY;
     }
 }

 // Função para aproximar a resistência medida ao valor mais próximo da série E24
 bool aproximar_e24(float resistencia, float *melhor_valor, int *melhor_exp, int *melhor_idx) {
     float melhor_diferenca = FLT_MAX;
     bool encontrado = false; // Retorna se um valor foi encontrado

     if (isfinite(resistencia) && resistencia > 0) {
         for (int e = 0; e <= 6; ++e) { // Expoentes 0 a 6 (Ohm a MOhm)
             for (int i = 0; i < 24; ++i) { // Bases E24
                 float candidato = VALORES_E24[i] * powf(10, e);
                 float diferenca = fabsf(logf(candidato) - logf(resistencia)); // Comparação logarítmica
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
     // Se não encontrou, zera os valores de saída (opcional)
     if (!encontrado) {
         *melhor_valor = 0;
         *melhor_exp = -1; // Indica erro/não encontrado
         *melhor_idx = -1;
     }
     return encontrado; // Retorna true se encontrou, false caso contrário
 }

 // Função para determinar as cores das faixas do resistor
 void determinar_cores(int idx_e24_base, int expoente, const char **cores) {
     // Usa os índices da paleta NOMES_CORES (índice + 2)
     int base = (int)VALORES_E24[idx_e24_base];
     int d1 = base / 10; // Primeiro dígito
     int d2 = base % 10; // Segundo dígito
     int multiplicador = expoente; // Expoente é o índice do multiplicador

     // Mapeia índices para nomes de cores, tratando limites
     cores[0] = (d1 >= 0 && d1 <= 7) ? NOMES_CORES[d1 + 2] : "---";
     cores[1] = (d2 >= 0 && d2 <= 7) ? NOMES_CORES[d2 + 2] : "---";
     cores[2] = (multiplicador >= 0 && multiplicador <= 7) ? NOMES_CORES[multiplicador + 2] : "---";
 }

 // Função para atualizar o display OLED
 void atualizar_display_oled(ssd1306_t *oled, float valor_adc, float resistencia, float melhor_valor_e24, const char **cores) {
     ssd1306_fill(oled, false); // Limpa o display

     char buffer[25]; // Buffer para strings formatadas
     uint8_t y = ESPACAMENTO; // Posição Y inicial

     // --- Linha 1: Valor ADC ---
     snprintf(buffer, sizeof(buffer), "%.0f", valor_adc);
     ssd1306_draw_string(oled, "ADC:", ESPACAMENTO, y, false);
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;

     // --- Linha 2: Resistor Conhecido (sem Ohm) ---
     snprintf(buffer, sizeof(buffer), "%.0f", RESISTOR_CONHECIDO);
     ssd1306_draw_string(oled, "R Fixo:", ESPACAMENTO, y, false);
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;

     // --- Linha 3: Resistência Medida (com Ohm) ---
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

     // --- Linha 4: Resistência Comercial E24 (com Ohm) ---
     ssd1306_draw_string(oled, "R E24:", ESPACAMENTO, y, false); // Label curto
     if (melhor_valor_e24 > 0) { // Usa o valor retornado por aproximar_e24
         snprintf(buffer, sizeof(buffer), "%.0f %c", melhor_valor_e24, SIMBOLO_OHM);
     } else {
         strcpy(buffer, "---"); // Se não encontrou E24
     }
     ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
     y += ESPACO_LINHA;

     // --- Separador Horizontal ---
     // Verifica se há espaço para a linha e as cores abaixo
     if (y > ALTURA_OLED - 3 - (3 * ALTURA_FONTE)) {
         y = ALTURA_OLED - 3 - (3 * ALTURA_FONTE); // Ajusta Y para caber tudo
     }
     ssd1306_hline(oled, ESPACAMENTO, LARGURA_OLED - 1 - ESPACAMENTO, y, true);
     y += 3; // Espaço após a linha

     // --- Exibir Faixas de Cor ---
     const char *rotulos[3] = {"1a:", "2a:", "3a:"};
     int posicao_nome_cor = ESPACAMENTO + (strlen(rotulos[0]) * LARGURA_FONTE) + 2; // Posição X do nome da cor

     for (int i = 0; i < 3 && (y + ALTURA_FONTE) <= ALTURA_OLED; ++i) {
         ssd1306_draw_string(oled, rotulos[i], ESPACAMENTO, y, false); // Desenha "1a:", "2a:", "3a:"
         ssd1306_draw_string(oled, cores[i], posicao_nome_cor, y, false); // Desenha o nome da cor
         y += ESPACO_LINHA; // Próxima linha
     }

     ssd1306_send_data(oled); // Envia os dados para o display OLED
 }

 int main(void) {
     inicializar_hardware(); // Inicializa ADC, I2C, Matriz LED

     ssd1306_t oled;
     ssd1306_init(&oled, LARGURA_OLED, ALTURA_OLED, false, OLED_ADDR, I2C_PORT);
     ssd1306_config(&oled);

     while (true) {
         float valor_adc = ler_adc();
         float resistencia = calcular_resistencia(valor_adc);

         float melhor_valor_e24 = 0; // Inicializa com 0
         int melhor_exp = -1, melhor_idx = -1;
         bool e24_encontrado = aproximar_e24(resistencia, &melhor_valor_e24, &melhor_exp, &melhor_idx);

         const char *cores[3] = {"---", "---", "---"}; // Padrão
         if (e24_encontrado) {
             // Passa o índice e expoente encontrados para determinar cores
             determinar_cores(melhor_idx, melhor_exp, cores);
         }

         // Atualiza o display OLED
         atualizar_display_oled(&oled, valor_adc, resistencia, melhor_valor_e24, cores);

         // Atualiza a Matriz LED <<< ADICIONADO
         if (e24_encontrado) {
             mostrar_faixas_cores(cores[0], cores[1], cores[2]);
         } else {
             desligar_matriz(); // Apaga a matriz se nenhum resistor E24 foi encontrado
         }

         sleep_ms(200); // Pausa principal do loop
     }

     return 0;
 }