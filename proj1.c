#include <mega32.h>
#include <stdio.h>
#include <delay.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#asm
    .equ __lcd_port=0x18 ; PORTB
#endasm
#include <lcd.h>

// ===================== KEYPAD MATRIX =====================
unsigned char code[4][4] = { 
    {7, 4, 1, 10},   
    {8, 5, 2, 0},   
    {9, 6, 3, 11}, 
    {12,13,14,15}   
}; 

// ===================== GLOBAL VARIABLES =====================
char pass[] = "1234";           // User password
char p1[5] = "";              
uint8_t p1_len = 0;            
uint8_t att = 0;               
uint8_t max_attempts = 3;      
bool in_change_pass_mode = false;  

// ===================== DELAY FUNCTION =====================
void delay(int time) {
    unsigned char i = 0;
    TCCR0 = 0x05;
    TCNT0 = 246;
  
    while (i < time) {   
        while ((TIFR & 0x01) == 0) {}
        TIFR = 0x01;
        TCNT0 = 246;  
        i++;  
    }       
}

// ===================== LED WARNING FLASH =====================
void led_warning_flash(void) {
    uint8_t i;
    
    // Fast LED blink - 5 times
    for (i = 0; i < 5; i++) {
        PORTB |= (1 << 3);   // LED ON
        delay(20);           // 100ms
        PORTB &= ~(1 << 3);  // LED OFF
        delay(20);           // 100ms
    }
}

// ===================== TIMER FUNCTION WITH LED WARNING =====================
void timer(int time) {
    unsigned char i = 0;
    unsigned char j;
    char s[3];
   
    TCCR0 = 0x05;
    TCNT0 = 12;
    lcd_clear();
    lcd_puts("SYSTEM LOCKED!");
    lcd_gotoxy(0, 1);
    lcd_puts("Wait: ");
    
    while (i < time) {
        // Update countdown and turn LED ON
        sprintf(s, "%u", (unsigned)(time - i));
        lcd_gotoxy(6, 1);
        lcd_puts(s);
        lcd_puts("s  ");
        PORTB |= (1 << 3);  // LED ON
        
        // Wait 20ms  (LED ON)
        for (j = 0; j < 2; j++) {
            while ((TIFR & 0x01) == 0) {}
            TIFR = 0x01;
            TCNT0 = 12;
        }
        
        // Turn LED OFF
        PORTB &= ~(1 << 3);
        
        // Wait 20ms  (LED OFF)
        for (j = 0; j < 2; j++) {
            while ((TIFR & 0x01) == 0) {}
            TIFR = 0x01;
            TCNT0 = 12;
        }
        
        i++;  
    }       
    
    // Turn off LED
    PORTB &= ~(1 << 3);
    lcd_clear();    
    lcd_gotoxy(0, 0);
    lcd_puts("You can retry");
    delay(150);
}

// ===================== KEYPAD SCAN =====================
unsigned char scan_key(void) {
    unsigned char i, data, num_key, temp;
    num_key = 0xFF;
    temp = 0x70;
    
    for (i = 0; i < 4; i++) {    
        PORTC = temp;
        delay(1);  // ~10ms delay
        data = PINC & 0x0F;
        
        if (data == 0x07) {num_key = code[0][i]; break;}
        if (data == 0x0B) {num_key = code[1][i]; break;}
        if (data == 0x0D) {num_key = code[2][i]; break;}
        if (data == 0x0E) {num_key = code[3][i]; break;}
        
        temp = ((temp >> 1) | 0x80) & 0xF0;
    }
    return num_key;
}

// ===================== DISPLAY MASKED PASSWORD =====================
void display_input(void) {
    uint8_t i;
    lcd_clear();
    
    if (in_change_pass_mode) {
        lcd_puts("New Pass: ");
    } else {
        lcd_puts("Pass: ");
    }
    
    for (i = 0; i < p1_len; i++) {
        lcd_putchar('*');
    }
}

// ===================== MAIN PROGRAM =====================
void main(void) {
    uint8_t result;
    
    DDRB = 0xFF;  // PORTB = LCD + LED (all output)
    DDRC = 0xF0;  // PC4-PC7=Output (Rows), PC0-PC3=Input (Cols)
    PORTC = 0x0F; // Pull-up for columns
    DDRD = 0x00;  // PORTD not used
    
    lcd_init(16);  
    lcd_clear();  
    lcd_gotoxy(0, 0);
    lcd_puts("Digital Lock");
    lcd_gotoxy(0, 1);
    lcd_puts("State: LOCKED");
    delay(200);
    
    lcd_clear();
    lcd_puts("Enter Password:");
    
    while (1) {
        result = scan_key();
        
        if (result != 255) {
            
            // ========== CHANGE PASSWORD MODE ==========
            if (in_change_pass_mode) {
                if (result == 11) {  // Enter
                    if (p1_len == 4) {
                        strcpy(pass, p1);
                        lcd_clear();
                        lcd_puts("Pass Changed!");
                        delay(150);
                        
                        in_change_pass_mode = false;
                        lcd_clear();
                        lcd_puts("Enter Password:");
                    }
                    p1_len = 0;
                    p1[0] = '\0';
                } 
                else if (result == 10) {  // on/c - Clear all
                    p1_len = 0;
                    p1[0] = '\0';
                    display_input();
                }
                else if (result == 14) {  // C - Backspace (delete last digit)
                    if (p1_len > 0) {
                        p1_len--;
                        p1[p1_len] = '\0';
                    }
                    display_input();
                } 
                else if (result <= 9 && p1_len < 4) {
                    p1[p1_len++] = '0' + result;
                    p1[p1_len] = '\0';
                    display_input();
                }
                
                delay(30);
                continue;
            }
            
            // ========== NORMAL MODE ==========
            if (result == 11) {  // Enter key
                // Check Password
                if (strcmp(p1, pass) == 0) {
                    lcd_clear(); 
                    lcd_puts("Access Granted!");
                    delay(200);
                    
                    // Ask for password change
                    lcd_clear();
                    lcd_puts("Change Pass?");
                    lcd_gotoxy(0, 1);
                    lcd_puts("1=Yes  0=No");
                    
                    // Wait for response
                    while(1) {
                        result = scan_key();
                        if (result == 1) {
                            // Enter change password mode
                            in_change_pass_mode = true;
                            lcd_clear();
                            lcd_puts("Enter New Pass:");
                            p1_len = 0;
                            p1[0] = '\0';
                            delay(200);
                            break;
                        } 
                        else if (result == 0) {
                            // Exit
                            lcd_clear();
                            lcd_puts("Enter Password:");
                            break;
                        }
                        delay(1);
                    }
                    
                    att = 0;
                    p1_len = 0;
                    p1[0] = '\0';
                }
                // Wrong Password
                else {
                    p1_len = 0;
                    p1[0] = '\0';
                    att++;
                    
                    if (att >= max_attempts) {
                        led_warning_flash();  // LED warning flash
                        timer(60);            // Lock for 60 seconds with LED blink
                        att = 0;
                        
                        lcd_clear();
                        lcd_puts("Enter Password:");
                    } else {
                        lcd_clear();
                        lcd_puts("Wrong Password!");
                        lcd_gotoxy(0, 1);
                        lcd_puts("Attempts: ");
                        lcd_putchar('0' + att);
                        lcd_putchar('/');
                        lcd_putchar('0' + max_attempts);
                        delay(200);
                        
                        lcd_clear();
                        lcd_puts("Enter Password:");
                    }
                }
            }
            else if (result == 10) {  // on/c - Clear all input
                p1_len = 0;
                p1[0] = '\0';
                display_input();
            }
            else if (result == 14) {  // C - Backspace (delete last digit)
                if (p1_len > 0) {
                    p1_len--;
                    p1[p1_len] = '\0';
                }
                display_input();
            }
            else if (result <= 9 && p1_len < 4) {
                p1[p1_len++] = '0' + result;
                p1[p1_len] = '\0';
                display_input();
            }
            
            delay(30);  // Debounce
        }
        
        delay(1);
    }
}