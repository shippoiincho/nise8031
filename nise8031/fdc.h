//void fdc_init(uint8_t *buffer);
void fdc_init(void);
uint8_t fdc_status(void);
void fdc_command_write(uint8_t data);
uint8_t fdc_command_read();
//uint8_t fdc_mount(uint8_t driveno,lfs_t lfs,lfs_file_t file);
uint8_t fdc_unmount(uint8_t driveno);
void fdc_check(uint8_t driveno);
void fdc_tc(void);

//extern lfs_t lfs_handler;
extern FIL *fd_drive[4];
extern uint8_t fd_drive_status[4];
extern uint32_t fdc_dma_datasize;
extern uint8_t fdc_interrupt_flag;
extern uint8_t fdc_tc_flag;