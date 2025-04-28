#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "libs/Display_Bibliotecas/ssd1306.h"
#include "libs/Display_Bibliotecas/font.h"
#include "libs/Matriz_Bibliotecas/matriz_led.h"

// Definições de hardware
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define OLED_ADDR 0x3C
#define ADC_PIN 28
#define RESISTOR_CONHECIDO 10000.0f // Resistor conhecido de 10 kΩ
#define RESOLUCAO_ADC 4095.0f  // Resolução do ADC (12-bit)

// Constantes da Interface OLED
#define LARGURA_OLED 128
#define ALTURA_OLED 64
#define LARGURA_FONTE 8
#define ALTURA_FONTE 8
#define ESPACAMENTO 2
#define ESPACO_LINHA ALTURA_FONTE
#define SIMBOLO_OHM 127 // Caractere usado para representar Ω (Ohm)
#define POSICAO_VALOR_X 80

// Série E24 de valores de resistores
static const float VALORES_E24[24] = {
    10, 11, 12, 13, 15, 16, 18, 20,
    22, 24, 27, 30, 33, 36, 39, 43,
    47, 51, 56, 62, 68, 75, 82, 91
};

// Nomes das cores das faixas dos resistores
static const char *NOMES_CORES[12] = {
    "Prata", "Ouro", "Preto", "Marrom",
    "Vermelho", "Laranja", "Amarelo",
    "Verde", "Azul", "Violeta", "Cinza","Branco"
};

// Inicializa o hardware (I2C, ADC, Matriz LED)
void inicializar_hardware() {
    stdio_init_all(); // Inicializa a comunicação serial
    i2c_init(I2C_PORT, 400000); // Inicializa o I2C
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C); // Configura o pino SDA
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C); // Configura o pino SCL
    gpio_pull_up(I2C_SDA_PIN); // Habilita o pull-up no pino SDA
    gpio_pull_up(I2C_SCL_PIN); // Habilita o pull-up no pino SCL
    adc_init(); // Inicializa o ADC
    adc_gpio_init(ADC_PIN); // Configura o pino do ADC
    inicializar_matriz_led(); // Inicializa a matriz LED
}

// Lê o valor do ADC e retorna a média de 100 leituras
float ler_adc() {
    adc_select_input(2); // Seleciona o canal ADC
    float soma = 0;
    for (int i = 0; i < 100; ++i) {
        soma += adc_read(); // Lê o valor do ADC
        sleep_us(100); // Espera 100 microsegundos
    }
    return soma / 100.0f; // Retorna a média das leituras
}

// Calcula a resistência com base no valor do ADC
float calcular_resistencia(float valor_adc) {
    if (valor_adc >= RESOLUCAO_ADC - 1) {
        return INFINITY; // Se o valor do ADC estiver no máximo, retorna infinito
    }
    return (RESISTOR_CONHECIDO * valor_adc) / (RESOLUCAO_ADC - valor_adc); // Calcula a resistência
}

// Aproxima a resistência medida ao valor mais próximo da série E24
bool aproximar_e24(float resistencia, float *melhor_valor, int *melhor_exp, int *melhor_idx) {
    float melhor_diferenca = FLT_MAX;
    bool encontrado = false;

    if (isfinite(resistencia) && resistencia > 0) {
        for (int e = 0; e <= 6; ++e) { // Expoentes de 0 a 6 (Ohm a MOhm)
            for (int i = 0; i < 24; ++i) { // Valores da série E24
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

    if (!encontrado) {
        *melhor_valor = 0;
        *melhor_exp = -1;
        *melhor_idx = -1;
    }
    return encontrado;
}

// Determina as cores das faixas do resistor
void determinar_cores(int idx_e24_base, int expoente, const char **cores) {
    int base = (int)VALORES_E24[idx_e24_base]; // Obtém o valor base da série E24
    int d1 = base / 10; // Primeiro dígito
    int d2 = base % 10; // Segundo dígito

    // Determina a cor da primeira faixa
    cores[0] = (d1 >= 0 && d1 <= 9) ? NOMES_CORES[d1 + 2] : "---";
    //De 0 até 9 faixas de cores definidas acima
    // Determina a cor da segunda faixa
    cores[1] = (d2 >= 0 && d2 <= 9) ? NOMES_CORES[d2 + 2] : "---";

    // Determina a cor da terceira faixa (multiplicador)
    cores[2] = (expoente >= 0 && expoente <= 7) ? NOMES_CORES[expoente + 2] : "---";
}

// Atualiza o display OLED com os valores lidos e calculados
void atualizar_display_oled(ssd1306_t *oled, float valor_adc, float resistencia, float melhor_valor_e24, const char **cores) {
    ssd1306_fill(oled, false); // Limpa o display

    char buffer[25]; // Buffer para strings formatadas
    uint8_t y = ESPACAMENTO; // Posição Y inicial

    // Linha 1: Valor ADC
    snprintf(buffer, sizeof(buffer), "%.0f", valor_adc);
    ssd1306_draw_string(oled, "ADC:", ESPACAMENTO, y, false);
    ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
    y += ESPACO_LINHA;

    // Linha 2: Resistor Conhecido
    snprintf(buffer, sizeof(buffer), "%.0f", RESISTOR_CONHECIDO);
    ssd1306_draw_string(oled, "R Fixo:", ESPACAMENTO, y, false);
    ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
    y += ESPACO_LINHA;

    // Linha 3: Resistência Medida
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

    // Linha 4: Resistência Comercial E24
    ssd1306_draw_string(oled, "R E24:", ESPACAMENTO, y, false);
    if (melhor_valor_e24 > 0) {
        snprintf(buffer, sizeof(buffer), "%.0f %c", melhor_valor_e24, SIMBOLO_OHM);
    } else {
        strcpy(buffer, "---");
    }
    ssd1306_draw_string(oled, buffer, POSICAO_VALOR_X, y, false);
    y += ESPACO_LINHA;

    // Separador Horizontal
    if (y > ALTURA_OLED - 3 - (3 * ALTURA_FONTE)) {
        y = ALTURA_OLED - 3 - (3 * ALTURA_FONTE);
    }
    ssd1306_hline(oled, ESPACAMENTO, LARGURA_OLED - 1 - ESPACAMENTO, y, true);
    y += 3;

    // Exibir Faixas de Cor
    const char *rotulos[3] = {"1a:", "2a:", "3a:"};
    int posicao_nome_cor = ESPACAMENTO + (strlen(rotulos[0]) * LARGURA_FONTE) + 2;

    for (int i = 0; i < 3 && (y + ALTURA_FONTE) <= ALTURA_OLED; ++i) {
        ssd1306_draw_string(oled, rotulos[i], ESPACAMENTO, y, false);
        ssd1306_draw_string(oled, cores[i], posicao_nome_cor, y, false);
        y += ESPACO_LINHA;
    }

    ssd1306_send_data(oled); // Envia os dados para o display OLED
}

int main(void) {
    inicializar_hardware(); // Inicializa o hardware

    ssd1306_t oled;
    ssd1306_init(&oled, LARGURA_OLED, ALTURA_OLED, false, OLED_ADDR, I2C_PORT);
    ssd1306_config(&oled);

    while (true) {
        float valor_adc = ler_adc(); // Lê o valor do ADC
        float resistencia = calcular_resistencia(valor_adc); // Calcula a resistência

        // Verifica se a resistência é maior que 500kΩ
        if (resistencia > 500000.0f) {
            desligar_matriz(); // Desliga a matriz LED
             
            ssd1306_fill(&oled, false); //limpa display
            ssd1306_draw_string(&oled, "Nenhum resistor", 7, 20, false);
            ssd1306_draw_string(&oled, "encontrado", 20, 30, false);
            ssd1306_send_data(&oled);
            
            continue; 
        }

        float melhor_valor_e24 = 0;
        int melhor_exp = -1, melhor_idx = -1;
        bool e24_encontrado = aproximar_e24(resistencia, &melhor_valor_e24, &melhor_exp, &melhor_idx); // Aproxima ao valor E24

        const char *cores[3] = {"---", "---", "---"};
        if (e24_encontrado) {
            determinar_cores(melhor_idx, melhor_exp, cores); // Determina as cores das faixas
        }

        atualizar_display_oled(&oled, valor_adc, resistencia, melhor_valor_e24, cores); // Atualiza o display OLED

        if (e24_encontrado) {
            mostrar_faixas_cores(cores[0], cores[1], cores[2]); // Mostra as faixas de cores na matriz LED
        } else {
            desligar_matriz(); // Desliga a matriz LED se nenhum resistor E24 for encontrado
        }

        sleep_ms(200); // Pausa de 200 ms
    }

    return 0;
}
