// USE FatFS or LittleFS
#define USE_FATFS
#define FDC_DEBUG



#ifdef USE_FATFS
#include "f_util.h"
#include "ff.h"
#else
#include "lfs.h"
#include "lfs_util.h"
#endif

#ifdef USE_FATFS
void fdc_init(void);
#else 
void fdc_init(uint8_t *buffer);
#endif
uint8_t fdc_status(void);
void fdc_command_write(uint8_t data);
uint8_t fdc_command_read();
//uint8_t fdc_mount(uint8_t driveno,lfs_t lfs,lfs_file_t file);
uint8_t fdc_unmount(uint8_t driveno);
void fdc_check(uint8_t driveno);
void fdc_tc(void);

#ifdef USE_FATFS
extern FIL *fd_drive[4];
#else
extern lfs_t lfs_handler;
extern lfs_file_t *fd_drive[4];
#endif
extern uint8_t fd_drive_status[4];
extern uint32_t fdc_dma_datasize;
extern uint8_t fdc_interrupt_flag;
extern uint8_t fdc_tc_flag;
extern uint8_t fd_media_type[4];

