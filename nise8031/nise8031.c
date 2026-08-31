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

// DEBUG control
//#define USE_DEBUG

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
#define RE_SW   42
#define RE_A    44
#define RE_B    45
#define LCD_SDA 40
#define LCD_SCL 41

struct repeating_timer timer,timer2;

// WAIT cycles to interrupt
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

volatile uint32_t timer_count;
struct repeating_timer timer;

volatile uint8_t fdu_init=0;

// UI

#define RE_DELAY 2
#define MENU_TIMEOUT 5000
uint32_t re_a_count;
uint32_t re_b_count;
uint32_t re_sw_count;
bool re_a_state=true;
bool re_b_state=true;
bool re_sw_state=true;

volatile uint32_t menumode=0;   // 0:normal 1:select drive 2:select file
volatile uint32_t menucount=0;

uint32_t menuitem=0;
uint32_t numitems=0;
uint32_t menuselected=0;
uint32_t menudrive=1;

unsigned char lcd_line1[256];
unsigned char lcd_line2[256];
unsigned char lcd_prompt1=0x20;
unsigned char lcd_prompt2=0x20;

volatile uint8_t disk_change=0;

// PC-8031 ROM filename
const uint8_t romfilename[]="disk.rom";

// config file to mount
// directory1
// filename2
// directory2
// filename2
const uint8_t configfile[]="config.txt";

uint8_t fd_directory1[256];
uint8_t fd_directory2[256];
uint8_t fd_filename1[256];
uint8_t fd_filename2[256];
uint8_t fd_filename[256];
uint8_t fd_menu_drive=0;

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
//  Timer

bool __not_in_flash_func(timer_handler)(struct repeating_timer *t) {

    timer_count++;

    return true;
}


// Rotary Encoder interrupts
// GPIO IRQ

void __not_in_flash_func(irq_callback)(uint gpio,uint32_t event) {

    bool resw,rea,reb;

    switch(gpio) {

        case RE_A:

//            gpio_acknowledge_irq(RE_A,event);
//            gpio_acknowledge_irq(RE_A,GPIO_IRQ_EDGE_FALL);
            
            rea=gpio_get(RE_A);
            reb=gpio_get(RE_B);

            sleep_us(10);

            if((rea!=gpio_get(RE_A))||(reb!=gpio_get(RE_B))) {
                return;
            }

            if(gpio_get(RE_A) ^ gpio_get(RE_B)) {
                printf("[RE+]");
            } else {
            printf("[RE-]");
            }

            return;

        case RE_SW:

//            gpio_acknowledge_irq(RE_SW,GPIO_IRQ_EDGE_FALL);

            resw=gpio_get(RE_SW);
            sleep_us(100);
            if(resw!=gpio_get(RE_SW)) {
                return;
            }

            printf("[SW:on]");

            return;


        default:
            return;

    }

}

uint8_t encoder_check() {

    uint8_t val;

    val=0;

    // check SW

    if(re_sw_state!=gpio_get(RE_SW)) {
        if((timer_count-re_sw_count)>RE_DELAY) {
            if(re_sw_state==true) { // Falling
#ifdef DEBUG
                printf("[SW:on]");
#endif
                val=1;
            }
            re_sw_state=!re_sw_state;
        }
    } else {
        re_sw_count=timer_count;
    }

    // check RE

    if(re_a_state!=gpio_get(RE_A)) {
        if((timer_count-re_a_count)>RE_DELAY) {
            if(re_a_state==false) {
                if(re_b_state==true) {
#ifdef DEBUG
                    printf("[RE:+]");
#endif
                    val|=4;
                } else {
#ifdef DEBUG
                    printf("[RE:-]");
#endif
                    val|=2;
                }
            }   // EC11 encoder make 1 pluse for 1 click
            // else {
            //     if(re_b_state==false) {
            //         printf("[RE:+]");
            //     } else {
            //         printf("[RE:-]");
            //     }                
            // }

            re_a_state=!re_a_state;
        }
    } else {
        re_a_count=timer_count;
    }

    if(re_b_state!=gpio_get(RE_B)) {
        if((timer_count-re_b_count)>RE_DELAY) {
            re_b_state=!re_b_state;
        }
    } else {
        re_b_count=timer_count;
    }

    return val;

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

//

void display_init(void) {

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(LCD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_SDA);
    gpio_pull_up(LCD_SCL);

    lcd_init();

    return;

}

void display_file(void) {

    unsigned char string[32];
    
    lcd_clear();

    if((menumode!=0)&&(menudrive==1)) {
        snprintf(string,17,"%c%-15s",'>',lcd_line1);            
    } else {
        snprintf(string,17,"%c%-15s",' ',lcd_line1);             
    }
    lcd_set_cursor(0,0);
    lcd_string(string);

    if((menumode!=0)&&(menudrive==2)) {
        snprintf(string,17,"%c%-15s",'>',lcd_line2);            
    } else {
        snprintf(string,17,"%c%-15s",' ',lcd_line2);             
    }

    lcd_set_cursor(1,0);
    lcd_string(string);

    return;

}

// get number of entries

int32_t get_directory(uint8_t *directory,uint8_t *filename) {

    DIR dir;
    FILINFO finfo;
    int32_t numfiles;
    FRESULT fr;

    numfiles=0;

    fr=f_opendir(&dir,directory);

    if(fr!=FR_OK) {
        return -1;
    }    

    while(1) {

        fr=f_readdir(&dir,&finfo);
#ifdef DEBUG
        printf("%d %s\n\r",finfo.fattrib,finfo.fname); 
#endif
        if(strcmp(finfo.fname,"")!=0) {

            // ignore system file (eg. "System Volume Information")

            if(finfo.fattrib&AM_SYS) {
                continue;
            }

            // ignore ROM and config file

            if(strcmp(finfo.fname,romfilename)==0) {
                continue;
            }
            if(strcmp(finfo.fname,configfile)==0) {
                continue;
            }

            numfiles++;

            if(strcmp(finfo.fname,filename)==0) {
                menuselected=numfiles;
            }

        } else {
            break;
        }


   }

   f_closedir(&dir);

   return numfiles;

}

//

int32_t get_filename(uint8_t *directory,uint32_t number) {

    DIR dir;
    FILINFO finfo;
    FRESULT fr;
    int32_t numfiles;

    numfiles=0;

    if(number==0) {
        strcpy(fd_filename,"..");
        return AM_DIR;
    }

    if(number==numitems+1) {
        strcpy(fd_filename,"UNMOUNT");
        return 255;
    }

    fr=f_opendir(&dir,directory);

    if(fr!=FR_OK) {
        return -1;
    }    

    while(1) {

        fr=f_readdir(&dir,&finfo);
#ifdef DEBUG        
        printf("%d %s\n\r",finfo.fattrib,finfo.fname);
#endif
        if(strcmp(finfo.fname,"")!=0) {

            // ignore system file (eg. "System Volume Information")

            if(finfo.fattrib&AM_SYS) {
                continue;
            }

            // ignore ROM and config file

            if(strcmp(finfo.fname,romfilename)==0) {
                continue;
            }
            if(strcmp(finfo.fname,configfile)==0) {
                continue;
            }

            numfiles++;
            if(numfiles==number) {
                sprintf(fd_filename,"%s",finfo.fname);
                if(finfo.fattrib&AM_DIR) {
                    return AM_DIR;
                } else {
                    return 0;
                }
            }

        } else {
            break;
        }

   }

   f_closedir(&dir);

   return 0;
  
    // return
    // 0:file
    // 0x10:directory

}

void updir(unsigned char *str) {

    int32_t ptr;

    ptr=strlen(str);
#ifdef DEBUG
    printf("[%s]->",str);
#endif
    if(ptr<1) return;

    while(ptr>0){
        if(str[ptr]=='/') {
            str[ptr]=0;
#ifdef DEBUG
            printf("[%s]\n\r",str);
#endif
            return;
            
        }
        ptr--;
    }

    strcpy(str,"/");

    return;
}

int32_t saveconfig(void) {

    FRESULT fr;

    fr=f_open(&fdtmp,configfile,FA_READ|FA_WRITE|FA_CREATE_ALWAYS);

    if (FR_OK == fr) {
        f_printf(&fdtmp,"%s\n",fd_directory1);
        f_printf(&fdtmp,"%s\n",fd_filename1);
        f_printf(&fdtmp,"%s\n",fd_directory2);
        f_printf(&fdtmp,"%s\n",fd_filename2);
        f_close(&fdtmp);

        return 0;
    }

    return -1;


}



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

//    gpio_set_irq_enabled(RESET_PIN,GPIO_IRQ_EDGE_RISE, true);
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
//    uint32_t filelist=0;
    UINT bytes_read;
    uint8_t encoder;
    unsigned char filename[256];
    unsigned char dirname[256];
    int32_t ftype;

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

    f_close(&fdtmp);

    // Overwrite drive type (for pseudo bios)

    mainram[0x7ef]=0xef;

    // READ Config file

    fr=f_open(&fdtmp,configfile,FA_READ);

    if (FR_OK == fr) {

        f_gets(fd_directory1,255,&fdtmp);
        f_gets(fd_filename1,255,&fdtmp);
        f_gets(fd_directory2,255,&fdtmp);
        f_gets(fd_filename2,255,&fdtmp);        

        // remove tailing newline
        fd_directory1[strcspn(fd_directory1, "\r\n")] = 0;
        fd_filename1[strcspn(fd_filename1, "\r\n")] = 0;
        fd_directory2[strcspn(fd_directory2, "\r\n")] = 0;
        fd_filename2[strcspn(fd_filename2, "\r\n")] = 0;

        f_close(&fdtmp);

    } else {
        // Can not read config file

        strcpy(fd_directory1,"/");
        strcpy(fd_directory2,"/");

        fd_filename1[0]=0;
        fd_filename2[0]=0;

    }

    // Initialize Rotary Encoder

    gpio_init(RE_SW);
    gpio_init(RE_A);
    gpio_init(RE_B);

    gpio_set_dir(RE_SW,false);
    gpio_set_dir(RE_A,false);
    gpio_set_dir(RE_B,false);

    gpio_set_pulls(RE_SW,true,false);
    gpio_set_pulls(RE_A,true,false);
    gpio_set_pulls(RE_B,true,false); 
 
//    gpio_set_irq_callback(irq_callback);

//    gpio_set_irq_enabled(RE_A,GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL,true);
//    gpio_set_irq_enabled(RE_SW,GPIO_IRQ_EDGE_FALL,true);

//    irq_set_enabled(IO_IRQ_BANK0,true);

    // timer (1 msec)
    add_repeating_timer_us(1000,timer_handler,NULL  ,&timer);

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

    multicore_launch_core1(main_core1);   
//    multicore_lockout_victim_init();

//  Image file mount

    strcpy(lcd_line1,"[EMPTY]");
    if(fd_filename1[0]!=0) {
        if(strcmp(fd_directory1,"/")==0) {
            snprintf(filename,255,"%s",fd_filename1);
        } else {
            snprintf(filename,255,"%s/%s",fd_directory1,fd_filename1);
        }
        fr=f_open(fd_drive[0],filename,FA_READ);

        if (FR_OK == fr) {
            fdc_check(0);
            strncpy(lcd_line1,fd_filename1,255);            
        } else {
            strcpy(fd_directory1,"/");
            fd_filename1[0]=0;
        }
    }

    strcpy(lcd_line2,"[EMPTY]");
    if(fd_filename2[0]!=0) {
        if(strcmp(fd_directory2,"/")==0) {
            snprintf(filename,255,"%s",fd_filename2);
        } else {
            snprintf(filename,255,"%s/%s",fd_directory2,fd_filename2);
        }
        fr=f_open(fd_drive[1],filename,FA_READ);

        if (FR_OK == fr) {
            fdc_check(1);
            strncpy(lcd_line2,fd_filename2,255);            
        } else {
            strcpy(fd_directory2,"/");
            fd_filename2[0]=0;            
        }
    }    

    display_file();

#ifdef DEBUG
    printf("[Drive1:%x,%x]",fd_drive_status[0],fd_media_type[0]);
    printf("[Drive2:%x,%x]",fd_drive_status[1],fd_media_type[1]);
#endif

    menumode=0;

    while(1){

        encoder=encoder_check();

        switch(menumode) {

            case 0:
                // enter menu mode
                if(encoder&1) {
                    menumode=1;
                    menucount=timer_count;
                    display_file();                    
                }

                break;

            case 1:

                if(encoder&1) {
                    menumode=2;
                    menucount=timer_count;
                    menuselected=1;

//                    if(fd_drive[menudrive]!=0) {
                        // find file entry if image was mounted
                        if(menudrive==1) {
                            numitems=get_directory(fd_directory1,fd_filename1);
                            ftype=get_filename(fd_directory1,menuselected);
                            if(ftype==0) {
                                strncpy(lcd_line1,fd_filename,255); 
                            } else {
                                snprintf(lcd_line1,255,"[%s]",fd_filename);
                            }
                        }   else {
                            numitems=get_directory(fd_directory2,fd_filename2);                            
                            ftype=get_filename(fd_directory2,menuselected);
                            if(ftype==0) {
                                strncpy(lcd_line2,fd_filename,255); 
                            } else {
                                snprintf(lcd_line2,255,"[%s]",fd_filename);
                            }                                                        
                        }                      
//                    } 

                    // display directories

                    display_file();

                    break;
                }
                if(encoder&6) {
                    menucount=timer_count;
                    if(menudrive==1) {
                        menudrive=2;
                    } else {
                        menudrive=1;
                    }
                    display_file();
                }

                break;

            case 2:

                if(encoder&1) {
                    // file select

                    if(menudrive==1) {
                        ftype=get_filename(fd_directory1,menuselected);
                    } else {
                        ftype=get_filename(fd_directory2,menuselected);
                    }
                    if(ftype==0) {
                        if(menudrive==1) {
                            if(strcmp(fd_directory1,"/")==0) {
                                snprintf(filename,255,"%s",fd_filename);
                            } else {
                                snprintf(filename,255,"%s/%s",fd_directory1,fd_filename);
                            }
#ifdef DEBUG
                            printf("[%s]\n",filename);
#endif
                            if(fd_drive_status[0]!=0) {
                                f_close(fd_drive[0]);
                            }
                            fr=f_open(fd_drive[0],filename,FA_READ|FA_WRITE);
                            if(fr!=FR_OK) {
                                break;
                            }
                            fdc_check(0);
                            strncpy(fd_filename1,fd_filename,255);                            
                            strncpy(lcd_line1,fd_filename1,255);
                            menumode=0;
                            display_file();
                            saveconfig();
                            break;

                        } else {
                            if(strcmp(fd_directory2,"/")==0) {
                                snprintf(filename,255,"%s",fd_filename);
                            } else {
                                snprintf(filename,255,"%s/%s",fd_directory2,fd_filename);
                            }
#ifdef DEBUG
                            printf("[%s]\n",filename);
#endif
                            if(fd_drive_status[1]!=0) {
                                f_close(fd_drive[1]);
                            }
                            fr=f_open(fd_drive[1],filename,FA_READ|FA_WRITE);
                            if(fr!=FR_OK) {
                                break;
                            }
                            fdc_check(1);
                            strncpy(fd_filename2,fd_filename,255);                            
                            strncpy(lcd_line2,fd_filename2,255);
                            menumode=0;
                            display_file();
                            saveconfig();
                            break;
                        }

                    } else {
                        // change directory
                        if(menuselected==0) {
                            // UP dir
                            if(menudrive==1) {
                                updir(fd_directory1);
                                numitems=get_directory(fd_directory1,"");
                                menuselected=1;
                                ftype=get_filename(fd_directory1,menuselected);
                                if(ftype==0) {
                                    strncpy(lcd_line1,fd_filename,255); 
                                } else {
                                    snprintf(lcd_line1,255,"[%s]",fd_filename);
                                }                                   
                            } else {
                                updir(fd_directory2);
                                numitems=get_directory(fd_directory2,"");
                                menuselected=1;
                                ftype=get_filename(fd_directory2,menuselected);
                                if(ftype==0) {
                                    strncpy(lcd_line2,fd_filename,255); 
                                } else {
                                    snprintf(lcd_line2,255,"[%s]",fd_filename);
                                }
                            }

                            display_file();

                        } else if (menuselected==numitems+1) {
                            // Unmount
                            if(menudrive==1) {
                                if(fd_drive_status[0]!=0) {
                                    f_close(fd_drive[0]);
                                }
                                fd_drive_status[0]=0;
                                strcpy(lcd_line1,"[EMPTY]");                                
                            } else {
                                if(fd_drive_status[1]!=0) {
                                    f_close(fd_drive[1]);
                                }
                                fd_drive_status[1]=0;
                                strcpy(lcd_line2,"[EMPTY]"); 
                            }

                            menumode=0;
                            display_file();

                        } else {
                            // Down dir
                            if(menudrive==1) {
                                if(strcmp(fd_directory1,"/")==0) {
                                    snprintf(dirname,255,"/%s",fd_filename);
                                } else {
                                    snprintf(dirname,255,"%s/%s",fd_directory1,fd_filename);
                                }
                                strncpy(fd_directory1,dirname,255);
                                numitems=get_directory(fd_directory1,"");
                                menuselected=1;
                                ftype=get_filename(fd_directory1,menuselected);
                                if(ftype==0) {
                                    strncpy(lcd_line1,fd_filename,255); 
                                } else {
                                    snprintf(lcd_line1,255,"[%s]",fd_filename);
                                }                   
                            } else {
                                if(strcmp(fd_directory2,"/")==0) {
                                    snprintf(dirname,255,"/%s",fd_filename);
                                } else {
                                    snprintf(dirname,255,"%s/%s",fd_directory2,fd_filename);
                                }
                                strncpy(fd_directory2,dirname,255);
                                numitems=get_directory(fd_directory2,"");
                                menuselected=1;
                                ftype=get_filename(fd_directory2,menuselected);
                                if(ftype==0) {
                                    strncpy(lcd_line2,fd_filename,255); 
                                } else {
                                    snprintf(lcd_line2,255,"[%s]",fd_filename);
                                }
                            }
 
                            display_file();

                        }

                    }
                }

                if(encoder&6) {
                    menucount=timer_count;

                    if(encoder&2) {
                        menuselected++;
                        if(menuselected>numitems+1) {
                            menuselected=0;
                        }
                    } else {
                        if(menuselected>0) {
                            menuselected--;
                        } else {
                            menuselected=numitems+1;
                        }
                    }
#ifdef DEBUG
                    printf("[%d]",menuselected);
#endif
                    if(menudrive==1) {
                        ftype=get_filename(fd_directory1,menuselected);
                        if(ftype==0) {
                            strncpy(lcd_line1,fd_filename,255); 
                        } else {
                            snprintf(lcd_line1,255,"[%s]",fd_filename);
                        }

                    } else {
                        ftype=get_filename(fd_directory2,menuselected);
                        if(ftype==0) {
                            strncpy(lcd_line2,fd_filename,255); 
                        } else {
                            snprintf(lcd_line2,255,"[%s]",fd_filename);                            
                        }

                    }
  
                    display_file();

                }

                break;

            default:

        }

        // TIMEOUT

        if((menumode!=0)&&((timer_count-menucount)>MENU_TIMEOUT)) {
            menumode=0;
            if(fd_drive_status[0]!=0) {
                strncpy(lcd_line1,fd_filename1,255); 
            } else {
                strcpy(lcd_line1,"[EMPTY]");
            }
            if(fd_drive_status[1]!=0) {
                strncpy(lcd_line2,fd_filename2,255); 
            } else {
                strcpy(lcd_line2,"[EMPTY]");
            }
            
            display_file();
        }

//          tight_loop_contents(); 
    }

}
