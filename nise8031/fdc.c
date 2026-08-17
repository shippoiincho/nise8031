#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "fdc.h"

uint8_t fdc_command_buffer[10];
uint8_t fdc_result_buffer[8];
uint8_t fdc_command_write_index=0;
uint8_t fdc_command_read_index=0;
uint8_t fdc_command_read_length=0;
uint8_t fdc_interrupt_flag=0;
uint8_t fdc_command_drive=0;
uint8_t fdc_command_cylinder[4];
uint8_t fdc_command_head[4];
uint8_t fdc_command_eot;
uint8_t fdc_dma_flag=0;
uint32_t fdc_read_count;
uint32_t fdc_read_sector_size;
uint32_t fdc_write_count;
uint32_t fdc_write_sector_size;
uint8_t fdc_exec_phase_finish;
uint8_t fdc_phase_flag=0;      // 0. command 1. Execute 2. Result
uint8_t fdc_sector_not_found=0;

uint8_t fdc_interrupt_flag;

uint8_t fdc_sector_info[16];

const uint8_t fdc_command_length[]={ 1,9,9,3,2,9,9,2,1,9,2,1,9,6,1,3};
const uint32_t fdc_sector_count[]={128,256,512,1024,2048,4096};

#ifdef USE_FATFS
FIL *fd_drive[4];
#else 
lfs_t lfs_handler;
lfs_file_t fd_drive[4];
#endif


uint8_t dummy_buff[1024];
uint8_t fd_drive_status[4];
uint8_t fd_media_type[4];   // 0x00:2D,0x10:2DD,0x20:2HD,0x30:1D,0x40:1DD,0x50:1DD for 2DD file


#define FDC_ACTIVE_LED1 46
#define FDC_ACTIVE_KED2 47
#define FDC_LED_DELAY 100

void fdc_led_active(uint8_t driveno) {

    if(driveno==0) {
        gpio_put(FDC_ACTIVE_LED1,true);
    } else if(driveno==1) {
        gpio_put(FDC_ACTIVE_KED2,true);
    }
    return;
}

void fdc_led_inactive(uint8_t driveno) {

    gpio_put(FDC_ACTIVE_LED1,false);
    gpio_put(FDC_ACTIVE_KED2,false);

    return;

}

// D88 track info

void fdc_init(void) {
//void fdc_init(uint8_t *buffer) {

    fdc_command_drive=0;
    for(int i=0;i<4;i++) {
        fdc_command_cylinder[0]=0;
        if(fd_drive[i]==NULL) {
            fd_drive[i]=malloc(sizeof(FIL));
        }
    }
    fdc_dma_flag=0;
    fdc_exec_phase_finish=0;

    fdc_phase_flag=0;
    fdc_command_write_index=0;
    fdc_command_read_index=0;
    fdc_command_read_length=0;

    fdc_interrupt_flag=0;

    return;
}

uint8_t fdc_status(void) {

    uint8_t fdc_status;

    fdc_status=0;

    if(fdc_command_write_index!=0) {
        fdc_status|=0x10;
    }
    if(fdc_command_read_index!=0) {
        fdc_status|=0x10;
    }
    if(fdc_phase_flag==1) {
         fdc_status|=0x20;
        // Write command

    }
    if(fdc_phase_flag==2) {
        fdc_status|=0x40;
    }

    fdc_status|=0x80;
#ifdef FDC_DEBUG
    printf("[FS:%x]",fdc_status);
#endif
    return fdc_status;

}

// check first posision of sector

int32_t fdc_find_sector(uint8_t driveno,uint8_t track,uint8_t head,uint8_t sector,uint8_t number) {

    uint8_t count;

    uint32_t sector_ptr;
    UINT bytes_read;

    uint8_t track_conv;
    uint8_t head_conv;

//    lfs_file_seek(&lfs_handler,&fd_drive[driveno],0x20,LFS_SEEK_SET);
    f_lseek(fd_drive[driveno],0x20);

    // Convert 1DD track/head number to 2D (for PC-6001mk2SR)

    if((fd_media_type[driveno]==0x50)||(fd_media_type[driveno]==0x40)) { 
        track_conv=track*2;
        if(head) { track_conv++; }
        head_conv=0;

#ifdef FDC_DEBUG
    printf("[D88ss:%x-%x,%x-%x,%x]",track,track_conv,head,head_conv,sector);
#endif

    } else {
        track_conv = track;
        head_conv = head;
    }

    // find track top

    if((fd_media_type[driveno]==0x30)||(fd_media_type[driveno]==0x40)) {
        // for Single side D88 file
        for(int i=0;i<=track_conv;i++) {
//        lfs_file_read(&lfs_handler,&fd_drive[driveno],&sector_ptr,4);
            f_read(fd_drive[driveno],&sector_ptr,4,&bytes_read);
        }
    } else {
    // for Double side D88 file
        for(int i=0;i<=track_conv*2;i++) {
//        lfs_file_read(&lfs_handler,&fd_drive[driveno],&sector_ptr,4);
            f_read(fd_drive[driveno],&sector_ptr,4,&bytes_read);
        }
    }




#ifdef FDC_DEBUG
printf("[D88T:%d,%x]",driveno,sector_ptr);
#endif
///    lfs_file_seek(&lfs_handler,&fd_drive[driveno],sector_ptr,LFS_SEEK_SET);

        f_lseek(fd_drive[driveno],sector_ptr);    
#ifdef FDC_DEBUG
printf("[D88SF:%x,%x,%x]",track,head,sector);
#endif

    while(1) {
        sector_ptr+=0x10;
///        lfs_file_read(&lfs_handler,&fd_drive[driveno],sector_info,16);
        f_read(fd_drive[driveno],fdc_sector_info,16,&bytes_read);
#ifdef FDC_DEBUG
printf("[D88S:%x,%x,%x,%x]",fdc_sector_info[0],fdc_sector_info[1],fdc_sector_info[2],fdc_sector_info[3]);
#endif
        if((fdc_sector_info[2]==sector)&&(fdc_sector_info[1]==head_conv)&&(fdc_sector_info[0]==track_conv)) {
            // if(sector_info[3]==0) {
            //     fd_sector_size=128;
            // } else if(sector_info[3]==1) {
            //     fd_sector_size=256;
            // } else if(sector_info[3]==2) {
            //     fd_sector_size=512;
            // } else if(sector_info[3]==3) {
            //     fd_sector_size=1024;
            // }
            // break;
            if(fdc_sector_info[3]==number) {
                break;
            }
        }
        if(fdc_sector_info[3]==0) {
            f_read(fd_drive[driveno],dummy_buff,128,&bytes_read);
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],128,LFS_SEEK_CUR);
            sector_ptr+=128;
        } else if(fdc_sector_info[3]==1) {
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],256,LFS_SEEK_CUR);
            f_read(fd_drive[driveno],dummy_buff,256,&bytes_read);
            sector_ptr+=256;
        } else if(fdc_sector_info[3]==2) {
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],512,LFS_SEEK_CUR);
            f_read(fd_drive[driveno],dummy_buff,512,&bytes_read);
            sector_ptr+=512;
        } else if(fdc_sector_info[3]==3) {
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],1024,LFS_SEEK_CUR);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);
            sector_ptr+=1024;
        } else if(fdc_sector_info[3]==4) {
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],2048,LFS_SEEK_CUR);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);            
            sector_ptr+=1024;
        } else if(fdc_sector_info[3]==5) {
///            lfs_file_seek(&lfs_handler,&fd_drive[driveno],4096,LFS_SEEK_CUR);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);
            f_read(fd_drive[driveno],dummy_buff,1024,&bytes_read);                        
            sector_ptr+=1024;
        }



        count++;
        if(count>40) {

            return -1;

            break;
        } // error        
    }

//    fd_ptr=sector_ptr;
//    fd_sector_bytes=0;

    return sector_ptr;

}

#if 0
uint8_t fdc_read_dma(uint8_t driveno,int32_t fd_ptr,uint8_t *buffer) {

    uint8_t data;

    if(fd_drive_status[driveno]==0) return 0xff;

///    lfs_file_read(&lfs_handler,&fd_drive[driveno],buffer,fdc_read_sector_size);

    fdc_read_count+=fdc_read_sector_size;

    return data;

}
#endif

void fdc_command_write(uint8_t data) {

    uint8_t fdc_command;
    int32_t fdc_read_ptr;
    int32_t fdc_write_ptr;
    uint8_t fdc_st3;
    uint32_t fdc_dma_offset;
    UINT bytes_write;

    if(fdc_phase_flag==1) {

        if((fdc_command_buffer[0]&0xf)==5) {

            // write command

            fdc_command_drive=fdc_command_buffer[1]&3;

            if(fd_drive_status[fdc_command_drive]&2) {
                // write protected

                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x10 | (fdc_command_buffer[1]&7);   // Normal end
                fdc_result_buffer[1]=2;                                 // Not writable
                fdc_result_buffer[2]=0;

                fdc_result_buffer[3]=fdc_command_buffer[2];
                fdc_result_buffer[4]=fdc_command_buffer[3];
                fdc_result_buffer[5]=fdc_command_buffer[4];
                fdc_result_buffer[6]=fdc_command_buffer[5];

                fdc_interrupt_flag=1;
                return;

            } else {

#ifdef USE_FATFS
                f_write(fd_drive[fdc_command_drive],&data,1,&bytes_write);
#else
                lfs_file_write(&lfs_handler,&fd_drive[fdc_command_drive]],&data,1);
#endif

                fdc_write_count++;

                if(fdc_write_count==fdc_write_sector_size) {   // MUST MODIFY FOR MULTI SECTOR READ
#ifdef USE_FATFS
                    f_sync(fd_drive[fdc_command_drive]);
#endif
                    if(fdc_command_buffer[4]==fdc_command_eot) {

                        fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                        // Result status

                        fdc_command_read_length=7;
                        fdc_command_read_index=0;

                        fdc_result_buffer[0]=0x00 | (fdc_command_buffer[1]&7);   // Normal end
                        fdc_result_buffer[1]=0;
                        fdc_result_buffer[2]=0;

                        fdc_result_buffer[3]=fdc_command_buffer[2];
                        fdc_result_buffer[4]=fdc_command_buffer[3];
                        fdc_result_buffer[5]=fdc_command_buffer[4];
                        fdc_result_buffer[6]=fdc_command_buffer[5];

                        fdc_led_inactive(fdc_command_drive);

                    } else {
                        fdc_command_buffer[4]++;

                        fdc_write_ptr=fdc_find_sector(fdc_command_buffer[1]&3,fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);
                        fdc_write_count=0;

                        // No sector

                        if(fdc_read_ptr==-1) {  // abort
                            fdc_sector_not_found=1; 
                            fdc_phase_flag=2;

                            fdc_command_read_length=7;
                            fdc_command_read_index=0;

                            fdc_result_buffer[0]=0x40 | (fdc_command_buffer[1]&7);   // Abormal end
                            fdc_result_buffer[1]=0x4;                                // NoData
                            fdc_result_buffer[2]=0;

                            fdc_result_buffer[3]=fdc_command_buffer[2];
                            fdc_result_buffer[4]=fdc_command_buffer[3];
                            fdc_result_buffer[5]=fdc_command_buffer[4];
                            fdc_result_buffer[6]=fdc_command_buffer[5];

                            fdc_interrupt_flag=1;

                            fdc_led_inactive(fdc_command_drive);

                            return;
                        }

                    }
                }

            }
            fdc_interrupt_flag=1;
            return;

        } else if((fdc_command_buffer[0]&0xf)==0xd) {
            // Write ID
            // Just ignore command
#ifdef FDC_DEBUG
            printf("[ID:%02x]",data);
#endif
            fdc_write_count++;

            if(fdc_write_count==(fdc_command_buffer[3]*4)) {
                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                    // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x00 | (fdc_command_buffer[1]&7);   // Normal end
                fdc_result_buffer[1]=0;                                 // Not writable
                fdc_result_buffer[2]=0;

                fdc_result_buffer[3]=fdc_command_buffer[2];
                fdc_result_buffer[4]=fdc_command_buffer[3];
                fdc_result_buffer[5]=fdc_command_buffer[4];
                fdc_result_buffer[6]=fdc_command_buffer[5];

            }

            fdc_interrupt_flag=1;
            return;

        }
        return;
    }

    fdc_command_buffer[fdc_command_write_index++]=data;

    if(fdc_command_length[fdc_command_buffer[0]&0xf]<=fdc_command_write_index) {

        fdc_command_write_index=0;

        // execute command
#ifdef FDC_DEBUG
        printf("[FDC:%x]\n",fdc_command_buffer[0]&0xf);
#endif
        switch(fdc_command_buffer[0]&0xf) {

            case 0x3: // SPECIFY

#if 0            
                // Set DMA flag

                if(fdc_command_buffer[2]&1) {
                    fdc_dma_flag=0;
                } else {
                    fdc_dma_flag=1;
                }
#endif
                fdc_phase_flag=0;
                return;

            case 0x4: // SENSE DEVICE

                fdc_command_read_length =1;

                fdc_command_drive=fdc_command_buffer[1]&3;

                fdc_phase_flag=2;

                // check drive status

//                 printf("[DEVICE:%d]",fdc_command_drive);

                fdc_result_buffer[0]=fdc_command_buffer[1]&7;

//                if(fdc_command_drive==0) {
                if(fd_drive_status[fdc_command_drive]!=0) {

                    if(fd_drive_status[fdc_command_drive]&2) { // write protected
                        fdc_result_buffer[0]|=0x40;
                    }

                    if(fdc_command_cylinder[fdc_command_drive]==0) {
                        fdc_result_buffer[0]|=0x10; // Track 0
                    }

                    fdc_result_buffer[0]|=0x20; // READY
                    fdc_result_buffer[0]|=0x8;  // TWO SIDE 

                } else {

//                    fdc_result_buffer[0]=0x28 | fdc_command_drive;   // Drive not ready
                    fdc_result_buffer[0]|=0x80; // Fault

                }

                return;            

            case 0x5: // WRITE DATA 

                fdc_command_eot=fdc_command_buffer[6];
                fdc_write_sector_size=fdc_sector_count[fdc_command_buffer[5]];
//                fdc_dma_offset=0;
#ifdef FDC_DEBUG
            printf("[EOT:%d]",fdc_command_eot);
#endif
                fdc_write_ptr=fdc_find_sector(fdc_command_buffer[1]&3,fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);

                if(fdc_read_ptr==-1) {
                    // READ Error
                    fdc_sector_not_found=1;

                    fdc_phase_flag=2;

                    fdc_command_read_length=7;
                    fdc_command_read_index=0;

                    fdc_result_buffer[0]=0x40 | (fdc_command_buffer[1]&7);   // Abormal end
                    fdc_result_buffer[1]=0x4;                                // NoData
                    fdc_result_buffer[2]=0;

                    fdc_result_buffer[3]=fdc_command_buffer[2];
                    fdc_result_buffer[4]=fdc_command_buffer[3];
                    fdc_result_buffer[5]=fdc_command_buffer[4];
                    fdc_result_buffer[6]=fdc_command_buffer[5];

                    fdc_interrupt_flag=1;                    


                } else {
                    // set 
                    fdc_phase_flag=1;
                    fdc_interrupt_flag=1;
                    fdc_write_count=0;
                    fdc_sector_not_found=0;

                    fdc_led_active(fdc_command_buffer[1]&3);

                }

                return;

#if 0
            if(fdc_dma_flag) {

                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x40 | fdc_command_drive;  // Abort
                fdc_result_buffer[1]=0x2;                       // Write Protected
                fdc_result_buffer[2]=0;

                fdc_result_buffer[3]=fdc_command_buffer[2];
                fdc_result_buffer[4]=fdc_command_buffer[3];
                fdc_result_buffer[5]=fdc_command_buffer[4];
                fdc_result_buffer[6]=fdc_command_buffer[5];

                return;

            }
#endif
            return;


            case 0x6: // READ DATA 

                fdc_command_eot=fdc_command_buffer[6];
                fdc_read_sector_size=fdc_sector_count[fdc_command_buffer[5]];
//                fdc_dma_offset=0;
#ifdef FDC_DEBUG
            printf("[EOT:%d]",fdc_command_eot);
#endif
                fdc_read_ptr=fdc_find_sector(fdc_command_buffer[1]&3,fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);

                if(fdc_read_ptr==-1) {
                    // READ Error
                    fdc_sector_not_found=1;

                    fdc_phase_flag=2;

                    fdc_command_read_length=7;
                    fdc_command_read_index=0;

                    fdc_result_buffer[0]=0x40 | (fdc_command_buffer[1]&7);   // Abormal end
                    fdc_result_buffer[1]=0x4;                                // NoData
                    fdc_result_buffer[2]=0;

                    fdc_result_buffer[3]=fdc_command_buffer[2];
                    fdc_result_buffer[4]=fdc_command_buffer[3];
                    fdc_result_buffer[5]=fdc_command_buffer[4];
                    fdc_result_buffer[6]=fdc_command_buffer[5];

                    fdc_interrupt_flag=1;

                    return;

                } else {
                    // set 
                    fdc_phase_flag=1;
                    fdc_interrupt_flag=1;
                    fdc_read_count=0;
                    fdc_sector_not_found=0;

                    fdc_led_active(fdc_command_buffer[1]&3);

                }

                return;

            case 0x7: // RECALIBRATE

                // Seek to track 0

                fdc_command_drive=fdc_command_buffer[1]&3;
                fdc_command_cylinder[fdc_command_drive]=0;

                fdc_phase_flag=0;

                fdc_interrupt_flag=1;

                fdc_led_active(fdc_command_buffer[1]&3);
                sleep_us(FDC_LED_DELAY);
                fdc_led_inactive(fdc_command_buffer[1]&3);

                return;

            case 0x8: // SENSE INTERRUPT STATUS

                fdc_phase_flag=2;
                fdc_command_read_length=2;
                fdc_command_read_index=0;

                // check disk status

//                 printf("[SENSE:%d]",fdc_command_drive);

//                if(fdc_command_drive==0) {
                if(fd_drive_status[fdc_command_drive]!=0) {

                    fdc_result_buffer[0]=0x20 | fdc_command_drive;   // Seek end
                    fdc_result_buffer[1]=fdc_command_cylinder[fdc_command_drive];    

//                printf("[%d]",fdc_command_cylinder[fdc_command_drive]);    

                } else {

//                    fdc_result_buffer[0]=0x28 | fdc_command_drive;   // Drive not ready
                    fdc_result_buffer[0]=0xa0 | fdc_command_drive;   // Ivalid
                    fdc_result_buffer[1]=0xff;    


                }

                return;

            case 0xd: // WRITE ID (Format)

                fdc_phase_flag=1;
                fdc_interrupt_flag=1;
                fdc_write_count=0;
                fdc_sector_not_found=0;

                fdc_led_active(fdc_command_buffer[1]&3);
                sleep_us(FDC_LED_DELAY);
                fdc_led_inactive(fdc_command_buffer[1]&3);

                return;


            case 0xf: // SEEK

                fdc_command_drive=fdc_command_buffer[1]&3;
                fdc_command_cylinder[fdc_command_drive]=fdc_command_buffer[2];
                
//                 printf("[SEEK%d:%d]",fdc_command_drive,fdc_command_cylinder[fdc_command_drive]);

                fdc_phase_flag=0;

                fdc_interrupt_flag=1;

                fdc_led_active(fdc_command_buffer[1]&3);
                sleep_us(FDC_LED_DELAY);
                fdc_led_inactive(fdc_command_buffer[1]&3);

                return;


            default:   // Error
                fdc_phase_flag=0;
                fdc_command_read_length=1;
                fdc_result_buffer[0]=0x80;

                return;
        }
    }

//    fdc_phase_flag=1;

    return;

}

uint8_t fdc_command_read() {

    uint8_t result;
    uint8_t data;
    int32_t fdc_read_ptr;
    UINT    bytes_read;

    if(fdc_phase_flag==2) {
        if(fdc_command_read_length>fdc_command_read_index) {
            result=fdc_result_buffer[fdc_command_read_index++];
        }
        if(fdc_command_read_length<=fdc_command_read_index) {
            fdc_phase_flag=0;
            fdc_command_read_index=0;
            fdc_led_inactive(fdc_command_buffer[1]&3);
        }
        // printf("[!%x:%d/%d]",result,fdc_command_read_index,fdc_command_read_length);
        return result;
    } else if(fdc_phase_flag==1) { 

        fdc_command_drive=fdc_command_buffer[1]&3;

        // READ COMMAND
        if((fdc_command_buffer[0]&0xf)==0x6) {

#ifdef USE_FATFS
            f_read(fd_drive[fdc_command_drive],&data,1,&bytes_read);
#else 
            lfs_file_read(&lfs_handler,&fd_drive[fdc_command_drive]],&data,1);
#endif

//        printf("[%02x]",data);

            fdc_read_count++;
            if(fdc_read_count==fdc_read_sector_size) {   // MUST MODIFY FOR MULTI SECTOR READ

                if(fdc_command_buffer[4]==fdc_command_eot) {

                fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                        // Result status

                fdc_command_read_length=7;
                fdc_command_read_index=0;

                fdc_result_buffer[0]=0x00 | (fdc_command_buffer[1]&7);   // Normal end
                fdc_result_buffer[1]=0;
                fdc_result_buffer[2]=0;

                fdc_result_buffer[3]=fdc_command_buffer[2];
                fdc_result_buffer[4]=fdc_command_buffer[3];
                fdc_result_buffer[5]=fdc_command_buffer[4];
                fdc_result_buffer[6]=fdc_command_buffer[5];

                } else {
                    fdc_command_buffer[4]++;

                    fdc_read_ptr=fdc_find_sector(fdc_command_buffer[1]&3,fdc_command_buffer[2],fdc_command_buffer[3],fdc_command_buffer[4],fdc_command_buffer[5]);
                    fdc_read_count=0;

                    if(fdc_read_ptr==-1) {  // abort
                        fdc_sector_not_found=1; 
                        fdc_phase_flag=2;

                        fdc_command_read_length=7;
                        fdc_command_read_index=0;

                        fdc_result_buffer[0]=0x40 | (fdc_command_buffer[1]&7);   // Abormal end
                        fdc_result_buffer[1]=0x4;                                // NoData
                        fdc_result_buffer[2]=0;

                        fdc_result_buffer[3]=fdc_command_buffer[2];
                        fdc_result_buffer[4]=fdc_command_buffer[3];
                        fdc_result_buffer[5]=fdc_command_buffer[4];
                        fdc_result_buffer[6]=fdc_command_buffer[5];

                        
                    }

                }

            }

            fdc_interrupt_flag=1;
            return data;

        }
    }

    return 0x80;

}

void fdc_tc() {

    uint8_t result;
    uint8_t data;
    UINT    bytes_read;

    if(fdc_phase_flag==1) { 

        fdc_command_drive=fdc_command_buffer[1]&3;

        fdc_phase_flag=2;
//                        fdc_exec_phase_finish=1;

                        // Result status

        fdc_command_read_length=7;
        fdc_command_read_index=0;

        fdc_result_buffer[0]=0x00 | (fdc_command_buffer[1]&7);   // Normal end
        fdc_result_buffer[1]=0;
        fdc_result_buffer[2]=0;

        fdc_result_buffer[3]=fdc_command_buffer[2];
        fdc_result_buffer[4]=fdc_command_buffer[3];
        fdc_result_buffer[5]=fdc_command_buffer[4];
        fdc_result_buffer[6]=fdc_command_buffer[5];

        fdc_led_inactive(fdc_command_drive);

        return;

    }

    return;


}


// check inserted disk

void fdc_check(uint8_t driveno) {

    uint8_t flags;
    UINT bytes_read;
    uint32_t track_info;

#ifdef USE_FATFS
    if(f_lseek(fd_drive[driveno],0x1a)!=FR_OK) {
#else
    if(lfs_file_seek(&lfs_handler,&fd_drive[driveno],0x1a,LFS_SEEK_SET)) {
#endif
        fd_drive_status[driveno]=0;
    }

#ifdef USE_FATFS
    f_read(fd_drive[driveno],&flags,1,&bytes_read);
#else
    lfs_file_read(&lfs_handler,&fd_drive[driveno],&flags,1);
#endif

#ifdef FDC_DEBUG
printf("[D88:%d]",flags);
#endif

    if(flags==0) {
        fd_drive_status[driveno]=1;
    } else {
        fd_drive_status[driveno]=3;        
    }

#ifdef USE_FATFS
    f_read(fd_drive[driveno],&flags,1,&bytes_read);
#else
    lfs_file_read(&lfs_handler,&fd_drive[driveno],&flags,1);
#endif

#ifdef FDC_DEBUG
printf("[Media:%d]",flags);
#endif

    fd_media_type[driveno]=flags;

    if(flags==0x10) {
        // 1DD check for 2DD Media
        // Read track 1 (CHS=0/1/1)

#ifdef USE_FATFS
    f_lseek(fd_drive[driveno],0x24);
    f_read(fd_drive[driveno],&track_info,4,&bytes_read);

#else
    lfs_file_seek(&lfs_handler,&fd_drive[driveno],0x24,LFS_SEEK_SET);)
    lfs_file_read(&lfs_handler,&fd_drive[driveno],&track_info,4);        
#endif

        if(track_info==0) {
            fd_media_type[driveno]=0x50;
        }

    }



    return;

}


//uint8_t fdc_mount(uint8_t driveno,lfs_t lfs,lfs_file_t file) {
//
//}

uint8_t fdc_unmount(uint8_t driveno) {

}