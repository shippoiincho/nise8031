//  NEC PC-8031 floppy drive emulator
// GP0-GP7:  PPI B (Input)
// GP8-GP15: PPI A (Output)
// GP9-GP23: PPI C
// GP25: RESET
// GP26: SD CLK
// GP27: SD CMD
// GP28: SD DAT0
// GP29: SD DAT1
// GP30: SD DAT2
// GP31: SD DAT3
// GP32: SD Detect
// GP40: LCD SDA
// GP41: LCD SCL
// GP42: Rotary encoder SW
// GP44: Rotary encoder 
// GP45: Rotary encoder
// GP46: Access LED1 
// GP47: Access LED2

// #define USE_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
//#include "pico/sync.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
//#include "hardware/dma.h"
#include "hardware/uart.h"
//#include "hardware/flash.h"
//#include "hardware/sync.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

#include "fdc.h"
#include "hw_config.h"

#include "Z80.h"

#include "lcd.h"

#define RESET_PIN 25

struct repeating_timer timer,timer2;

#define FDC_INTERRUPT_INTERVAL 100

// PC configuration

static Z80 cpu;
uint32_t cpu_clocks=0;
uint32_t cpu_ei=0;
uint32_t cpu_cycles=0;
uint32_t cpu_hsync=0;
uint32_t interrupt_cycles;

uint8_t mainram[0x10000];
uint8_t ioport[0x100];

//uint8_t diskbuffer[0x400];
//unsigned char fd_filename[16];

volatile uint8_t fdu_init=0;

// UI

volatile uint32_t menumode=0;
uint32_t menuitem=0;


//unsigned char filename[16];
//unsigned char tape_filename[16];
//unsigned char rampac_filename[16];

//static inline unsigned char tohex(int);
//static inline unsigned char fromhex(int);
//static inline void video_print(uint8_t *);
//uint8_t fdc_find_sector(void);

volatile uint8_t disk_change=0;

//

const uint8_t romfilename[]="disk.rom";

const uint8_t testfilename[]="[OS] N80SR BASIC system disk (PC-8037SR) (PC-8001mkIISR).d88";
//const uint8_t testfilename2[]="[OS] N80 BASIC system disk (PC-8001mkII).d88";

//const uint8_t testfilename[]="pc-6601sr-utility_scp.d88";   // 2DD media for 1DD (Type 0x10)
//const uint8_t testfilename[]="FM Music Exercises (SR).d88"; // 1DD (Type 0x40)
//const uint8_t testfilename[]="[Utility] PC-6601 Utility Disk.d88"; // 2D media for 1D (Type 0x00)

const uint8_t testfilename2[]="newdisk.d88";

//const uint8_t configfile[]="config.txt";

// FatFS configuration

    FATFS fs;
    FIL fdtmp;

/* SDIO Interface */
static sd_sdio_if_t sdio_if = {
    /*
    Pins CLK_gpio, D1_gpio, D2_gpio, and D3_gpio are at offsets from pin D0_gpio.
    The offsets are determined by sd_driver\SDIO\rp2040_sdio.pio.
        CLK_gpio = (D0_gpio + SDIO_CLK_PIN_D0_OFFSET) % 32;
        As of this writing, SDIO_CLK_PIN_D0_OFFSET is 30,
            which is -2 in mod32 arithmetic, so:
        CLK_gpio = D0_gpio -2.
        D1_gpio = D0_gpio + 1;
        D2_gpio = D0_gpio + 2;
        D3_gpio = D0_gpio + 3;
    */
    .CMD_gpio = 27,
    .D0_gpio = 28,
    .baud_rate = 125 * 1000 * 1000 / 6  // 20833333 Hz
};

/* Hardware Configuration of the SD Card socket "object" */
static sd_card_t sd_card = {.type = SD_IF_SDIO, .sdio_if_p = &sdio_if};

size_t sd_get_num() { return 1; }

sd_card_t* sd_get_by_num(size_t num) {
    if (0 == num) {
        // The number 0 is a valid SD card number.
        // Return a pointer to the sd_card object.
        return &sd_card;
    } else {
        // The number is invalid. Return @c NULL.
        return NULL;
    }
}


//
//  reset

void __not_in_flash_func(z80reset)(uint gpio,uint32_t event) {

//    gpio_acknowledge_irq(RESET_PIN,GPIO_IRQ_EDGE_FALL);
    gpio_acknowledge_irq(RESET_PIN,GPIO_IRQ_EDGE_RISE);

    fdc_init();
    z80_power(&cpu,TRUE);
    memset(ioport,0,256);
//    memset(mainram,0,0x10000);
//    memcpy(mainram,mainrom,0x2000);

//    gpio_put_masked(0xff,0);
//    gpio_put_masked(0xf0000,0);

    fdu_init=1;

    return;

}

void display_init(void) {

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(40, GPIO_FUNC_I2C);
    gpio_set_function(41, GPIO_FUNC_I2C);
    gpio_pull_up(40);
    gpio_pull_up(41);

    lcd_init();

    return;

}

void display_file(void) {

    unsigned char string[32];

    lcd_clear();

    snprintf(string,17,"%c%15s",'>',testfilename);

    lcd_set_cursor(0,0);
    lcd_string(string);

    snprintf(string,17,"%c%15s",0x20,testfilename2);

    lcd_set_cursor(1,0);
    lcd_string(string);

    return;

}

#if 0

void draw_menu(void) {

    cursor_x=2;
    cursor_y=2;
    fbcolor=7;
      video_print("                                    ");
    for(int i=3;i<19;i++) {
        cursor_x=2;
        cursor_y=i;
        video_print("                                    ");
    }

    cursor_x=2;
    cursor_y=19;
    fbcolor=7;
    video_print("                                    ");

}

int draw_files(int num_selected,int page) {

    lfs_dir_t lfs_dirs;
    struct lfs_info lfs_dir_info;
    uint32_t num_entry=0;
    unsigned char str[16];

    int err= lfs_dir_open(&lfs,&lfs_dirs,"/");

    if(err) return -1;

    // for(int i=0;i<LFS_LS_FILES;i++) {
    //     cursor_x=22;
    //     cursor_y=i+3;
    //     fbcolor=7;
    //     video_print("             ");
    // }

    while(1) {

        int res= lfs_dir_read(&lfs,&lfs_dirs,&lfs_dir_info);
        if(res<=0) {

            if(num_entry>=LFS_LS_FILES*(page+1)) {
                break;
            }

            if((num_entry%LFS_LS_FILES)!=(LFS_LS_FILES-1)) {
                for(int i=num_entry%LFS_LS_FILES;i<LFS_LS_FILES;i++) {
                    cursor_x=22;
                    cursor_y=i+3;
                    fbcolor=7;
                    video_print("                  ");                    
                }
            }

            break;
        }

        cursor_x=28;
        cursor_y=18;
        fbcolor=7;
        sprintf(str,"Page %02d",page+1);

        video_print(str);

        switch(lfs_dir_info.type) {

            case LFS_TYPE_DIR:
                break;
            
            case LFS_TYPE_REG:

                if((num_entry>=LFS_LS_FILES*page)&&(num_entry<LFS_LS_FILES*(page+1))) {

                    cursor_x=22;
                    cursor_y=num_entry%LFS_LS_FILES+3;

                    if(num_entry==num_selected) {
                        fbcolor=0x70;
                        memcpy(filename,lfs_dir_info.name,16);
                    } else {
                        fbcolor=7;
                    }

                    snprintf(str,16,"%s            ",lfs_dir_info.name);
                    video_print(str);
//                    video_print(lfs_dir_info.name);

                }

                num_entry++;

                break;

            default:
                break; 

        }

    }

    lfs_dir_close(&lfs,&lfs_dirs);

    return num_entry;

}

int file_selector(void) {

    uint32_t num_selected=0;
    uint32_t num_files=0;
    uint32_t num_pages=0;

    num_files=draw_files(-1,0);

    if(num_files==0) {
         return -1;
    }

    while(1) {

        while(video_vsync==0) ;
        video_vsync=0;

        draw_files(num_selected,num_selected/LFS_LS_FILES);

        tuh_task();

        if(keypressed==0x52) { // up
            keypressed=0;
            if(num_selected>0) {
                num_selected--;
            }
        }

        if(keypressed==0x51) { // down
            keypressed=0;
            if(num_selected<num_files-1) {
                num_selected++;
            }
        }

        if(keypressed==0x4b) { // Pageup
            keypressed=0;
            if(num_selected>=LFS_LS_FILES) {
                num_selected-=LFS_LS_FILES;
            }
        }

        if(keypressed==0x4e) { // Pagedown
            keypressed=0;
            if(num_selected<num_files-LFS_LS_FILES) {
                num_selected+=LFS_LS_FILES;
            }
        }

        if(keypressed==0x28) { // Ret
            keypressed=0;

            return 0;
        }

        if(keypressed==0x29 ) {  // ESC

            return -1;

        }

    }
}

int enter_filename() {

    unsigned char new_filename[16];
    unsigned char str[32];
    uint8_t keycode;
    uint32_t pos=0;

    memset(new_filename,0,16);

    while(1) {

        sprintf(str,"Filename:%s  ",new_filename);
        cursor_x=3;
        cursor_y=18;
        video_print(str);

        while(video_vsync==0) ;
        video_vsync=0;

        tuh_task();

        if(keypressed!=0) {

            if(keypressed==0x28) { // enter
                keypressed=0;
                if(pos!=0) {
                    memcpy(filename,new_filename,16);
                    return 0;
                } else {
                    return -1;
                }
            }

            if(keypressed==0x29) { // escape
                keypressed=0;
                return -1;
            }

            if(keypressed==0x2a) { // backspace
                keypressed=0;

                cursor_x=3;
                cursor_y=18;
                video_print("Filename:          ");

                new_filename[pos]=0;

                if(pos>0) {
                    pos--;
                }
            }

            if(keypressed<0x4f) {
                keycode=usbhidcode[keypressed*2];
                keypressed=0;

                if(pos<7) {

                    if((keycode>0x20)&&(keycode<0x5f)&&(keycode!=0x2f)) {

                        new_filename[pos]=keycode;
                        pos++;

                    }

                }
            }

        }
    }

}
#endif

static inline uint8_t mem_read(void *context,uint16_t address)
{

    if(address<0x8000) {
        return mainram[address];
    }

    return 0xff;

}

static inline void mem_write(void *context,uint16_t address, uint8_t data)
{

    if((address>=0x4000)&&(address<0x8000)) {
        mainram[address]=data;
    }

    return;

}

static inline uint8_t io_read(void *context, uint16_t address)
{
    uint8_t data = ioport[address&0xff];
    uint8_t b;
    uint32_t gpio_data;


    if((address&0xff)!=0xfe) {
//        printf("[IOR %02x:%04x]",address&0xff,Z80_PC(cpu));
    }
        // if((address&0xf0)==0xe0) {
        // printf("[IOR:%04x:%02x]",Z80_PC(cpu),address&0xff);
        // }

    switch(address&0xff) {

        case 0xf8:  // TC
#ifdef USE_DEBUG
        printf("[TC]");
#endif
        fdc_tc();

            return 0xff;

        case 0xfa:  // FDC status
        
            return fdc_status();

        case 0xfb:  // FDC Data

            return fdc_command_read();

        case 0xfc:  // PPI A

            gpio_data=gpio_get_all();

            gpio_data&=0xff00;
            gpio_data>>=8;

#ifdef USE_DEBUG
            if(gpio_data!=0) {
                printf("[PR:%02x]",gpio_data);
            }
#endif
            return gpio_data;

        case 0xfd:  // PPI B

            return ioport[0xfd];

        case 0xfe:  // PPI C

            gpio_data=gpio_get_all()&0xf00000;

//        printf("[%x]",gpio_data);
            
            gpio_data>>=20;

            ioport[0xfe]&=0xf0;
            ioport[0xfe]|=gpio_data;

#if DEBUG            
if((ioport[0xfe]&0xf)!=0) {
        printf("[SR:%02x]",ioport[0xfe]);
}
#endif

            return ioport[0xfe];

        case 0xff:  // PPI Control
            return ioport[address&0xff];

        default:
            return 0xff;

    }

//   return 0xff;
}

static inline void io_write(void *context, uint16_t address, uint8_t data)
{

    uint8_t b;

#ifdef USE_DEBUG
    if((address&0xff)!=0xff) {
    printf("[IOW %02x:%02x]",address&0xff,data);
    }
#endif

    // if((address&0xf0)==0xf0) {
    // printf("[IOW:%04x:%02x:%02x->%02x]",Z80_PC(cpu),address&0xff,ioport[address&0xff],data);
    // }

    switch(address&0xff) {

        case 0xf8:  // Motor
            ioport[0xf8]=data;
            return;

        case 0xfa:  // FDC data

            return;


        case 0xfb:  // FDC command
            fdc_command_write(data);
            return;

        case 0xfd:  // PPI B

            gpio_put_masked(0xff,data);
#ifdef USE_DEBUG
            printf("{%02x}",data);
#endif
            ioport[0xfd]=data;
            return;

        case 0xfe:  // PPI C

            gpio_put_masked(0xf0000,(data&0xf0)<<12);
            ioport[0xfe]&=0xf;
            ioport[0xfe]|=data&0xf0;

            return;

        case 0xff:

            if((data&0x80)==0) { // Bit operation

                b=(data&0x0e)>>1;

                if(data&1) {
                    ioport[0xfe]|= 1<<b;
                } else {
                    ioport[0xfe]&= ~(1<<b);
                }

                gpio_put_masked(0xf0000,(((uint32_t)(ioport[0xfe])&0xf0)<<12));
#ifdef DEBUG
        printf("[SW:%02x]",ioport[0xfe]);
//                printf("[%x]",gpio_get_all());
#endif            
            }

            return;

        default:
            ioport[address&0xff]=data;
            return;

    }


}

static uint8_t ird_read(void *context,uint16_t address) {

    // PC-8031 use Mode 0 and return NOP(00)

    z80_int(context,FALSE);

    return 0;

}

#if 0
static void reti_callback(void *context) {

}
#endif

void init_emulator(void) {
    //  setup emulator 
    // COPY DISK ROM to RAM
    // Maximum 8Kib
    //    memcpy(mainram,mainrom,0x2000);
}

void main_core1(void) {

    uint32_t    gpio_data;

//    multicore_lockout_victim_init();

    gpio_set_irq_enabled_with_callback(RESET_PIN,GPIO_IRQ_EDGE_RISE,true,z80reset);

    // RUN Z80 EMULATION on Core1

    init_emulator();

    fdu_init=1;

    cpu.read = mem_read;
    cpu.write = mem_write;
    cpu.in = io_read;
    cpu.out = io_write;
	cpu.fetch = mem_read;
    cpu.fetch_opcode = mem_read;
//    cpu.reti = reti_callback;
    cpu.inta = ird_read;

    z80_power(&cpu,true);
    z80_instant_reset(&cpu);

    cpu_hsync=0;
    cpu_cycles=0;

    while(1) {

        if(fdu_init) {

            // Wait PC is ready

            while(1) {
                gpio_put_masked(0xf00ff,0);
                sleep_ms(10);
        
                gpio_data=gpio_get_all()&0xf00000;
                if(gpio_data!=0xf00000) {
                    fdu_init=0;    
                    break;
                }
            }
        }

        cpu_cycles += z80_run(&cpu,1);
        cpu_clocks++;

        if(fdc_interrupt_flag==1) {
            fdc_interrupt_flag=2;
            interrupt_cycles = cpu_clocks + FDC_INTERRUPT_INTERVAL;
        } 
        if((fdc_interrupt_flag==2)&&(cpu_clocks>interrupt_cycles)) {
            fdc_interrupt_flag=0;
            z80_int(&cpu,TRUE);
        }

    }
}

int main() {

    uint32_t menuprint=0;
    uint32_t filelist=0;
    uint32_t subcpu_wait;
    UINT bytes_read;

    static uint32_t hsync_wait,vsync_wait;

//    set_sys_clock_khz(300000 ,true);

    stdio_init_all();

    gpio_init_mask(0xffffffff);
    gpio_set_dir_all_bits(0x0f00ff);

    display_init();

    gpio_init(46);
    gpio_init(47);
    gpio_set_dir(46,true);
    gpio_set_dir(47,true);
    gpio_put(46,false);
    gpio_put(47,false);

    fdc_init();
//    disk_change=0;

//sleep_ms(1000);

//  Initialize FatFs

    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html

    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        lcd_clear();
        lcd_set_cursor(0,0);
        lcd_string("Can not mount SD card");
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);



        return -1;
    }

    // READ ROM File

    fr=f_open(&fdtmp,romfilename,FA_READ);

    if (FR_OK != fr) {
        lcd_clear();
        lcd_set_cursor(0,0);
        lcd_string("Can not read rom file");
        panic("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return -1;
    }

    f_read(&fdtmp,mainram,8192,&bytes_read);

    // READ Config file



    // Beep & PSG

#if 0
    gpio_set_function(6,GPIO_FUNC_PWM);
 //   gpio_set_function(11,GPIO_FUNC_PWM);
    pwm_slice_num = pwm_gpio_to_slice_num(6);

    pwm_set_wrap(pwm_slice_num, 256);
    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_A, 0);
//    pwm_set_chan_level(pwm_slice_num, PWM_CHAN_B, 0);
    pwm_set_enabled(pwm_slice_num, true);
#endif

// uart handler

    // irq_set_exclusive_handler(UART0_IRQ,uart_handler);
    // irq_set_enabled(UART0_IRQ,true);
    // uart_set_irq_enables(uart0,true,false);


    multicore_launch_core1(main_core1);   
//    multicore_lockout_victim_init();

// TEST TEST TEST

   fr=f_open(fd_drive[0],testfilename,FA_READ);

    if (FR_OK != fr) {
        panic("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return -1;
    }

    fdc_check(0);

   fr=f_open(fd_drive[1],testfilename2,FA_READ|FA_WRITE);

    if (FR_OK != fr) {
        panic("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return -1;
    }

    fdc_check(1);

    display_file();

#ifdef DEBUG
    printf("[Drive1:%x,%x]",fd_drive_status[0],fd_media_type[0]);
    printf("[Drive2:%x,%x]",fd_drive_status[1],fd_media_type[1]);
#endif

    sleep_ms(1);

    menumode=1;  // Pause emulator

    while(1){
          tight_loop_contents(); 
    }

}
