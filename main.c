#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
 
#define LED_PIN     PB1
#define BUZZER_PIN  PB0
#define BUTTON_PIN  PD2
 
/* ---------------- LCD (4-bit mode, HD44780) ---------------- */
#define LCD_PORT   PORTC
#define LCD_DDR    DDRC
#define LCD_RS     PC0
#define LCD_EN     PC1
#define LCD_D4     PC2
#define LCD_D5     PC3
#define LCD_D6     PC4
#define LCD_D7     PC5
 
static void lcd_pulse_enable(void) {
    LCD_PORT |= (1 << LCD_EN);
    _delay_us(1);
    LCD_PORT &= ~(1 << LCD_EN);
    _delay_us(100);
}
 
static void lcd_send_nibble(uint8_t nibble) {
    // Clear the four data-line bits, then set them according to the nibble
    LCD_PORT &= ~((1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7));
    if (nibble & 0x01) LCD_PORT |= (1 << LCD_D4);
    if (nibble & 0x02) LCD_PORT |= (1 << LCD_D5);
    if (nibble & 0x04) LCD_PORT |= (1 << LCD_D6);
    if (nibble & 0x08) LCD_PORT |= (1 << LCD_D7);
    lcd_pulse_enable();
}
 
static void lcd_write(uint8_t value, uint8_t is_data) {
    if (is_data) LCD_PORT |= (1 << LCD_RS);
    else         LCD_PORT &= ~(1 << LCD_RS);
 
    lcd_send_nibble(value >> 4);   // high nibble first
    lcd_send_nibble(value & 0x0F); // then low nibble
    _delay_us(50);
}
 
void lcd_command(uint8_t cmd) { lcd_write(cmd, 0); }
void lcd_data(uint8_t data)   { lcd_write(data, 1); }
 
void lcd_clear(void) {
    lcd_command(0x01);
    _delay_ms(2);
}
 
void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_command(addr);
}
 
void lcd_print(const char *s) {
    while (*s) lcd_data((uint8_t)*s++);
}
 
void lcd_init(void) {
    LCD_DDR |= (1 << LCD_RS) | (1 << LCD_EN) | (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7);
    _delay_ms(40); // wait for LCD power-on
 
    // Standard HD44780 4-bit init sequence
    lcd_send_nibble(0x03); _delay_ms(5);
    lcd_send_nibble(0x03); _delay_us(150);
    lcd_send_nibble(0x03); _delay_us(150);
    lcd_send_nibble(0x02); // switch to 4-bit mode
 
    lcd_command(0x28); // 4-bit, 2 lines, 5x8 font
    lcd_command(0x0C); // display on, cursor off, blink off
    lcd_command(0x06); // entry mode: increment cursor, no shift
    lcd_clear();
}
 
/* ---------------- Peripheral setup ---------------- */
void io_init(void) {
    DDRB |= (1 << LED_PIN) | (1 << BUZZER_PIN);
    PORTB &= ~((1 << LED_PIN) | (1 << BUZZER_PIN));
 
    DDRD  &= ~(1 << BUTTON_PIN);
    PORTD |=  (1 << BUTTON_PIN); // internal pull-up
}
void delay_ms_var(uint16_t ms) {
    while (ms >= 10) { _delay_ms(10); ms -= 10; }
    while (ms--)      { _delay_ms(1);  }
}

void buzzer_beep(uint16_t ms) {
    PORTB |= (1 << BUZZER_PIN);    // base driven high -> transistor on -> buzzer sounds
    delay_ms_var(ms);             
    PORTB &= ~(1 << BUZZER_PIN);
}

/* ---------------- External interrupt (wake source) ---------------- */
void int0_init(void) {
    EICRA &= ~((1 << ISC01) | (1 << ISC00)); // low level triggers INT0
    EIFR  |= (1 << INTF0);
    EIMSK |= (1 << INT0);
}
 
volatile uint8_t wake_flag = 0;
 
ISR(INT0_vect) {
    wake_flag = 1;
}
 
/* ---------------- Sleep control ---------------- */
void enter_power_down_sleep(void) {
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("SLEEPING...");
    lcd_set_cursor(1, 0);
    lcd_print("Press button");
    PORTB &= ~(1 << LED_PIN);
 
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    sleep_enable();
    sei();
    sleep_cpu();  // CPU halts here until INT0 wakes it
 
    /* --- resumes here after ISR --- */
    sleep_disable();
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("AWAKE!");
    buzzer_beep(150);
    _delay_ms(500);
}
 
int main(void) {
    io_init();
    int0_init();
    lcd_init();
    sei();
 
    lcd_set_cursor(0, 0);
    lcd_print("SYSTEM START");
    _delay_ms(800);
 
    while (1) {
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print("RUNNING...");
 
        for (uint8_t i = 0; i < 5; i++) {
            PORTB |= (1 << LED_PIN);
            _delay_ms(200);
            PORTB &= ~(1 << LED_PIN);
            _delay_ms(200);
        }
 
        enter_power_down_sleep();
        wake_flag = 0;
    }
}
 