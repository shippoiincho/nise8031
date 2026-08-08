extern void i2c_write_byte(uint8_t val);
extern void lcd_toggle_enable(uint8_t val);
extern void lcd_send_byte(uint8_t val, int mode);
extern void lcd_clear(void);
extern void lcd_set_cursor(int line, int position);
extern void lcd_string(const char *s);
extern void lcd_init(void);