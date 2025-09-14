#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "pico/rand.h"

#define SPI spi0
#define SPI_CSN 17
#define SPI_MOSI 19
#define SPI_CLK 18

#define JOY_VER 27
#define JOY_HOR 26
#define JOY_SEL 22

#define KEY_UP 119      // w
#define KEY_DOWN 115    // s
#define KEY_LEFT 97     // a
#define KEY_RIGHT 100   // d

#define SNACK 128
#define BOARD_SIZE 64

const uint8_t CMD_NOOP = 0;
const uint8_t CMD_DIGIT0 = 1; 
const uint8_t CMD_DECODEMODE = 9;
const uint8_t CMD_BRIGHTNESS = 10;
const uint8_t CMD_SCANLIMIT = 11;
const uint8_t CMD_SHUTDOWN = 12;
const uint8_t CMD_DISPLAYTEST = 15;


typedef struct snake_t{
    uint8_t head;
    uint8_t tail;
    uint8_t len;
} snake_t;


static void snake_init(snake_t *snake, uint8_t *board, uint8_t start_x, uint8_t start_y){
    snake->head = 8 * start_y + start_x;
    snake->tail = 8 * start_y + start_x;
    memset(board, 0, BOARD_SIZE);
    board[snake->head] = 1;
    snake->len = 1;
}

static void set_snack(snake_t *snake, uint8_t *board, uint8_t old){
    board[old] = 0;
    uint32_t rnd = get_rand_32() % 64;

    while(board[rnd] != 0){
        rnd = (rnd + 1) % 64;
    }

    board[rnd] = SNACK;
}


static int update_board(snake_t *snake, uint8_t *board, char *input_dir, char *current_dir){

    int tmp_head = snake->head;
    
    switch(*input_dir){
        case KEY_UP:
            if((snake->head -= 8) > 247) 
                return -1;
            break;
        case KEY_DOWN:
            if((snake->head += 8) > 63) 
                return -1;
            break;
        case KEY_LEFT:
            if((snake->head % 8) == 7) 
                return -1;
            snake->head += 1;
            break;
        case KEY_RIGHT:
            if((snake->head % 8) == 0) 
                return -1;
            snake->head -= 1;
            break;
        default:
            return -1;
    }
    *current_dir = *input_dir;

    if(board[snake->head] && board[snake->head] != SNACK){
        return -1;
    }

    board[tmp_head] = snake->head;

    if(board[snake->head] == SNACK){
        set_snack(snake, board, snake->head);
        snake->len++;
    }
    else{
        int tmp_tail = snake->tail;
        snake->tail = board[snake->tail];
        board[tmp_tail] = 0;
    }

    board[snake->head] = 1;


    return 1;
}


static inline void spi_csn_put(bool value) {
    asm volatile("nop \n nop \n nop");
    gpio_put(SPI_CSN, value);
    asm volatile("nop \n nop \n nop");
}

static void write_register(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    spi_csn_put(0);
    spi_write_blocking(SPI, buf, 2);
    spi_csn_put(1);
    sleep_ms(1);
}


static void display_snake(uint8_t *board){
    uint8_t val;
    for(int i = 0; i < 8; i++){
        val = 0;
        for(int j = 0;  j < 8; j++){
            int set = board[8 * i + j] ? 1 : 0;
            val = (val << 1) | set;
        }
        write_register(CMD_DIGIT0 + i, val);
    }
}


static void setup_matrix(){
    spi_init(SPI, 10 * 1000 * 1000);
    gpio_set_function(SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI, GPIO_FUNC_SPI);

    gpio_init(SPI_CSN);
    gpio_set_dir(SPI_CSN, GPIO_OUT);
    gpio_put(SPI_CSN, 1);

    // Send init sequence to device
    write_register(CMD_SHUTDOWN, 0);
    write_register(CMD_DISPLAYTEST, 0);
    write_register(CMD_SCANLIMIT, 7);
    write_register(CMD_DECODEMODE, 255);
    write_register(CMD_SHUTDOWN, 1);
    write_register(CMD_BRIGHTNESS, 8);
}

static void setup_joystick(){
    adc_init();
    adc_gpio_init(JOY_HOR);
    adc_gpio_init(JOY_VER);
}

static void read_joystick(char* input_dir, char* current_dir){
    char tmp = 0, l, r;
    int adc_chan;

    if(*current_dir == KEY_UP || *current_dir == KEY_DOWN){
        adc_chan = 0;
        l = KEY_LEFT; 
        r = KEY_RIGHT;
    }
    else{
        adc_chan = 1;
        l = KEY_UP;
        r = KEY_DOWN;
    }

    for(int i = 0; i < 50; i++){
        adc_select_input(adc_chan);
        int x = adc_read();
        tmp = x == 2047 ? 0 : x < 2047 ? r : l;
        sleep_ms(10);
    }

    *input_dir = tmp == 0 ? *current_dir : tmp;

} 


uint8_t board[64];
snake_t snake;

int main() {

    setup_matrix();
    setup_joystick();

    snake_init(&snake, board, 1, 1);
    set_snack(&snake, board, 0);

    display_snake(board);

    char input_dir;
    char current_dir = KEY_LEFT;

    while(true){

        read_joystick(&input_dir, &current_dir);

        if(update_board(&snake, board, &input_dir, &current_dir) == -1){
            snake_init(&snake, board, 1, 1);
            set_snack(&snake, board, 0);
            input_dir = 0;
            current_dir = KEY_LEFT;
        }
        display_snake(board);
    }

}