#include "matriz_led.h"
#include <string.h> // Para strcmp

// Estrutura para mapear nome da cor para valores RGB
typedef struct {
    const char *nome;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} CorRGB;

// Paleta de cores para as faixas (Cores ajustadas para melhor diferenciação)
const CorRGB PALETA_CORES[] = {
    {"Preto",    0,   0,   0},
    {"Marrom",   60,  30,  10},  // Marrom mais escuro e menos avermelhado
    {"Vermelho", 150,  0,   0},
    {"Laranja",  230, 100,  0},  // Laranja mais vibrante e claro
    {"Amarelo",  150, 90,   0},  // Cor amarela ajustada (menos verde)
    {"Verde",    0,  150,   0},
    {"Azul",     0,   0,  150},
    {"Violeta",  100,  0,  100},
    {"Cinza",    80,  80,  80},
    {"Branco",   100, 100, 100},
    {"Prata",    192, 192, 192},
    {"Ouro",     218, 165,  32},
    {"---",      0,   0,   0}
};

const int NUM_PALETA = sizeof(PALETA_CORES) / sizeof(PALETA_CORES[0]);

// --- Funções Internas ---

// Converte R, G, B para o formato GRB de 32 bits usado pelo WS2812
static inline uint32_t rgb_para_uint32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

// Envia um valor de pixel (formato GRB) para a state machine PIO
static inline void enviar_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Procura o nome da cor na paleta e retorna o valor GRB correspondente
uint32_t nome_cor_para_grb(const char *nome) {
    for (int i = 0; i < NUM_PALETA; ++i) {
        if (strcmp(nome, PALETA_CORES[i].nome) == 0) {
            return rgb_para_uint32(PALETA_CORES[i].r, PALETA_CORES[i].g, PALETA_CORES[i].b);
        }
    }
    return 0; // Retorna preto se não encontrar
}


// --- Funções Públicas ---

// Inicializa a PIO para controlar a matriz de LEDs
void inicializar_matriz_led() {
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, PINO_WS2812, 800000, RGBW_ATIVO);
}

// Mostra as cores das faixas nas linhas corretas (1ª e 5ª invertidas)
void mostrar_faixas_cores(const char *cor_faixa1, const char *cor_faixa2, const char *cor_faixa3) {
    uint32_t grb_faixa1 = nome_cor_para_grb(cor_faixa1);
    uint32_t grb_faixa2 = nome_cor_para_grb(cor_faixa2);
    uint32_t grb_faixa3 = nome_cor_para_grb(cor_faixa3);
    uint32_t cor_pixel;

    for (int i = 0; i < NUM_PIXELS; i++) {
        int linha = i / NUM_COLUNAS; // Linha física (0-4)

        // *** LÓGICA DE INVERSÃO DAS LINHAS 0 E 4 ***
        if (linha == 4) {         // Linha física 5 (índice 4) mostra COR 1
            cor_pixel = grb_faixa1;
        } else if (linha == 2) { // Linha física 3 (índice 2) mostra COR 2
            cor_pixel = grb_faixa2;
        } else if (linha == 0) { // Linha física 1 (índice 0) mostra COR 3
            cor_pixel = grb_faixa3;
        } else {                 // Linhas físicas 2 e 4 (índices 1 e 3) ficam apagadas
            cor_pixel = 0;
        }
        enviar_pixel(cor_pixel);
    }
    sleep_us(100); // Pequeno delay opcional
}

// Desliga todos os LEDs da matriz
void desligar_matriz() {
    for (int i = 0; i < NUM_PIXELS; i++) {
        enviar_pixel(0);
    }
    sleep_us(100); // Pequeno delay opcional
}