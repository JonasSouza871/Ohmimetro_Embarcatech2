/*
 * Ohmímetro E24 com interface otimizada no OLED SSD1306
 * Placa: Raspberry Pi Pico
 */

 #include <stdio.h>
 #include <math.h>
 #include <float.h>
 #include <string.h> // Necessário para strlen
 #include "pico/stdlib.h"
 #include "hardware/adc.h"
 #include "hardware/i2c.h"
 #include "libs/Display_Bibliotecas/ssd1306.h"
 #include "libs/Display_Bibliotecas/font.h" // Assumindo que a fonte tem largura ~6px

 #define I2C_PORT      i2c1
 #define I2C_SDA_PIN   14
 #define I2C_SCL_PIN   15
 #define OLED_ADDR     0x3C
 #define ADC_PIN       28       // GPIO28 → ADC2
 #define R_KNOWN       10000.0f // Valor do resistor conhecido (10k Ohms)
 #define ADC_RES       4095.0f // Resolução máxima do ADC (12-bit)

 // --- Constantes da Interface ---
 #define OLED_WIDTH    128
 #define OLED_HEIGHT   64
 #define FONT_WIDTH    6  // Largura aproximada de um caractere da fonte (ajuste se necessário)
 #define FONT_HEIGHT   8  // Altura da fonte
 #define PADDING       2  // Espaçamento da borda
 #define LINE_SPACING  FONT_HEIGHT // Espaço entre linhas de texto (igual à altura da fonte)
 // -----------------------------

 // Valores base da série E24
 static const float E24_BASE[24] = {
     10,11,12,13,15,16,18,20,
     22,24,27,30,33,36,39,43,
     47,51,56,62,68,75,82,91
 };

 /* Nomes completos das cores para as faixas */
 static const char *COLOR_NAME[10] = {
     "prata",    // (-2) - Não usado neste código atualmente
     "ouro",     // (-1) - Não usado neste código atualmente
     "preto",    // (0)
     "marrom",   // (1)
     "vermelho", // (2)
     "laranja",  // (3)
     "amarelo",  // (4)
     "verde",    // (5)
     "azul",     // (6)
     "violeta"   // (7)
     // Índices 8 e 9 (Cinza, Branco) não mapeados aqui
 };

 // Função auxiliar para desenhar texto com alinhamento à direita na tela
 void ssd1306_draw_string_right_aligned(ssd1306_t *p, const char *text, uint8_t y, bool color) {
     int text_width = strlen(text) * FONT_WIDTH; // Calcula a largura do texto em pixels
     int x_pos = OLED_WIDTH - PADDING - text_width; // Calcula a posição X para alinhar à direita
     if (x_pos < PADDING) x_pos = PADDING; // Garante que não saia da tela à esquerda
     ssd1306_draw_string(p, text, x_pos, y, color); // Desenha a string
 }

 int main(void) {
     stdio_init_all(); // Inicializa stdio (para debug, se necessário)

     // Inicializa I2C e OLED
     i2c_init(I2C_PORT, 400000); // Inicializa I2C1 a 400kHz
     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C); // Define pino SDA
     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C); // Define pino SCL
     gpio_pull_up(I2C_SDA_PIN); // Habilita pull-up interno no SDA
     gpio_pull_up(I2C_SCL_PIN); // Habilita pull-up interno no SCL
     ssd1306_t oled; // Cria a estrutura do OLED
     // Inicializa o OLED 128x64, sem rotação, com endereço e porta I2C definidos
     ssd1306_init(&oled, OLED_WIDTH, OLED_HEIGHT, false, OLED_ADDR, I2C_PORT);
     ssd1306_config(&oled); // Aplica configurações padrão ao OLED

     // Inicializa ADC
     adc_init(); // Inicializa o módulo ADC
     adc_gpio_init(ADC_PIN); // Habilita a função ADC no pino GPIO28

     while (true) { // Loop principal infinito
         // --- Leitura ADC ---
         adc_select_input(2); // Seleciona o canal ADC 2 (GPIO28)
         float sum = 0;
         // Faz múltiplas leituras para obter uma média e reduzir ruído
         for (int i = 0; i < 100; ++i) {
            sum += adc_read(); // Lê o valor do ADC
            sleep_us(100); // Pequeno delay entre leituras
         }
         float adc_val = sum / 100.0f; // Calcula a média das leituras

         // --- Cálculo da Resistência Medida ---
         float r_meas = 0.0f;
         // Verifica se a leitura ADC não está no máximo (evita divisão por zero/infinito)
         if (adc_val < ADC_RES - 1) { // Subtrai 1 como margem de segurança
            // Fórmula do divisor de tensão: R_medido = (R_conhecido * V_medido) / (V_total - V_medido)
            // Aqui, V_medido é proporcional a adc_val e V_total a ADC_RES
            r_meas = (R_KNOWN * adc_val) / (ADC_RES - adc_val);
         } else {
            r_meas = INFINITY; // Se ADC no máximo, considera circuito aberto (resistência infinita)
         }

         // --- Aproximação para Série E24 ---
         float best_val = 0;          // Melhor valor E24 encontrado
         float best_diff = FLT_MAX;   // Menor diferença encontrada (inicializa com valor máximo)
         int best_exp = 0;            // Expoente do melhor valor (para as cores)
         int best_idx = 0;            // Índice na E24_BASE do melhor valor (para as cores)
         bool found_e24 = false;      // Flag para indicar se um valor E24 razoável foi encontrado

         // Só procura valor E24 se a medição for válida (finita e positiva)
         if (isfinite(r_meas) && r_meas > 0) {
             // Testa diferentes ordens de magnitude (10^0 a 10^6 -> Ohms a MegaOhms)
             for (int exp = 0; exp <= 6; ++exp) {
                 // Testa cada valor base da série E24
                 for (int i = 0; i < 24; ++i) {
                     // Calcula o valor candidato E24 (base * 10^expoente)
                     float cand = E24_BASE[i] * powf(10, exp);
                     // Calcula a diferença em escala logarítmica (melhor para resistores)
                     float diff = fabsf(logf(cand) - logf(r_meas));
                     // Se a diferença atual for menor que a melhor encontrada até agora...
                     if (diff < best_diff) {
                         best_diff = diff; // Atualiza a melhor diferença
                         best_val  = cand; // Atualiza o melhor valor E24
                         best_exp  = exp;  // Guarda o expoente
                         best_idx  = i;    // Guarda o índice da base E24
                         found_e24 = true; // Marca que um valor foi encontrado
                     }
                 }
             }
         }

         // --- Determinação das Cores das Faixas ---
         const char *cores[3] = {"---", "---", "---"}; // Valores padrão caso não encontre E24
         if (found_e24) {
             int base = (int)E24_BASE[best_idx]; // Pega o valor base (ex: 47 para 4.7k)
             int dig1_idx = (base / 10);         // Índice da cor do primeiro dígito
             int dig2_idx = (base % 10);         // Índice da cor do segundo dígito
             int mult_idx = best_exp;            // Índice da cor do multiplicador

             // Mapeia os índices para os nomes das cores, adicionando 2 (offset no array COLOR_NAME)
             // e verificando os limites do array (0 a 7 para preto a violeta)
             if (dig1_idx >= 0 && dig1_idx <= 7) cores[0] = COLOR_NAME[dig1_idx + 2];
             if (dig2_idx >= 0 && dig2_idx <= 7) cores[1] = COLOR_NAME[dig2_idx + 2];
             if (mult_idx >= 0 && mult_idx <= 7) cores[2] = COLOR_NAME[mult_idx + 2];
             // Nota: Multiplicadores prata (-2) e ouro (-1) não estão implementados aqui.
         }


         // --- Desenho da Interface no OLED ---
         ssd1306_fill(&oled, false); // Limpa o buffer do display (preenche com preto)

         char buf[20]; // Buffer para formatar strings
         uint8_t current_y = PADDING; // Posição Y inicial para desenhar (começa no topo com padding)

         // Linha 1: Valor ADC
         snprintf(buf, sizeof buf, "%.0f", adc_val); // Formata o valor ADC como inteiro
         ssd1306_draw_string(&oled, "ADC:", PADDING, current_y, true); // Desenha o label à esquerda
         ssd1306_draw_string_right_aligned(&oled, buf, current_y, true); // Desenha o valor à direita
         current_y += LINE_SPACING; // Move a posição Y para a próxima linha

         // Linha 2: Resistor Fixo (R_KNOWN)
         snprintf(buf, sizeof buf, "%.0f", R_KNOWN); // Formata o valor de R_KNOWN
         ssd1306_draw_string(&oled, "R Fixo:", PADDING, current_y, true);
         ssd1306_draw_string_right_aligned(&oled, buf, current_y, true);
         current_y += LINE_SPACING;

         // Linha 3: Resistência Medida (R_MEAS)
         if (isinf(r_meas)) {
             snprintf(buf, sizeof buf, "Aberto"); // Se infinito, mostra "Aberto"
         } else if (r_meas < 1.0 && r_meas > 0) {
             snprintf(buf, sizeof buf, "%.2f", r_meas); // Mostra decimais para < 1 Ohm
         } else if (r_meas == 0) { // Pode indicar curto-circuito
             snprintf(buf, sizeof buf, "0");
         } else {
             snprintf(buf, sizeof buf, "%.0f", r_meas); // Formato inteiro para >= 1 Ohm
         }
         ssd1306_draw_string(&oled, "R Medido:", PADDING, current_y, true);
         ssd1306_draw_string_right_aligned(&oled, buf, current_y, true);
         current_y += LINE_SPACING;

         // Linha 4: Resistência Comercial (E24)
         if (found_e24) {
             // ****************************************************************
             // * ALTERAÇÃO AQUI: Removida a formatação com 'k' e 'M'         *
             // * Sempre mostra o valor E24 completo como número inteiro.      *
             // ****************************************************************
             snprintf(buf, sizeof buf, "%.0f", best_val); // Formata sempre como inteiro
         } else {
             snprintf(buf, sizeof buf, "---"); // Mostra "---" se não encontrou E24
         }
         ssd1306_draw_string(&oled, "R Comercial:", PADDING, current_y, true); // Desenha o label (R Comercial:)
         ssd1306_draw_string_right_aligned(&oled, buf, current_y, true); // Desenha o valor E24 formatado (ou "---")
         current_y += LINE_SPACING; // Move Y para a posição da linha separadora


         // --- Separador Horizontal ---
         uint8_t line_y = current_y; // Posição Y da linha (logo abaixo do último texto)
         // Desenha a linha horizontal com padding nas laterais
         ssd1306_hline(&oled, PADDING, OLED_WIDTH - 1 - PADDING, line_y, true);
         current_y = line_y + 3; // Move Y para baixo da linha, com um pequeno espaço


         // --- Faixas de Cor ---
         const char* label1 = "1a:"; // Label da primeira faixa
         const char* label2 = "2a:"; // Label da segunda faixa
         const char* label3 = "3a:"; // Label da terceira faixa
         // Calcula a largura dos labels (assumindo que são iguais)
         int label_width = strlen(label1) * FONT_WIDTH;
         // Calcula a posição X para os nomes das cores (logo após o label + pequeno espaço)
         int color_name_x = PADDING + label_width + FONT_WIDTH / 2;

         // Garante que a posição X do nome da cor não saia da tela se for muito longa
         if (color_name_x > OLED_WIDTH - (5 * FONT_WIDTH)) { // Estima 5 chars para o nome da cor
              color_name_x = PADDING + label_width + 1; // Deixa só 1 pixel de espaço se apertado
         }

         // Desenha a 1ª Faixa
         ssd1306_draw_string(&oled, label1, PADDING, current_y, true); // Desenha "1a:"
         ssd1306_draw_string(&oled, cores[0], color_name_x, current_y, true); // Desenha o nome da cor
         current_y += LINE_SPACING; // Move Y para a próxima linha

         // Desenha a 2ª Faixa
         ssd1306_draw_string(&oled, label2, PADDING, current_y, true); // Desenha "2a:"
         ssd1306_draw_string(&oled, cores[1], color_name_x, current_y, true); // Desenha o nome da cor
         current_y += LINE_SPACING; // Move Y para a próxima linha

         // Desenha a 3ª Faixa (apenas se couber na tela)
         if (current_y <= (OLED_HEIGHT - FONT_HEIGHT)) { // Verifica se há espaço vertical
            ssd1306_draw_string(&oled, label3, PADDING, current_y, true); // Desenha "3a:"
            ssd1306_draw_string(&oled, cores[2], color_name_x, current_y, true); // Desenha o nome da cor
         }

         // --- Atualização do Display ---
         ssd1306_send_data(&oled); // Envia o buffer desenhado para o display OLED
         sleep_ms(200); // Pausa para evitar atualização muito rápida e permitir leitura
     }

     return 0; // Esta linha nunca será alcançada devido ao loop infinito
 }