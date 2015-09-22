/*
    This file is part of Repetier-Firmware.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Repetier-Firmware.  If not, see <http://www.gnu.org/licenses/>.

*/

#define UI_MAIN 1
#include "Repetier.h"
// The uimenu.h declares static variables of menus, which must be declared only once.
// It does not define interfaces for other modules, so should never be included elsewhere
#include "uimenu.h"

extern const int8_t encoder_table[16] PROGMEM ;
#include <math.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

#if FEATURE_SERVO > 0 && UI_SERVO_CONTROL > 0
#if   UI_SERVO_CONTROL == 1 && defined(SERVO0_NEUTRAL_POS)
uint16_t servoPosition = SERVO0_NEUTRAL_POS;
#elif UI_SERVO_CONTROL == 2 && defined(SERVO1_NEUTRAL_POS)
uint16_t servoPosition = SERVO1_NEUTRAL_POS;
#elif UI_SERVO_CONTROL == 3 && defined(SERVO2_NEUTRAL_POS)
uint16_t servoPosition = SERVO2_NEUTRAL_POS;
#elif UI_SERVO_CONTROL == 4 && defined(SERVO3_NEUTRAL_POS)
uint16_t servoPosition = SERVO3_NEUTRAL_POS;
#else
uint16_t servoPosition = 1500;
#endif
#endif
//Davinci Specific, custom dialog page
char uipagedialog[4][MAX_COLS+1];

#if BEEPER_TYPE==2 && defined(UI_HAS_I2C_KEYS) && UI_I2C_KEY_ADDRESS!=BEEPER_ADDRESS
#error Beeper address and i2c key address must be identical
#else
#if BEEPER_TYPE==2
#define UI_I2C_KEY_ADDRESS BEEPER_ADDRESS
#endif
#endif

static TemperatureController *currHeaterForSetup;    // pointer to extruder or heatbed temperature controller

#if UI_AUTORETURN_TO_MENU_AFTER!=0
long ui_autoreturn_time=0;
//Davinci Specific, to block auto return when in sub menu
bool benable_autoreturn=true;
#endif
#if FEATURE_BABYSTEPPING
int zBabySteps = 0;
#endif
//Davinci Specific, to be able to disable feature
#if FEATURE_BEEPER
bool enablesound = true;
#endif

//Davinci Specific, powersave management
#if UI_AUTOLIGHTOFF_AFTER!=0
millis_t UIDisplay::ui_autolightoff_time=-1;
#endif

//Davinci Specific, filter for Interface Easy/Advanced
uint8_t UIDisplay::display_mode=ADVANCED_MODE;

//Davinci Specific, to get fancy effect
void playsound(int tone,int duration)
{
#if FEATURE_BEEPER
if (!HAL::enablesound)return;
#if FEATURE_WATCHDOG
            HAL::pingWatchdog();
#endif
HAL::tone(BEEPER_PIN, tone);
HAL::delayMilliseconds(duration);
HAL::noTone(BEEPER_PIN);
#endif
}
float Z_probe[3];
void beep(uint8_t duration,uint8_t count)
{
#if FEATURE_BEEPER
//Davinci Specific, to be able to disable sound from menu
if (!HAL::enablesound)return;
#if BEEPER_TYPE!=0
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
    SET_OUTPUT(BEEPER_PIN);
#endif
#if BEEPER_TYPE==2
    HAL::i2cStartWait(BEEPER_ADDRESS+I2C_WRITE);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    HAL::i2cWrite( 0x14); // Start at port a
#endif
#endif
    for(uint8_t i=0; i < count; i++)
    {
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
#if defined(BEEPER_TYPE_INVERTING) && BEEPER_TYPE_INVERTING
        WRITE(BEEPER_PIN,LOW);
#else
        WRITE(BEEPER_PIN,HIGH);
#endif
#else
#if UI_DISPLAY_I2C_CHIPTYPE==0
#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
        HAL::i2cWrite(uid.outputMask & ~BEEPER_PIN);
#else
        HAL::i2cWrite(~BEEPER_PIN);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
        HAL::i2cWrite((BEEPER_PIN) | uid.outputMask);
        HAL::i2cWrite(((BEEPER_PIN) | uid.outputMask)>>8);
#endif
#endif
        HAL::delayMilliseconds(duration);
#if BEEPER_TYPE==1 && defined(BEEPER_PIN) && BEEPER_PIN>=0
#if defined(BEEPER_TYPE_INVERTING) && BEEPER_TYPE_INVERTING
        WRITE(BEEPER_PIN,HIGH);
#else
        WRITE(BEEPER_PIN,LOW);
#endif
#else
#if UI_DISPLAY_I2C_CHIPTYPE==0

#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
        HAL::i2cWrite((BEEPER_PIN) | uid.outputMask);
#else
        HAL::i2cWrite(255);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
        HAL::i2cWrite( uid.outputMask);
        HAL::i2cWrite(uid.outputMask>>8);
#endif
#endif
        HAL::delayMilliseconds(duration);
    }
#if BEEPER_TYPE==2
    HAL::i2cStop();
#endif
#endif
#endif
}

bool UIMenuEntry::showEntry() const
{
    bool ret = true;
//Davinci Specific, filter fr UI : Easy/Advanced
    uint8_t f,f2,f3;
    //check what mode is targeted
    f3 = HAL::readFlashByte((PGM_P)&display_mode);
    //if not for current mode not need to continue
    if (!(f3 & UIDisplay::display_mode) ) return false;
    f = HAL::readFlashByte((PGM_P)&filter);
    if(f != 0)
        ret = (f & Printer::menuMode) != 0;
    if(ret && (f2 = HAL::readFlashByte((PGM_P)&nofilter)) != 0)
        ret = (f2 & Printer::menuMode) == 0;
    return ret;
}

#if UI_DISPLAY_TYPE != NO_DISPLAY
UIDisplay uid;
char displayCache[UI_ROWS][MAX_COLS+1];

// Menu up sign - code 1
// ..*.. 4
// .***. 14
// *.*.* 21
// ..*.. 4
// ***.. 28
// ..... 0
// ..... 0
// ..... 0
const uint8_t character_back[8] PROGMEM = {4,14,21,4,28,0,0,0};
// Degrees sign - code 2
// ..*.. 4
// .*.*. 10
// ..*.. 4
// ..... 0
// ..... 0
// ..... 0
// ..... 0
// ..... 0
const uint8_t character_degree[8] PROGMEM = {4,10,4,0,0,0,0,0};
// selected - code 3
// ..... 0
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_selected[8] PROGMEM = {0,31,31,31,31,31,0,0};
// unselected - code 4
// ..... 0
// ***** 31
// *...* 17
// *...* 17
// *...* 17
// *...* 17
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_unselected[8] PROGMEM = {0,31,17,17,17,31,0,0};
// unselected - code 5
// ..*.. 4
// .*.*. 10
// .*.*. 10
// .*.*. 10
// .*.*. 10
// .***. 14
// ***** 31
// ***** 31
// .***. 14
const uint8_t character_temperature[8] PROGMEM = {4,10,10,10,14,31,31,14};
// unselected - code 6
// ..... 0
// ***.. 28
// ***** 31
// *...* 17
// *...* 17
// ***** 31
// ..... 0
// ..... 0
const uint8_t character_folder[8] PROGMEM = {0,28,31,17,17,31,0,0};

// printer ready - code 7
// *...* 17
// .*.*. 10
// ..*.. 4
// *...* 17
// ..*.. 4
// .*.*. 10
// *...* 17
// *...* 17
const byte character_ready[8] PROGMEM = {17,10,4,17,4,10,17,17};
//Davinci Specific, create bed icon
// Bed - code 7
// ..... 0
// ***** 31
// *.*.* 21
// *...* 17
// *.*.* 21
// ***** 31
// ..... 0
// ..... 0
const byte character_bed[8] PROGMEM = {0,31,21,17,21,31,0,0};

const long baudrates[] PROGMEM = {9600,14400,19200,28800,38400,56000,57600,76800,111112,115200,128000,230400,250000,256000,
                                  460800,500000,921600,1000000,1500000,0
                                 };

#define LCD_ENTRYMODE			0x04			/**< Set entrymode */

/** @name GENERAL COMMANDS */
/*@{*/
#define LCD_CLEAR			0x01	/**< Clear screen */
#define LCD_HOME			0x02	/**< Cursor move to first digit */
/*@}*/

/** @name ENTRYMODES */
/*@{*/
#define LCD_ENTRYMODE			0x04			/**< Set entrymode */
#define LCD_INCREASE		LCD_ENTRYMODE | 0x02	/**<	Set cursor move direction -- Increase */
#define LCD_DECREASE		LCD_ENTRYMODE | 0x00	/**<	Set cursor move direction -- Decrease */
#define LCD_DISPLAYSHIFTON	LCD_ENTRYMODE | 0x01	/**<	Display is shifted */
#define LCD_DISPLAYSHIFTOFF	LCD_ENTRYMODE | 0x00	/**<	Display is not shifted */
/*@}*/

/** @name DISPLAYMODES */
/*@{*/
#define LCD_DISPLAYMODE			0x08			/**< Set displaymode */
#define LCD_DISPLAYON		LCD_DISPLAYMODE | 0x04	/**<	Display on */
#define LCD_DISPLAYOFF		LCD_DISPLAYMODE | 0x00	/**<	Display off */
#define LCD_CURSORON		LCD_DISPLAYMODE | 0x02	/**<	Cursor on */
#define LCD_CURSOROFF		LCD_DISPLAYMODE | 0x00	/**<	Cursor off */
#define LCD_BLINKINGON		LCD_DISPLAYMODE | 0x01	/**<	Blinking on */
#define LCD_BLINKINGOFF		LCD_DISPLAYMODE | 0x00	/**<	Blinking off */
/*@}*/

/** @name SHIFTMODES */
/*@{*/
#define LCD_SHIFTMODE			0x10			/**< Set shiftmode */
#define LCD_DISPLAYSHIFT	LCD_SHIFTMODE | 0x08	/**<	Display shift */
#define LCD_CURSORMOVE		LCD_SHIFTMODE | 0x00	/**<	Cursor move */
#define LCD_RIGHT		LCD_SHIFTMODE | 0x04	/**<	Right shift */
#define LCD_LEFT		LCD_SHIFTMODE | 0x00	/**<	Left shift */
/*@}*/

/** @name DISPLAY_CONFIGURATION */
/*@{*/
#define LCD_CONFIGURATION		0x20				/**< Set function */
#define LCD_8BIT		LCD_CONFIGURATION | 0x10	/**<	8 bits interface */
#define LCD_4BIT		LCD_CONFIGURATION | 0x00	/**<	4 bits interface */
#define LCD_2LINE		LCD_CONFIGURATION | 0x08	/**<	2 line display */
#define LCD_1LINE		LCD_CONFIGURATION | 0x00	/**<	1 line display */
//for HD44780 and clones
#define LCD_5X10		LCD_CONFIGURATION | 0x04	/**<	5 X 10 dots */
#define LCD_5X7			LCD_CONFIGURATION | 0x00	/**<	5 X 7 dots */
//Davinci Specific, for Winstar 1604A 
#define LCD_5X11        LCD_CONFIGURATION | 0x04    /**<    5 X 18 dots */
#define LCD_5X8         LCD_CONFIGURATION | 0x00    /**<    5 X 8 dots */

#define LCD_SETCGRAMADDR 0x40

#define lcdPutChar(value) lcdWriteByte(value,1)
#define lcdCommand(value) lcdWriteByte(value,0)

static const uint8_t LCDLineOffsets[] PROGMEM = UI_LINE_OFFSETS;
static const char versionString[] PROGMEM = UI_VERSION_STRING;


#if UI_DISPLAY_TYPE == DISPLAY_I2C

// ============= I2C LCD Display driver ================
inline void lcdStartWrite()
{
    HAL::i2cStartWait(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
#if UI_DISPLAY_I2C_CHIPTYPE == 1
    HAL::i2cWrite( 0x14); // Start at port a
#endif
}
inline void lcdStopWrite()
{
    HAL::i2cStop();
}
void lcdWriteNibble(uint8_t value)
{
#if UI_DISPLAY_I2C_CHIPTYPE==0
    value |= uid.outputMask;
#if UI_DISPLAY_D4_PIN==1 && UI_DISPLAY_D5_PIN==2 && UI_DISPLAY_D6_PIN==4 && UI_DISPLAY_D7_PIN==8
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#else
    uint8_t v=(value & 1?UI_DISPLAY_D4_PIN:0)|(value & 2?UI_DISPLAY_D5_PIN:0)|(value & 4?UI_DISPLAY_D6_PIN:0)|(value & 8?UI_DISPLAY_D7_PIN:0);
    HAL::i2cWrite((v) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(v);
#
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
    unsigned int v=(value & 1?UI_DISPLAY_D4_PIN:0)|(value & 2?UI_DISPLAY_D5_PIN:0)|(value & 4?UI_DISPLAY_D6_PIN:0)|(value & 8?UI_DISPLAY_D7_PIN:0) | uid.outputMask;
    unsigned int v2 = v | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(v2 & 255);
    HAL::i2cWrite(v2 >> 8);
    HAL::i2cWrite(v & 255);
    HAL::i2cWrite(v >> 8);
#endif
}
void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if UI_DISPLAY_I2C_CHIPTYPE==0
    uint8_t mod = (rs?UI_DISPLAY_RS_PIN:0) | uid.outputMask; // | (UI_DISPLAY_RW_PIN);
#if UI_DISPLAY_D4_PIN==1 && UI_DISPLAY_D5_PIN==2 && UI_DISPLAY_D6_PIN==4 && UI_DISPLAY_D7_PIN==8
    uint8_t value = (c >> 4) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
    value = (c & 15) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#else
    uint8_t value = (c & 16?UI_DISPLAY_D4_PIN:0)|(c & 32?UI_DISPLAY_D5_PIN:0)|(c & 64?UI_DISPLAY_D6_PIN:0)|(c & 128?UI_DISPLAY_D7_PIN:0) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
    value = (c & 1?UI_DISPLAY_D4_PIN:0)|(c & 2?UI_DISPLAY_D5_PIN:0)|(c & 4?UI_DISPLAY_D6_PIN:0)|(c & 8?UI_DISPLAY_D7_PIN:0) | mod;
    HAL::i2cWrite((value) | UI_DISPLAY_ENABLE_PIN);
    HAL::i2cWrite(value);
#endif
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==1
    unsigned int mod = (rs?UI_DISPLAY_RS_PIN:0) | uid.outputMask; // | (UI_DISPLAY_RW_PIN);
    unsigned int value = (c & 16?UI_DISPLAY_D4_PIN:0)|(c & 32?UI_DISPLAY_D5_PIN:0)|(c & 64?UI_DISPLAY_D6_PIN:0)|(c & 128?UI_DISPLAY_D7_PIN:0) | mod;
    unsigned int value2 = (value) | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(value2 & 255);
    HAL::i2cWrite(value2 >>8);
    HAL::i2cWrite(value & 255);
    HAL::i2cWrite(value>>8);
    value = (c & 1?UI_DISPLAY_D4_PIN:0)|(c & 2?UI_DISPLAY_D5_PIN:0)|(c & 4?UI_DISPLAY_D6_PIN:0)|(c & 8?UI_DISPLAY_D7_PIN:0) | mod;
    value2 = (value) | UI_DISPLAY_ENABLE_PIN;
    HAL::i2cWrite(value2 & 255);
    HAL::i2cWrite(value2 >>8);
    HAL::i2cWrite(value & 255);
    HAL::i2cWrite(value>>8);
#endif
}
void initializeLCD()
{
    HAL::delayMilliseconds(235);
    lcdStartWrite();
    HAL::i2cWrite(uid.outputMask & 255);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    HAL::i2cWrite(uid.outputMask >> 8);
#endif
    HAL::delayMicroseconds(20);
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(6000); // I have one LCD for which 4500 here was not long enough.
    // second try
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(180); // wait
    // third go!
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(180);
    // finally, set to 4-bit interface
    lcdWriteNibble(0x02);
    HAL::delayMicroseconds(180);
    // finally, set # lines, font size, etc.
    lcdCommand(LCD_4BIT | LCD_2LINE | LCD_5X7);
    lcdCommand(LCD_CLEAR);					//-	Clear Screen
    HAL::delayMilliseconds(4); // clear is slow operation
    lcdCommand(LCD_INCREASE | LCD_DISPLAYSHIFTOFF);	//-	Entrymode (Display Shift: off, Increment Address Counter)
    lcdCommand(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKINGOFF);	//-	Display on
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
    uid.createChar(5,character_temperature);
    uid.createChar(6,character_folder);
    uid.createChar(7,character_ready);
    lcdStopWrite();
}
#endif
//Davinci Specific, if not winstar screen it is standard Text LCD
#if !WINSTAR_SCREEN
#if UI_DISPLAY_TYPE == DISPLAY_4BIT || UI_DISPLAY_TYPE == DISPLAY_8BIT

void lcdWriteNibble(uint8_t value)
{
    WRITE(UI_DISPLAY_D4_PIN,value & 1);
    WRITE(UI_DISPLAY_D5_PIN,value & 2);
    WRITE(UI_DISPLAY_D6_PIN,value & 4);
    WRITE(UI_DISPLAY_D7_PIN,value & 8);
    DELAY1MICROSECOND;
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);// enable pulse must be >450ns
    HAL::delayMicroseconds(2);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(UI_DELAYPERCHAR);
}

void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if false && UI_DISPLAY_RW_PIN >= 0 // not really needed
    SET_INPUT(UI_DISPLAY_D4_PIN);
    SET_INPUT(UI_DISPLAY_D5_PIN);
    SET_INPUT(UI_DISPLAY_D6_PIN);
    SET_INPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, HIGH);
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    uint8_t busy;
    do
    {
        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
        DELAY1MICROSECOND;
        busy = READ(UI_DISPLAY_D7_PIN);
        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
        DELAY2MICROSECOND;

        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
        DELAY2MICROSECOND;

        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
        DELAY2MICROSECOND;

    }
    while (busy);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, LOW);
#endif
    WRITE(UI_DISPLAY_RS_PIN, rs);

    WRITE(UI_DISPLAY_D4_PIN, c & 0x10);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x20);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x40);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x80);
    HAL::delayMicroseconds(2);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
    HAL::delayMicroseconds(2);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);

    WRITE(UI_DISPLAY_D4_PIN, c & 0x01);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x02);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x04);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x08);
    HAL::delayMicroseconds(2);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
    HAL::delayMicroseconds(2);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(100);
}

void initializeLCD()
{
    // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    // according to datasheet, we need at least 40ms after power rises above 2.7V
    // before sending commands. Arduino can turn on way before 4.5V.
    // is this delay long enough for all cases??
    HAL::delayMilliseconds(235);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    SET_OUTPUT(UI_DISPLAY_RS_PIN);
#if UI_DISPLAY_RW_PIN > -1
    SET_OUTPUT(UI_DISPLAY_RW_PIN);
#endif
    SET_OUTPUT(UI_DISPLAY_ENABLE_PIN);

    // Now we pull both RS and R/W low to begin commands
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);

    //put the LCD into 4 bit mode
    // this is according to the hitachi HD44780 datasheet
    // figure 24, pg 46

    // we start in 8bit mode, try to set 4 bit mode
    // at this point we are in 8 bit mode but of course in this
    // interface 4 pins are dangling unconnected and the values
    // on them don't matter for these instructions.
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    HAL::delayMicroseconds(20);
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(5000); // I have one LCD for which 4500 here was not long enough.
    // second try
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(5000); // wait
    // third go!
    lcdWriteNibble(0x03);
    HAL::delayMicroseconds(160);
    // finally, set to 4-bit interface
    lcdWriteNibble(0x02);
    HAL::delayMicroseconds(160);
    // finally, set # lines, font size, etc.
    lcdCommand(LCD_4BIT | LCD_2LINE | LCD_5X7);

    lcdCommand(LCD_CLEAR);					//-	Clear Screen
    HAL::delayMilliseconds(3); // clear is slow operation
    lcdCommand(LCD_INCREASE | LCD_DISPLAYSHIFTOFF);	//-	Entrymode (Display Shift: off, Increment Address Counter)
    lcdCommand(LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKINGOFF);	//-	Display on
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1, character_back);
    uid.createChar(2, character_degree);
    uid.createChar(3, character_selected);
    uid.createChar(4, character_unselected);
    uid.createChar(5, character_temperature);
    uid.createChar(6, character_folder);
    uid.createChar(7, character_ready);
}
// ----------- end direct LCD driver
#endif
#else //Davinci Specific, Special LCD from Winstar 16X4
#if UI_DISPLAY_TYPE == DISPLAY_4BIT || UI_DISPLAY_TYPE == DISPLAY_8BIT
//Davinci Specific, separate 8bts form 4 bits initialization
#if  UI_DISPLAY_TYPE == DISPLAY_4BIT

#define BIT_INTERFACE LCD_4BIT

void lcdWriteNibble(uint8_t value)
{
    WRITE(UI_DISPLAY_D4_PIN,value & 1);
    WRITE(UI_DISPLAY_D5_PIN,value & 2);
    WRITE(UI_DISPLAY_D6_PIN,value & 4);
    WRITE(UI_DISPLAY_D7_PIN,value & 8);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);// enable pulse must be >450ns
    HAL::delayMicroseconds(5);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
   HAL::delayMicroseconds(15); //tc must be >1200ns

}
void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if UI_DISPLAY_RW_PIN<0
    HAL::delayMicroseconds(UI_DELAYPERCHAR);
#else
    SET_INPUT(UI_DISPLAY_D4_PIN);
    SET_INPUT(UI_DISPLAY_D5_PIN);
    SET_INPUT(UI_DISPLAY_D6_PIN);
    SET_INPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, HIGH);
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    uint8_t busy;
    do
    {
        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
        DELAY1MICROSECOND;
        busy = READ(UI_DISPLAY_D7_PIN);
        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
        HAL::delayMicroseconds(5);

        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
        HAL::delayMicroseconds(5);

        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
        HAL::delayMicroseconds(5);

    }
    while (busy);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, LOW);
#endif
    WRITE(UI_DISPLAY_RS_PIN, rs);
    HAL::delayMicroseconds(5);
    WRITE(UI_DISPLAY_D4_PIN, c & 0x10);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x20);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x40);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x80);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
    HAL::delayMicroseconds(5);

    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(5);//tc must be >1200ns

    WRITE(UI_DISPLAY_D4_PIN, c & 0x01);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x02);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x04);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x08);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
    HAL::delayMicroseconds(5);

    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(5);//tc must be >1200ns
}
#else  //DISPLAY_8BIT

#define BIT_INTERFACE LCD_8BIT

void lcdWriteByte(uint8_t c,uint8_t rs)
{
#if UI_DISPLAY_RW_PIN<0
    HAL::delayMicroseconds(UI_DELAYPERCHAR);
#else
    SET_INPUT(UI_DISPLAY_D4_PIN);
    SET_INPUT(UI_DISPLAY_D5_PIN);
    SET_INPUT(UI_DISPLAY_D6_PIN);
    SET_INPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, HIGH);
    WRITE(UI_DISPLAY_RS_PIN, LOW);
    uint8_t busy;
    do
    {
        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
        DELAY1MICROSECOND;
        busy = READ(UI_DISPLAY_D7_PIN);
        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(5);

        WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);
    HAL::delayMicroseconds(5);

        WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMicroseconds(5);

    }
    while (busy);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    WRITE(UI_DISPLAY_RW_PIN, LOW);
#endif
    WRITE(UI_DISPLAY_RS_PIN, rs);
    HAL::delayMicroseconds(5);
    WRITE(UI_DISPLAY_D0_PIN, c & 0x01);
    WRITE(UI_DISPLAY_D1_PIN, c & 0x02);
    WRITE(UI_DISPLAY_D2_PIN, c & 0x04);
    WRITE(UI_DISPLAY_D3_PIN, c & 0x08);
    WRITE(UI_DISPLAY_D4_PIN, c & 0x10);
    WRITE(UI_DISPLAY_D5_PIN, c & 0x20);
    WRITE(UI_DISPLAY_D6_PIN, c & 0x40);
    WRITE(UI_DISPLAY_D7_PIN, c & 0x80);
    WRITE(UI_DISPLAY_ENABLE_PIN, HIGH);   // enable pulse must be >450ns
    HAL::delayMicroseconds(5);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);   //tc must be >1200ns
    HAL::delayMicroseconds(5);
}

#endif 

void initializeLCD()
{
    playsound(5000,240);
    playsound(3000,120);
#if  UI_DISPLAY_TYPE == DISPLAY_8BIT
    playsound(5000,120);
    SET_OUTPUT(UI_DISPLAY_D0_PIN);
    SET_OUTPUT(UI_DISPLAY_D1_PIN);
    SET_OUTPUT(UI_DISPLAY_D2_PIN);
    SET_OUTPUT(UI_DISPLAY_D3_PIN);
#endif
  // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
    // according to HD44780 datasheet, we need at least 40ms after power rises above 2.7V
    // before sending commands. Arduino can turn on way before 4.5V.
    // is this delay long enough for all cases??
    HAL::delayMilliseconds(500);
    SET_OUTPUT(UI_DISPLAY_D4_PIN);
    SET_OUTPUT(UI_DISPLAY_D5_PIN);
    SET_OUTPUT(UI_DISPLAY_D6_PIN);
    SET_OUTPUT(UI_DISPLAY_D7_PIN);
    SET_OUTPUT(UI_DISPLAY_RS_PIN);
#if UI_DISPLAY_RW_PIN_NOT_USED  > -1
    SET_OUTPUT(UI_DISPLAY_RW_PIN_NOT_USED); //RW pin but we do not use it
 #endif 
    //init pins to a known state
    WRITE(UI_DISPLAY_D0_PIN, HIGH);
    WRITE(UI_DISPLAY_D1_PIN, HIGH);
    WRITE(UI_DISPLAY_D2_PIN, HIGH);
    WRITE(UI_DISPLAY_D3_PIN, HIGH);
    WRITE(UI_DISPLAY_D4_PIN, HIGH);
    WRITE(UI_DISPLAY_D5_PIN, HIGH);
    WRITE(UI_DISPLAY_D6_PIN, HIGH);
    WRITE(UI_DISPLAY_D7_PIN, HIGH);
    
#if UI_DISPLAY_RW_PIN>-1
    SET_OUTPUT(UI_DISPLAY_RW_PIN);
#endif
    // Now we pull both RS and R/W low to begin commands
    WRITE(UI_DISPLAY_RS_PIN, LOW);
#if UI_DISPLAY_RW_PIN  > -1
    WRITE(UI_DISPLAY_RW_PIN, LOW);
 #endif 
#if UI_DISPLAY_RW_PIN_NOT_USED  > -1
    WRITE(UI_DISPLAY_RW_PIN_NOT_USED, LOW);//RW pin but we do not use it just set a state
 #endif 
    HAL::delayMicroseconds(5);
    //move set output late to be sure pin is not affected before
    SET_OUTPUT(UI_DISPLAY_ENABLE_PIN);
    HAL::delayMicroseconds(5);
    WRITE(UI_DISPLAY_ENABLE_PIN, LOW);
    HAL::delayMilliseconds(10); // Just to be safe
    //initialization sequence for 4bits/8bits of Winstar 1604A Screen
    //16 rows, 4 lines
#if  UI_DISPLAY_TYPE == DISPLAY_4BIT
    lcdWriteNibble(0x03);//Init Function Set for 4bits
    HAL::delayMicroseconds(150); //more than 39micro seconds
#endif
    
    lcdCommand( BIT_INTERFACE | LCD_2LINE | LCD_5X8); //LCD Configuration: Bits, Lines and Font
    HAL::delayMicroseconds(150); //more than 39micro seconds
    
    lcdCommand(BIT_INTERFACE | LCD_2LINE | LCD_5X8);//LCD Configuration: Bits, Lines and Font
    HAL::delayMicroseconds(150); //more than 39micro seconds
    
    //specification says 2 is enough but a 3rd one solve issue if restart with keypad buttons pressed 
    lcdCommand(BIT_INTERFACE | LCD_2LINE | LCD_5X8);//LCD Configuration: Bits, Lines and Font
    HAL::delayMicroseconds(150); //more than 39micro seconds
    
    lcdCommand( LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKINGOFF);    //Display Control : Display on/off, Cursor, Blinking Cursor
    HAL::delayMicroseconds(150);
    
    lcdCommand(LCD_CLEAR);                  //Clear Screen
    HAL::delayMilliseconds(8); // clear is slow operation more than 1.53ms
    
    lcdCommand(LCD_INCREASE | LCD_DISPLAYSHIFTOFF); //Entrymode: Sets cursor move direction (I/D); specifies to shift the display
    HAL::delayMicroseconds(150);

    HAL::delayMilliseconds(10);//no recommendation so just a feeling

    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
    uid.createChar(5,character_temperature);
    uid.createChar(6,character_folder);
    //uid.createChar(7,character_ready);
    uid.createChar(7,character_bed);

#if defined(UI_BACKLIGHT_PIN)
    SET_OUTPUT(UI_BACKLIGHT_PIN);
    WRITE(UI_BACKLIGHT_PIN, HIGH);
#endif
}
// ----------- end direct LCD driver
#endif
#endif //DAVINCI LCD or Not

#if UI_DISPLAY_TYPE < DISPLAY_ARDUINO_LIB
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col=0;
// Set row
    if(r >= UI_ROWS) return;
#if UI_DISPLAY_TYPE == DISPLAY_I2C
    lcdStartWrite();
#endif
    lcdWriteByte(128 + HAL::readFlashByte((const char *)&LCDLineOffsets[r]), 0); // Position cursor
    char c;
    while((c = *txt) != 0x00 && col < changeAtCol)
    {
        txt++;
        lcdPutChar(c);
        col++;
    }
    while(col < changeAtCol)
    {
        lcdPutChar(' ');
        col++;
    }
    if(txt2 != NULL)
    {
        while((c = *txt2) != 0x00 && col < UI_COLS)
        {
            txt2++;
            lcdPutChar(c);
            col++;
        }
        while(col < UI_COLS)
        {
            lcdPutChar(' ');
            col++;
        }
    }
#if UI_DISPLAY_TYPE == DISPLAY_I2C
    lcdStopWrite();
#endif
#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    uiCheckSlowEncoder();
#endif
}
#endif

#if UI_DISPLAY_TYPE == DISPLAY_ARDUINO_LIB
// Use LiquidCrystal library instead
#include <LiquidCrystal.h>

LiquidCrystal lcd(UI_DISPLAY_RS_PIN, UI_DISPLAY_RW_PIN,UI_DISPLAY_ENABLE_PIN,UI_DISPLAY_D4_PIN,UI_DISPLAY_D5_PIN,UI_DISPLAY_D6_PIN,UI_DISPLAY_D7_PIN);

void UIDisplay::createChar(uint8_t location,const uint8_t charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    uint8_t data[8];
    for (int i = 0; i < 8; i++)
    {
        data[i] = pgm_read_byte(&(charmap[i]));
    }
    lcd.createChar(location, data);
}
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col = 0;
// Set row
    if(r >= UI_ROWS) return;
    lcd.setCursor(0,r);
    char c;
    while((c = *txt) != 0x00 && col < changeAtCol)
    {
        txt++;
        lcd.write(c);
        col++;
    }
    while(col < changeAtCol)
    {
        lcd.write(' ');
        col++;
    }
    if(txt2 != NULL)
    {
        while((c = *txt2) != 0x00 && col < UI_COLS)
        {
            txt2++;
            lcd.write(c);
            col++;
        }
        while(col < UI_COLS)
        {
            lcd.write(' ');
            col++;
        }
    }
#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    uiCheckSlowEncoder();
#endif
}

void initializeLCD()
{
    lcd.begin(UI_COLS,UI_ROWS);
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
    uid.createChar(1,character_back);
    uid.createChar(2,character_degree);
    uid.createChar(3,character_selected);
    uid.createChar(4,character_unselected);
}
// ------------------ End LiquidCrystal library as LCD driver
#endif // UI_DISPLAY_TYPE == DISPLAY_ARDUINO_LIB

#if UI_DISPLAY_TYPE == DISPLAY_U8G
//u8glib
#ifdef U8GLIB_ST7920
#define UI_SPI_SCK UI_DISPLAY_D4_PIN
#define UI_SPI_MOSI UI_DISPLAY_ENABLE_PIN
#define UI_SPI_CS UI_DISPLAY_RS_PIN
#endif
#include "u8glib_ex.h"
#include "logo.h"

u8g_t u8g;
u8g_uint_t u8_tx = 0, u8_ty = 0;

void u8PrintChar(char c)
{
    switch(c)
    {
    case 0x7E: // right arrow
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0x52);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    case CHAR_SELECTOR:
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0xb7);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    case CHAR_SELECTED:
        u8g_SetFont(&u8g, u8g_font_6x12_67_75);
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, 0xb6);
        u8g_SetFont(&u8g, UI_FONT_DEFAULT);
        break;
    case 253:      //shift one pixel to right
        u8_tx++;
        break;
    default:
        u8_tx += u8g_DrawGlyph(&u8g, u8_tx, u8_ty, c);
    }
}
void printU8GRow(uint8_t x,uint8_t y,char *text)
{
    char c;
    u8_tx = x;
    u8_ty = y;
    while((c = *(text++)) != 0) u8PrintChar(c);  //version compatible with position adjust
//        x += u8g_DrawGlyph(&u8g,x,y,c);
}
void UIDisplay::printRow(uint8_t r,char *txt,char *txt2,uint8_t changeAtCol)
{
    changeAtCol = RMath::min(UI_COLS,changeAtCol);
    uint8_t col = 0;
// Set row
    if(r >= UI_ROWS) return;
    int y = r * UI_FONT_HEIGHT;
    if(!u8g_IsBBXIntersection(&u8g,0,y,UI_LCD_WIDTH,UI_FONT_HEIGHT+2)) return; // row not visible
    u8_tx = 0;
    u8_ty = y+UI_FONT_HEIGHT; //set position
    bool highlight = ((uint8_t)(*txt) == CHAR_SELECTOR) || ((uint8_t)(*txt) == CHAR_SELECTED);
    if(highlight)
    {
        u8g_SetColorIndex(&u8g,1);
        u8g_draw_box(&u8g, 0, y + 1, u8g_GetWidth(&u8g), UI_FONT_HEIGHT + 1);
        u8g_SetColorIndex(&u8g, 0);
    }
    char c;
    while((c = *(txt++)) != 0 && col < changeAtCol)
    {
        u8PrintChar(c);
        col++;
    }
    if(txt2 != NULL)
    {
        col = changeAtCol;
        u8_tx = col*UI_FONT_WIDTH; //set position
        while((c=*(txt2++)) != 0 && col < UI_COLS)
        {
            u8PrintChar(c);
            col++;
        }
    }
    if(highlight)
    {
        u8g_SetColorIndex(&u8g,1);
    }

#if UI_HAS_KEYS==1 && UI_HAS_I2C_ENCODER>0
    uiCheckSlowEncoder();
#endif
}

void initializeLCD()
{
#ifdef U8GLIB_ST7920
    u8g_InitSPI(&u8g,&u8g_dev_st7920_128x64_sw_spi,  UI_DISPLAY_D4_PIN, UI_DISPLAY_ENABLE_PIN, UI_DISPLAY_RS_PIN, U8G_PIN_NONE, U8G_PIN_NONE);
#endif
    u8g_Begin(&u8g);
    u8g_FirstPage(&u8g);
    do
    {
        u8g_SetColorIndex(&u8g, 0);
    }
    while( u8g_NextPage(&u8g) );

    u8g_SetFont(&u8g, UI_FONT_DEFAULT);
    u8g_SetColorIndex(&u8g, 1);
    uid.lastSwitch = uid.lastRefresh = HAL::timeInMilliseconds();
}
// ------------------ End u8GLIB library as LCD driver
#endif // UI_DISPLAY_TYPE == DISPLAY_U8G

#if UI_DISPLAY_TYPE == DISPLAY_GAMEDUINO2
#include "gameduino2.h"
#endif

UIDisplay::UIDisplay()
{
}
#if UI_ANIMATION
void slideIn(uint8_t row,FSTRINGPARAM(text))
{
    char *empty="";
    int8_t i = 0;
    uid.col=0;
    uid.addStringP(text);
    uid.printCols[uid.col]=0;
    for(i=UI_COLS-1; i>=0; i--)
    {
        uid.printRow(row,empty,uid.printCols,i);
        HAL::pingWatchdog();
        HAL::delayMilliseconds(10);
    }
}
#endif // UI_ANIMATION
void UIDisplay::initialize()
{
    oldMenuLevel = -2;
#ifdef COMPILE_I2C_DRIVER
    uid.outputMask = UI_DISPLAY_I2C_OUTPUT_START_MASK;
#if UI_DISPLAY_I2C_CHIPTYPE==0 && BEEPER_TYPE==2 && BEEPER_PIN>=0
#if BEEPER_ADDRESS == UI_DISPLAY_I2C_ADDRESS
    uid.outputMask |= BEEPER_PIN;
#endif
#endif
    HAL::i2cInit(UI_I2C_CLOCKSPEED);
#if UI_DISPLAY_I2C_CHIPTYPE==1
    // set direction of pins
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS + I2C_WRITE);
    HAL::i2cWrite(0); // IODIRA
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS & 255));
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS >> 8));
    HAL::i2cStop();
    // Set pullups according to  UI_DISPLAY_I2C_PULLUP
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0x0C); // GPPUA
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP & 255);
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP >> 8);
    HAL::i2cStop();
#endif

#endif
    flags = 0;
    menuLevel = 0;
    shift = -2;
    menuPos[0] = 0;
    lastAction = 0;
    delayedAction = 0;
    lastButtonAction = 0;
    activeAction = 0;
    statusMsg[0] = 0;
    uiInitKeys();
    cwd[0] = '/';
    cwd[1] = 0;
    folderLevel = 0;
    UI_STATUS(UI_TEXT_PRINTER_READY);
#if UI_DISPLAY_TYPE != NO_DISPLAY
    initializeLCD();
#if defined(USER_KEY1_PIN) && USER_KEY1_PIN > -1
    UI_KEYS_INIT_BUTTON_LOW(USER_KEY1_PIN);
#endif
#if defined(USER_KEY2_PIN) && USER_KEY2_PIN > -1
    UI_KEYS_INIT_BUTTON_LOW(USER_KEY2_PIN);
#endif
#if defined(USER_KEY3_PIN) && USER_KEY3_PIN > -1
    UI_KEYS_INIT_BUTTON_LOW(USER_KEY3_PIN);
#endif
#if defined(USER_KEY4_PIN) && USER_KEY4_PIN > -1
    UI_KEYS_INIT_BUTTON_LOW(USER_KEY4_PIN);
#endif
#if UI_DISPLAY_TYPE == DISPLAY_I2C
    // I don't know why but after power up the lcd does not come up
    // but if I reinitialize i2c and the lcd again here it works.
    HAL::delayMilliseconds(10);
    HAL::i2cInit(UI_I2C_CLOCKSPEED);
    // set direction of pins
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0); // IODIRA
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS & 255));
    HAL::i2cWrite(~(UI_DISPLAY_I2C_OUTPUT_PINS >> 8));
    HAL::i2cStop();
    // Set pullups according to  UI_DISPLAY_I2C_PULLUP
    HAL::i2cStart(UI_DISPLAY_I2C_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(0x0C); // GPPUA
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP & 255);
    HAL::i2cWrite(UI_DISPLAY_I2C_PULLUP >> 8);
    HAL::i2cStop();
    initializeLCD();
#endif
#if UI_DISPLAY_TYPE == DISPLAY_GAMEDUINO2
    GD2::startScreen();
#else
#if UI_ANIMATION==false || UI_DISPLAY_TYPE == DISPLAY_U8G
#if UI_DISPLAY_TYPE == DISPLAY_U8G
    //u8g picture loop
    u8g_FirstPage(&u8g);
    do
    {
        u8g_DrawBitmapP(&u8g, 128 - LOGO_WIDTH, 0, ((LOGO_WIDTH + 8) / 8), LOGO_HEIGHT, logo);
        for(uint8_t y = 0; y < UI_ROWS; y++) displayCache[y][0] = 0;
        printRowP(0, PSTR("Repetier"));
        printRowP(1, PSTR("Ver " REPETIER_VERSION));
        printRowP(3, PSTR("Machine:"));
        printRowP(4, PSTR(UI_PRINTER_NAME));
        printRowP(5, PSTR(UI_PRINTER_COMPANY));
    }
    while( u8g_NextPage(&u8g) );  //end picture loop
#else // not DISPLAY_U8G
    for(uint8_t y=0; y<UI_ROWS; y++) displayCache[y][0] = 0;
     //Davinci Specific, line 1 : DaVinci X.0, Line 3: Repetier Base 
    printRowP(0, PSTR(UI_PRINTER_NAME) );
#if UI_ROWS>2
    printRowP(UI_ROWS-2, versionString);
#endif
#endif
#else
    slideIn(0, versionString);
    strcpy(displayCache[0], uid.printCols);
    slideIn(1, PSTR(UI_PRINTER_NAME));
    strcpy(displayCache[1], uid.printCols);
#if UI_ROWS>2
    slideIn(UI_ROWS-1, PSTR(UI_PRINTER_COMPANY));
    strcpy(displayCache[UI_ROWS-1], uid.printCols);
#endif
#endif
#endif // gameduino2
    HAL::delayMilliseconds(UI_START_SCREEN_DELAY);
#endif
#if UI_DISPLAY_I2C_CHIPTYPE==0 && (BEEPER_TYPE==2 || defined(UI_HAS_I2C_KEYS))
    // Make sure the beeper is off
    HAL::i2cStartWait(UI_I2C_KEY_ADDRESS+I2C_WRITE);
    HAL::i2cWrite(255); // Disable beeper, enable read for other pins.
    HAL::i2cStop();
#endif
}
#if UI_DISPLAY_TYPE == DISPLAY_4BIT || UI_DISPLAY_TYPE == DISPLAY_8BIT || UI_DISPLAY_TYPE == DISPLAY_I2C
void UIDisplay::createChar(uint8_t location,const uint8_t PROGMEM charmap[])
{
    location &= 0x7; // we only have 8 locations 0-7
    lcdCommand(LCD_SETCGRAMADDR | (location << 3));
    for (int i=0; i<8; i++)
    {
        lcdPutChar(pgm_read_byte(&(charmap[i])));
    }
}
#endif
void  UIDisplay::waitForKey()
{
    int nextAction = 0;

    lastButtonAction = 0;
    while(lastButtonAction==nextAction)
    {
        uiCheckSlowKeys(nextAction);
    }
}

void UIDisplay::printRowP(uint8_t r,PGM_P txt)
{
    if(r >= UI_ROWS) return;
    col = 0;
    addStringP(txt);
    uid.printCols[col] = 0;
    printRow(r,uid.printCols,NULL,UI_COLS);
}
void UIDisplay::addInt(int value,uint8_t digits,char fillChar)
{
    uint8_t dig = 0, neg = 0;
    if(value < 0)
    {
        value = -value;
        neg = 1;
        dig++;
    }
    char buf[7]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[6];
    buf[6] = 0;
    do
    {
        unsigned int m = value;
        value /= 10;
        char c = m - 10 * value;
        *--str = c + '0';
        dig++;
    }
    while(value);
    if(neg)
        uid.printCols[col++] = '-';
    if(digits < 6)
        while(dig < digits)
        {
            *--str = fillChar; //' ';
            dig++;
        }
    while(*str && col < MAX_COLS)
    {
        uid.printCols[col++] = *str;
        str++;
    }
}
void UIDisplay::addLong(long value,char digits)
{
    uint8_t dig = 0,neg = 0;
    byte addspaces = digits > 0;
    if (digits < 0) digits = -digits;
    if(value < 0)
    {
        neg = 1;
        value = -value;
        dig++;
    }
    char buf[13]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[12];
    buf[12] = 0;
    do
    {
        unsigned long m = value;
        value /= 10;
        char c = m - 10 * value;
        *--str = c + '0';
        dig++;
    }
    while(value);
    if(neg)
        uid.printCols[col++] = '-';
    if(addspaces && digits <= 11)
        while(dig < digits)
        {
            *--str = ' ';
            dig++;
        }
    while(*str && col<MAX_COLS)
    {
        uid.printCols[col++] = *str;
        str++;
    }
}

const float roundingTable[] PROGMEM = {0.5, 0.05, 0.005, 0.0005};

UI_STRING(ui_text_on,UI_TEXT_ON);
UI_STRING(ui_text_off,UI_TEXT_OFF);
UI_STRING(ui_text_na,UI_TEXT_NA);
UI_STRING(ui_yes,UI_TEXT_YES);
UI_STRING(ui_no,UI_TEXT_NO);
UI_STRING(ui_print_pos,UI_TEXT_PRINT_POS);
UI_STRING(ui_selected,UI_TEXT_SEL);
UI_STRING(ui_unselected,UI_TEXT_NOSEL);
UI_STRING(ui_action,UI_TEXT_STRING_ACTION);

void UIDisplay::addFloat(float number, char fixdigits, uint8_t digits)
{
    // Handle negative numbers
    if (number < 0.0)
    {
        uid.printCols[col++] = '-';
        if(col >= MAX_COLS) return;
        number = -number;
        fixdigits--;
    }
    number += pgm_read_float(&roundingTable[digits]); // for correct rounding

    // Extract the integer part of the number and print it
    unsigned long int_part = (unsigned long)number;
    float remainder = number - (float)int_part;
    addLong(int_part,fixdigits);
    if(col >= UI_COLS) return;

    // Print the decimal point, but only if there are digits beyond
    if (digits > 0)
    {
        uid.printCols[col++] = '.';
    }

    // Extract digits from the remainder one at a time
    while (col < MAX_COLS && digits-- > 0)
    {
        remainder *= 10.0;
        uint8_t toPrint = uint8_t(remainder);
        uid.printCols[col++] = '0' + toPrint;
        remainder -= toPrint;
    }
}

void UIDisplay::addStringP(FSTRINGPARAM(text))
{
    while(col < MAX_COLS)
    {
        uint8_t c = HAL::readFlashByte(text++);
        if(c == 0) return;
        uid.printCols[col++] = c;
    }
}

void UIDisplay::addStringOnOff(uint8_t on)
{
    addStringP(on ? ui_text_on : ui_text_off);
}

void UIDisplay::addChar(const char c)
{
    if(col < UI_COLS)
    {
        uid.printCols[col++] = c;
    }
}
void UIDisplay::addGCode(GCode *code)
{
    // assume volatile and make copy so we dont "see" multple code lines as we go.
    //GCode myCode = *code; insuffeicnet memory for this safety check
    //code = &myCode;
    addChar('#');
    addLong(code->N);
    if(code->hasM())
    {
        addChar('M');
        addLong((long)code->M);
    }
    if(code->hasG())
    {
        addChar('G');
        addLong((long)code->G);
    }
    if(code->hasT())
    {
        addChar('T');
        addLong((long)code->T);
    }
    if(code->hasX())
    {
        addChar('X');
        addFloat(code->X);
    }
    if(code->hasY())
    {
        addChar('Y');
        addFloat(code->Y);
    }
    if(code->hasZ())
    {
        addChar('Z');
        addFloat(code->Z);
    }
    if(code->hasE())
    {
        addChar('E');
        addFloat(code->E);
    }
    if(code->hasF())
    {
        addChar('F');
        addFloat(code->F);
    }
    if(code->hasS())
    {
        addChar('S');
        addLong(code->S);
    }
    if(code->hasP())
    {
        addChar('P');
        addLong(code->P);
    }
#ifdef ARC_SUPPORT
    if(code->hasI())
    {
        addChar('I');
        addFloat(code->I);
    }
    if(code->hasJ())
    {
        addChar('J');
        addFloat(code->J);
    }
    if(code->hasR())
    {
        addChar('R');
        addFloat(code->R);
    }
#endif
    // cannot print string, it isnt part of the gcode structure.
    //it points to temp memory in a buffer.
    //if(code->hasSTRING())
}


void UIDisplay::parse(const char *txt,bool ram)
{
    static uint8_t beepdelay = 0;
    int ivalue = 0;
    float fvalue = 0;
    while(col < MAX_COLS)
    {
        char c = (ram ? *(txt++) : pgm_read_byte(txt++));
        if(c == 0) break; // finished
        if(c != '%')
        {
            uid.printCols[col++] = c;
            continue;
        }
        // dynamic parameter, parse meaning and replace
        char c1 = (ram ? *(txt++) : pgm_read_byte(txt++));
        char c2 = (ram ? *(txt++) : pgm_read_byte(txt++));
        switch(c1)
        {
        case '%':
        {
            // print % for input '%%' or '%%%'
            if(col < UI_COLS) uid.printCols[col++] = '%'; // if data = '%%?' escaped percent, with left over ? char
            if (c2 != '%') txt--; // Be flexible and accept 2 or 3 chars
            break;
        } // case '%'

        case '?' : // conditional spacer or other char
        {
            // If something has been printed, check if the last char is c2.
            // if not, append c2.
            // otherwise do nothing.
            if (col > 0 && col < UI_COLS)
            {
                if (uid.printCols[col - 1] != c2) uid.printCols[col++] = c2;
            }
            break;
        }
        case 'a': // Acceleration settings
            if(c2 >= 'x' && c2 <= 'z')       addFloat(Printer::maxAccelerationMMPerSquareSecond[c2 - 'x'], 5, 0);
            else if(c2 >= 'X' &&  c2 <= 'Z') addFloat(Printer::maxTravelAccelerationMMPerSquareSecond[c2-'X'], 5, 0);
            else if(c2 == 'j') addFloat(Printer::maxJerk, 3, 1);
#if DRIVE_SYSTEM!=DELTA
            else if(c2 == 'J') addFloat(Printer::maxZJerk, 3, 1);
#endif
            break;
//Davinci Specific,
        case 'B':
        if(c2=='1') //heat PLA
                {
                        bool allheat=true;
                        if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                       #if NUM_EXTRUDER>1
                        if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                        #endif
                       #if NUM_EXTRUDER>2
                        if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
                        #endif
                        #if HAVE_HEATED_BED==true
                        if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_pla)allheat=false;
                        #endif
                        addStringP(allheat?"\003":"\004");
                }
        else if(c2=='2') //heat ABS
                {
                    bool allheat=true;
                    if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                   #if NUM_EXTRUDER>1
                    if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                    #endif
                   #if NUM_EXTRUDER>2
                    if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
                    #endif
                    #if HAVE_HEATED_BED==true
                    if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_abs)allheat=false;
                    #endif
                    addStringP(allheat?"\003":"\004");
                }
          else if(c2=='3') //Cooldown
                {
                     bool alloff=true;
                     if(extruder[0].tempControl.targetTemperatureC>0)alloff=false;
#if NUM_EXTRUDER>1
                    if(extruder[1].tempControl.targetTemperatureC>0)alloff=false;
#endif
#if NUM_EXTRUDER>2
                     if(extruder[2].tempControl.targetTemperatureC>0)alloff=false;
#endif
#if HAVE_HEATED_BED==true
                    if (heatedBedController.targetTemperatureC>0)alloff=false;
#endif
               addStringP(alloff?"\003":"\004");
                }
            else if(c2=='4') //Extruder 1 Off
                {
                    addStringP(extruder[0].tempControl.targetTemperatureC>0?"\004":"\003");
                }
            else if(c2=='5') //Extruder 2 Off
                {
                addStringP(extruder[1].tempControl.targetTemperatureC>0?"\004":"\003");
                }
             else if(c2=='6') //Extruder 3 Off
                {
                addStringP(extruder[2].tempControl.targetTemperatureC>0?"\004":"\003");
                }
#if HAVE_HEATED_BED
             else if(c2=='7') //Bed Off
                {
                addStringP(heatedBedController.targetTemperatureC>0?"\004":"\003");
                }
#endif
        break;
	    case 'C':
            if(c2=='1')
                {
                addStringP(uipagedialog[0]);
                }
            else if(c2=='2')
                {
                addStringP(uipagedialog[1]);
                }
            else if(c2=='3')
                {
                addStringP(uipagedialog[2]);
                }
            else if(c2=='4')
                {
                addStringP(uipagedialog[3]);
                }
	    break;

        case 'd':
            if(c2 == 'o') addStringOnOff(Printer::debugEcho());
            else if(c2 == 'i') addStringOnOff(Printer::debugInfo());
            else if(c2 == 'e') addStringOnOff(Printer::debugErrors());
            else if(c2 == 'd') addStringOnOff(Printer::debugDryrun());
            break;

        case 'e': // Extruder temperature
        {
            if(c2 == 'I')
            {
                //give integer display
                char c2=(ram ? *(txt++) : pgm_read_byte(txt++));
                ivalue=0;
            }
            else ivalue = UI_TEMP_PRECISION;

            if(c2 == 'r')   // Extruder relative mode
            {
                addStringP(Printer::relativeExtruderCoordinateMode ? ui_yes : ui_no);
                break;
            }
            uint8_t eid = NUM_EXTRUDER;    // default = BED if c2 not specified extruder number
            if(c2 == 'c') eid = Extruder::current->id;
            else if(c2 >= '0' && c2 <= '9') eid = c2 - '0';
            if(Printer::isAnyTempsensorDefect())
            {
                if(eid == 0 && ++beepdelay > 30) beepdelay = 0; // beep every 30 seconds
                if(beepdelay == 1) BEEP_LONG;
                if(tempController[eid]->isSensorDefect())
                {
                    addStringP(PSTR(" def "));
                    break;
                }
//Davinci Specific, be able to disable decouple test
#if FEATURE_DECOUPLE_TEST
                else if(tempController[eid]->isSensorDecoupled())
                {
                    addStringP(PSTR(" dec "));
                    break;
                }
#endif
            }
#if EXTRUDER_JAM_CONTROL
            if(tempController[eid]->isJammed())
            {
                if(++beepdelay > 10) beepdelay = 0;  // beep every 10 seconds
                if(beepdelay == 1) BEEP_LONG;
                addStringP(PSTR(" jam "));
                break;
            }
#endif
            if(c2 == 'c') fvalue = Extruder::current->tempControl.currentTemperatureC;
            else if(c2 >= '0' && c2 <= '9') fvalue=extruder[c2 - '0'].tempControl.currentTemperatureC;
            else if(c2 == 'b') fvalue = Extruder::getHeatedBedTemperature();
            else if(c2 == 'B')
            {
                ivalue = 0;
                fvalue = Extruder::getHeatedBedTemperature();
            }
            addFloat(fvalue, 3, ivalue);
            break;
        }
        case 'E': // Target extruder temperature
            if(c2 == 'c') fvalue = Extruder::current->tempControl.targetTemperatureC;
            else if(c2 >= '0' && c2 <= '9') fvalue = extruder[c2 - '0'].tempControl.targetTemperatureC;
#if HAVE_HEATED_BED
            else if(c2 == 'b') fvalue = heatedBedController.targetTemperatureC;
#endif
            addFloat(fvalue, 3, 0 /*UI_TEMP_PRECISION*/);
            break;
#if FAN_PIN > -1 && FEATURE_FAN_CONTROL
        case 'F': // FAN speed
            if(c2 == 's') addInt(floor(Printer::getFanSpeed() * 100 / 255 + 0.5f), 3);
            if(c2=='i') addStringP((Printer::flag2 & PRINTER_FLAG2_IGNORE_M106_COMMAND) ? ui_selected : ui_unselected);
            break;
#endif
        case 'f':
            if(c2 >= 'x' && c2 <= 'z') addFloat(Printer::maxFeedrate[c2 - 'x'], 5, 0);
            else if(c2 >= 'X' && c2 <= 'Z') addFloat(Printer::homingFeedrate[c2 - 'X'], 5, 0);
            break;
//Davinci Specific, XYZ Min position
        case 'H':
                if(c2=='x') addFloat(Printer::xMin,4,2);
                else if(c2=='y') addFloat(Printer::yMin,4,2);
                else if(c2=='z') addFloat(Printer::zMin,4,2);
                break;
        case 'i':
            if(c2 == 's') addInt(stepperInactiveTime / 60000, 3);
            else if(c2 == 'p') addInt(maxInactiveTime / 60000, 3);
//Davinci Specific, powersave
            else if(c2 == 'l') addLong(EEPROM::timepowersaving/60000,3);
            break;
        case 'O': // ops related stuff
            break;
//Davinci Specific, XYZ Lenght
         case 'L':
             if(c2=='x') addFloat(Printer::xLength,4,0);
             else if(c2=='y') addFloat(Printer::yLength,4,0);
             else if(c2=='z') addFloat(Printer::zLength,4,0);
         break;
        case 'l':
            if(c2 == 'a') addInt(lastAction,4);
#if defined(BADGE_LIGHT_PIN) && BADGE_LIGHT_PIN >= 0
            else if(c2 == 'b') addStringOnOff(READ(BADGE_LIGHT_PIN));        // Lights on/off
#endif
#if defined(CASE_LIGHTS_PIN) && CASE_LIGHTS_PIN >= 0
            else if(c2 == 'o') addStringOnOff(READ(CASE_LIGHTS_PIN));        // Lights on/off
//Davinci Specific, Light management
            else if(c2 == 'k') addStringOnOff(EEPROM::bkeeplighton);        // Keep Lights on/off
#endif
#if FEATURE_AUTOLEVEL
            else if(c2 == 'l') addStringOnOff((Printer::isAutolevelActive()));        // Autolevel on/off
#endif
            break;
        case 'o':
            if(c2 == 's')
            {
#if SDSUPPORT
                if(sd.sdactive && sd.sdmode)
                {
                    addStringP(PSTR( UI_TEXT_PRINT_POS));
                    float percent;
                    if(sd.filesize < 2000000) percent = sd.sdpos * 100.0 / sd.filesize;
                    else percent = (sd.sdpos >> 8) * 100.0 / (sd.filesize >> 8);
                    addFloat(percent, 3, 1);
                    if(col<MAX_COLS)
						{
                        uid.printCols[col++] = '%';
                        uid.printCols[col++] = 0;
						}
                }
                else
#endif
                    parse(statusMsg, true);
//Davinci Wifi
#if ENABLE_WIFI
					static char lastmsg[21]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
					char currentmsg[21]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
					if(HAL::bwifion)
						{
						strcpy(currentmsg,uid.printCols);
						//send only if different than previous and not empty
						if (strcmp(lastmsg,currentmsg)!=0 && strlen(statusMsg)>0)
							{
							strcpy(lastmsg,currentmsg);
							Com::print(Com::tStatus);
							Com::printFLN(currentmsg);
							}
						}
#endif
                break;
            }
            if(c2 == 'c')
            {
                addLong(baudrate, 6);
                break;
            }
            if(c2 == 'e')
            {
                if(errorMsg != 0) addStringP((char PROGMEM *)errorMsg);
                break;
            }
            if(c2 == 'B')
            {
                addInt((int)PrintLine::linesCount, 2);
                break;
            }
            if(c2 == 'f')
            {
                addInt(Printer::extrudeMultiply, 3);
                break;
            }
            if(c2 == 'm')
            {
                addInt(Printer::feedrateMultiply, 3);
                break;
            }
            if(c2 == 'n')
            {
                addInt(Extruder::current->id + 1, 1);
                break;
            }
#if FEATURE_SERVO > 0 && UI_SERVO_CONTROL > 0
            if(c2 == 'S')
            {
                addInt(servoPosition, 4);
                break;
            }
#endif
#if FEATURE_BABYSTEPPING
            if(c2 == 'Y')
            {
//                addInt(zBabySteps,0);
                addFloat((float)zBabySteps * Printer::invAxisStepsPerMM[Z_AXIS], 2, 2);
                break;
            }
#endif
            // Extruder output level
            if(c2 >= '0' && c2 <= '9') ivalue = pwm_pos[c2 - '0'];
#if HAVE_HEATED_BED
            else if(c2 == 'b') ivalue = pwm_pos[heatedBedController.pwmIndex];
#endif
            else if(c2 == 'C') ivalue = pwm_pos[Extruder::current->id];
            ivalue = (ivalue * 100) / 255;
            addInt(ivalue, 3);
            if(col < MAX_COLS)
                uid.printCols[col++] = '%';
            break;
//Davinci Specific, type of menu
        case 'M':
            if(c2=='d')
              {
                  addStringP((display_mode&ADVANCED_MODE)?UI_TEXT_ADVANCED_MODE:UI_TEXT_EASY_MODE);
              }
            break;
        case 'N':
            if(c2=='e')
              {
#if NUM_EXTRUDER>1
                   addInt(Extruder::current->id+1,1);
#endif
              }
        break;
case 'P':
            if(c2=='N') addStringP(PSTR(UI_PRINTER_NAME));
            #if UI_AUTOLIGHTOFF_AFTER > 0
            else if(c2=='s')
            if((EEPROM::timepowersaving!=maxInactiveTime)||(EEPROM::timepowersaving!=stepperInactiveTime))
                    {
                    addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off" vs "30min"
                    addStringP(ui_text_on);//if not defined by preset
                    }
            else if(EEPROM::timepowersaving==0)
                {
                addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off/on" vs "30min"
                addStringP(ui_text_off);        // powersave off
                }
            else if (EEPROM::timepowersaving==(1000 * 60)) addStringP(" 1min");//1mn
            else if (EEPROM::timepowersaving==(1000 * 60 *5)) addStringP(" 5min");//5 min
            else if (EEPROM::timepowersaving==(1000 * 60 * 15)) addStringP("15min");//15 min
            else if (EEPROM::timepowersaving==(1000 * 60 * 30)) addStringP("30min");//30 min
            else {
                    addStringP("  ");//for alignement need a better way as it should be depending of size of translation of "off" vs "30min"
                    addStringP(ui_text_on);//if not defined by preset
                    }
            #endif
            break;
        case 's': // Endstop positions
            //Davinci Specific, sound and sensor
	    #if FEATURE_BEEPER
            if(c2=='o')addStringP(HAL::enablesound?ui_text_on:ui_text_off);        // sound on/off
            #endif
            #if defined(FIL_SENSOR1_PIN)
              if(c2=='f')addStringP(EEPROM::busesensor?ui_text_on:ui_text_off);        //filament sensors on/off
            #endif
            #if defined(TOP_SENSOR_PIN)
              if(c2=='t')addStringP(EEPROM::btopsensor?ui_text_on:ui_text_off);        //top sensors on/off
            #endif
            if(c2 == 'x')
            {
#if (X_MIN_PIN > -1) && MIN_HARDWARE_ENDSTOP_X
                //addStringOnOff(Printer::isXMinEndstopHit());
                addStringP(Printer::isXMinEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            }
            if(c2 == 'X')
#if (X_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_X
                //addStringOnOff(Printer::isXMaxEndstopHit());
                addStringP(Printer::isXMaxEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            if(c2 == 'y')
#if (Y_MIN_PIN > -1)&& MIN_HARDWARE_ENDSTOP_Y
                //addStringOnOff(Printer::isYMinEndstopHit());
                addStringP(Printer::isYMinEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            if(c2 == 'Y')
#if (Y_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_Y
                //addStringOnOff(Printer::isYMaxEndstopHit());
                addStringP(Printer::isYMaxEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            if(c2 == 'z')
#if (Z_MIN_PIN > -1) && MIN_HARDWARE_ENDSTOP_Z
                //addStringOnOff(Printer::isZMinEndstopHit());
                addStringP(Printer::isZMinEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            if(c2=='Z')
#if (Z_MAX_PIN > -1) && MAX_HARDWARE_ENDSTOP_Z
                //addStringOnOff(Printer::isZMaxEndstopHit());
                addStringP(Printer::isZMaxEndstopHit()?"\003":"\004");
#else
                addStringP(ui_text_na);
#endif
            if(c2=='P')
#if (Z_PROBE_PIN > -1)
                addStringOnOff(Printer::isZProbeHit());
#else
                addStringP(ui_text_na);
#endif
            break;
        case 'S':
            if(c2 >= 'x' && c2 <= 'z') addFloat(Printer::axisStepsPerMM[c2 - 'x'], 3, 1);
            if(c2 == 'e') addFloat(Extruder::current->stepsPerMM, 3, 1);
            break;
//Davinci Specific, Temperature for Extruder/bed ABS and PLA
        case 'T':
            if(c2=='1')addFloat(EEPROM::ftemp_ext_abs,3,0 );
            else if(c2=='2')addFloat(EEPROM::ftemp_ext_pla,3,0 );
             #if HAVE_HEATED_BED==true
             else if(c2=='3')addFloat(EEPROM::ftemp_bed_abs,3,0 );
             else if(c2=='4')addFloat(EEPROM::ftemp_bed_pla,3,0 );
             #endif
        break;

        case 'U':
            if(c2 == 't')   // Printing time
            {
#if EEPROM_MODE
                bool alloff = true;
                for(uint8_t i = 0; i < NUM_EXTRUDER; i++)
                    if(tempController[i]->targetTemperatureC > 15) alloff = false;

                long seconds = (alloff ? 0 : (HAL::timeInMilliseconds() - Printer::msecondsPrinting) / 1000) + HAL::eprGetInt32(EPR_PRINTING_TIME);
                long tmp = seconds / 86400;
                seconds -= tmp * 86400;
                addInt(tmp, 5);
                addStringP(PSTR(UI_TEXT_PRINTTIME_DAYS));
                tmp = seconds / 3600;
                addInt(tmp,2);
                addStringP(PSTR(UI_TEXT_PRINTTIME_HOURS));
                seconds -= tmp * 3600;
                tmp = seconds / 60;
                addInt(tmp,2,'0');
                addStringP(PSTR(UI_TEXT_PRINTTIME_MINUTES));
#endif
            }
            else if(c2 == 'f')     // Filament usage
            {
#if EEPROM_MODE
                float dist = Printer::filamentPrinted * 0.001 + HAL::eprGetFloat(EPR_PRINTING_DISTANCE);
#else
                float dist = Printer::filamentPrinted * 0.001;
#endif
                addFloat(dist, 6, 1);
            }
            break;

#if ENABLE_WIFI
		case 'w':
			if(c2=='o')addStringP(HAL::bwifion?ui_text_on:ui_text_off);        //wifi on/off
			break;
#endif
        case 'x':
            if(c2>='0' && c2<='4')
            {
                if(c2=='4') // this sequence save 14 bytes of flash
                {
                    addFloat(Printer::filamentPrinted * 0.001,3,2);
                    break;
                }
                if(c2=='0')
                    fvalue = Printer::realXPosition();
                else if(c2=='1')
                    fvalue = Printer::realYPosition();
                else if(c2=='2')
                    fvalue = Printer::realZPosition();
                else
                    fvalue = (float)Printer::currentPositionSteps[E_AXIS] * Printer::invAxisStepsPerMM[E_AXIS];
                addFloat(fvalue,4,2);
            }
            break;

        case 'X': // Extruder related
#if NUM_EXTRUDER>0
            if(c2>='0' && c2<='9')
            {
                addStringP(Extruder::current->id==c2-'0'?ui_selected:ui_unselected);
            }
#if TEMP_PID
            else if(c2=='i')
            {
                addFloat(currHeaterForSetup->pidIGain, 4,2);
            }
            else if(c2=='p')
            {
                addFloat(currHeaterForSetup->pidPGain, 4,2);
            }
            else if(c2=='d')
            {
                addFloat(currHeaterForSetup->pidDGain, 4,2);
            }
            else if(c2=='m')
            {
                addInt(currHeaterForSetup->pidDriveMin, 3);
            }
            else if(c2=='M')
            {
                addInt(currHeaterForSetup->pidDriveMax, 3);
            }
            else if(c2=='D')
            {
                addInt(currHeaterForSetup->pidMax, 3);
            }
#endif
            else if(c2=='w')
            {
                addInt(Extruder::current->watchPeriod,4);
            }
#if RETRACT_DURING_HEATUP
            else if(c2=='T')
            {
                addInt(Extruder::current->waitRetractTemperature,4);
            }
            else if(c2=='U')
            {
                addInt(Extruder::current->waitRetractUnits,2);
            }
#endif
            else if(c2=='h')
            {
                uint8_t hm = currHeaterForSetup->heatManager;
                if(hm == HTR_PID)
                    addStringP(PSTR(UI_TEXT_STRING_HM_PID));
                else if(hm == HTR_DEADTIME)
                    addStringP(PSTR(UI_TEXT_STRING_HM_DEADTIME));
                else if(hm == HTR_SLOWBANG)
                    addStringP(PSTR(UI_TEXT_STRING_HM_SLOWBANG));
                else
                    addStringP(PSTR(UI_TEXT_STRING_HM_BANGBANG));
            }
#if USE_ADVANCE
#if ENABLE_QUADRATIC_ADVANCE
            else if(c2=='a')
            {
                addFloat(Extruder::current->advanceK, 3, 0);
            }
#endif
            else if(c2=='l')
            {
                addFloat(Extruder::current->advanceL, 3, 0);
            }
#endif
            else if(c2=='x')
            {
                addFloat(Extruder::current->xOffset * Printer::invAxisStepsPerMM[X_AXIS], 3, 2);
            }
            else if(c2=='y')
            {
                addFloat(Extruder::current->yOffset * Printer::invAxisStepsPerMM[Y_AXIS], 3, 2);
            }
            else if(c2=='f')
            {
                addFloat(Extruder::current->maxStartFeedrate,5,0);
            }
            else if(c2=='F')
            {
                addFloat(Extruder::current->maxFeedrate,5,0);
            }
            else if(c2=='A')
            {
                addFloat(Extruder::current->maxAcceleration,5,0);
            }
#endif
            break;
        case 'y':
#if DRIVE_SYSTEM==DELTA
            if(c2>='0' && c2<='3') fvalue = (float)Printer::currentDeltaPositionSteps[c2-'0']*Printer::invAxisStepsPerMM[c2-'0'];
            addFloat(fvalue,3,2);
#endif
            break;
//Davinci Specific, autoleveling display
         case 'Z':
            if(c2=='1' || c2=='2' || c2=='3')
                    {
                        int p = c2- '0';
                        p--;
                        if (Z_probe[p] == -1000)
                            {
                                addStringP("  --.--- mm");
                            }
                        else
                             if (Z_probe[p] == -2000)
                            {
                                addStringP(UI_TEXT_FAILED);
                            }
                        else
                            {
                                addFloat(Z_probe[p]-10 ,4,3);
                                addStringP(" mm");
                            }
                    }
            break;
        case 'z':
            if(c2=='m')
             {
             addFloat(Printer::zMin,4,2);
             }
        }
    }
    uid.printCols[col] = 0;
}
void UIDisplay::setStatusP(PGM_P txt,bool error)
{
    if(!error && Printer::isUIErrorMessage()) return;
    uint8_t i=0;
    while(i<20)
    {
        uint8_t c = pgm_read_byte(txt++);
        if(!c) break;
        statusMsg[i++] = c;
    }
    statusMsg[i]=0;
    if(error)
        Printer::setUIErrorMessage(true); 
}
void UIDisplay::setStatus(const char *txt,bool error)
{
    if(!error && Printer::isUIErrorMessage()) return;
    uint8_t i=0;
    while(*txt && i<20)
        statusMsg[i++] = *txt++;
    statusMsg[i]=0;
    if(error)
        Printer::setUIErrorMessage(true);
}

const UIMenu * const ui_pages[UI_NUM_PAGES] PROGMEM = UI_PAGES;
uint16_t nFilesOnCard;
void UIDisplay::updateSDFileCount()
{
#if SDSUPPORT
    dir_t* p = NULL;
    byte offset = menuTop[menuLevel];
    SdBaseFile *root = sd.fat.vwd();

    root->rewind();
    nFilesOnCard = 0;
    //Davinci Specific, to get filename and filter according extension
    while ((p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        if (! (DIR_IS_FILE(p) || DIR_IS_SUBDIR(p)))
            continue;
        if (folderLevel>=SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.'))
            continue;
//Davinci Specific, to get filename and filter according extension
 #if HIDE_BINARY_ON_SD
        //hide unwished files
        if (!SDCard::showFilename(p,tempLongFilename))continue;
#endif
        nFilesOnCard++;
        if (nFilesOnCard > 5000) // Arbitrary maximum, limited only by how long someone would scroll
            return;
    }
#endif
}

void getSDFilenameAt(uint16_t filePos,char *filename)
{
#if SDSUPPORT
    dir_t* p;
    SdBaseFile *root = sd.fat.vwd();

    root->rewind();
    while ((p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        HAL::pingWatchdog();
        if (!DIR_IS_FILE(p) && !DIR_IS_SUBDIR(p)) continue;
        if(uid.folderLevel>=SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.')) continue;
//Davinci Specific, to get filename and filter according extension 
#if HIDE_BINARY_ON_SD
        //hide unwished files
        if (!SDCard::showFilename(p,tempLongFilename))continue;
#endif
        if (filePos--)
            continue;
        strcpy(filename, tempLongFilename);
        if(DIR_IS_SUBDIR(p)) strcat(filename, "/"); // Set marker for directory
        break;
    }
#endif
}

bool UIDisplay::isDirname(char *name)
{
    while(*name) name++;
    name--;
    return *name=='/';
}

void UIDisplay::goDir(char *name)
{
#if SDSUPPORT
    char *p = cwd;
    while(*p)p++;
    if(name[0]=='.' && name[1]=='.')
    {
        if(folderLevel==0) return;
        p--;
        p--;
        while(*p!='/') p--;
        p++;
        *p = 0;
        folderLevel--;
    }
    else
    {
        if(folderLevel>=SD_MAX_FOLDER_DEPTH) return;
        while(*name) *p++ = *name++;
        *p = 0;
        folderLevel++;
    }
    sd.fat.chdir(cwd);
    updateSDFileCount();
#endif
}

//Davinci Specific, integrate function in class
void UIDisplay::sdrefresh(uint16_t &r,char cache[UI_ROWS][MAX_COLS+1])
{
#if SDSUPPORT
    dir_t* p = NULL;
    uint16_t offset = uid.menuTop[uid.menuLevel];
    SdBaseFile *root;
    uint16_t length, skip;

    sd.fat.chdir(uid.cwd);
    root = sd.fat.vwd();
    root->rewind();

    skip = (offset > 0 ? offset - 1 : 0);

    while (r + offset < nFilesOnCard + 1 && r < UI_ROWS && (p = root->getLongFilename(p, tempLongFilename, 0, NULL)))
    {
        HAL::pingWatchdog();
        // done if past last used entry
        // skip deleted entry and entries for . and  ..
        // only list subdirectories and files
        if ((DIR_IS_FILE(p) || DIR_IS_SUBDIR(p)))
        {
            if(uid.folderLevel >= SD_MAX_FOLDER_DEPTH && DIR_IS_SUBDIR(p) && !(p->name[0]=='.' && p->name[1]=='.'))
                continue;
//Davinci Specific, to get filename and filter according extension 
#if HIDE_BINARY_ON_SD
            //hide unwished files
            if (!SDCard::showFilename(p,tempLongFilename))continue;
 #endif
            if(skip > 0)
            {
                skip--;
                continue;
            }
            uid.col = 0;
            if(r + offset == uid.menuPos[uid.menuLevel])
                uid.printCols[uid.col++] = CHAR_SELECTOR;
            else
                uid.printCols[uid.col++] = ' ';
            // print file name with possible blank fill
            if(DIR_IS_SUBDIR(p))
                uid.printCols[uid.col++] = bFOLD; // Prepend folder symbol
            length = RMath::min((int)strlen(tempLongFilename), MAX_COLS - uid.col);
            memcpy(uid.printCols + uid.col, tempLongFilename, length);
            uid.col += length;
            uid.printCols[uid.col] = 0;
            strcpy(cache[r++],uid.printCols);
        }
    }
#endif
}

// Refresh current menu page
void UIDisplay::refreshPage()
{
#if  UI_DISPLAY_TYPE == DISPLAY_GAMEDUINO2
    GD2::refresh();
#else
    uint16_t r;
    uint8_t mtype;
    char cache[UI_ROWS][MAX_COLS+1];
    adjustMenuPos();
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    // Reset timeout on menu back when user active on menu
    if (uid.encoderLast != encoderStartScreen)
        ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;
#endif
//Davinci Specific, powersave, light management
#if UI_AUTOLIGHTOFF_AFTER!=0
    //reset timeout for power saving
    if (uid.encoderLast != encoderStartScreen)
        UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif
    encoderStartScreen = uid.encoderLast;

    // Copy result into cache
    if(menuLevel == 0) // Top level menu
    {
        UIMenu *men = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
        uint16_t nr = pgm_read_word_near(&(men->numEntries));
        UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
        //Davinci Specific, check entry is visible
        UIMenuEntry *testent =(UIMenuEntry *)pgm_read_word(&(entries[0]));
         if (!testent->showEntry())//not visible so look for next one
            {
            int  nb = 0;
            bool bloop =true;
          while( nb < UI_NUM_PAGES+1 && bloop)
              {
                  if (lastAction==UI_ACTION_PREVIOUS)
                  {
                    menuPos[0] = (menuPos[0]==0 ? UI_NUM_PAGES-1 : menuPos[0]-1);
                  }
                  else
                  {
                  menuPos[0]++;
                  if(menuPos[0]>=UI_NUM_PAGES)menuPos[0]=0;
                 }
              nb++; //to avoid to loop more than all entries once
              UIMenu *mentest = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
              UIMenuEntry **entriestest = (UIMenuEntry**)pgm_read_word(&(mentest->entries));
              UIMenuEntry *enttest=(UIMenuEntry *)pgm_read_word(&(entriestest[0]));
               if (enttest->showEntry()) bloop=false; //ok we found visible so we end here
               }
           if (bloop) menuPos[0]=0; //nothing was found so set page 0 even not visible
           men = (UIMenu*)pgm_read_word(&(ui_pages[menuPos[0]]));
            nr = pgm_read_word_near(&(men->numEntries));
            entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
            }
        for(r = 0; r < nr && r < UI_ROWS; r++)
        {
            UIMenuEntry *ent = (UIMenuEntry *)pgm_read_word(&(entries[r]));
            col = 0;
            parse((char*)pgm_read_word(&(ent->text)),false);
            strcpy(cache[r],uid.printCols);
        }
    }
    else
    {
        //Davinci Specific, to filter according UI
	int numrows = UI_ROWS;
        UIMenu *men = (UIMenu*)menu[menuLevel];
        uint16_t nr = pgm_read_word_near((void*)&(men->numEntries));
        mtype = pgm_read_byte((void*)&(men->menuType));
        uint16_t offset = menuTop[menuLevel];
        UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
        //Davinci Specific, to filter according UI
	if (mtype== UI_MENU_TYPE_MENU_WITH_STATUS)numrows--;
        for(r = 0; r + offset < nr && r < numrows; )
        {
            UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[r+offset]));
            if(!ent->showEntry())
            {
                offset++;
                continue;
            }
            uint8_t entType = pgm_read_byte(&(ent->menuType));
            uint16_t entAction = pgm_read_word(&(ent->action));
            col = 0;
	    //Davinci Specific, to filter according entry type
            if(entType>=UI_MENU_ENTRY_MIN_TYPE_CHECK && entType<=UI_MENU_ENTRY_MAX_TYPE_CHECK)
            {
                if(r + offset == menuPos[menuLevel] && activeAction != entAction)
                    uid.printCols[col++] = CHAR_SELECTOR;
                else if(activeAction == entAction)
                    uid.printCols[col++] = CHAR_SELECTED;
                else
                    uid.printCols[col++]=' ';
            }
            parse((char*)pgm_read_word(&(ent->text)),false);
	    //Davinci Specific, to filter according entry type
            if(entType==UI_MENU_TYPE_SUBMENU || entType==UI_MENU_TYPE_MENU_WITH_STATUS)   // Draw submenu marker at the right side
            {
                while(col<UI_COLS-1) uid.printCols[col++]=' ';
                if(col>UI_COLS)
                {
                    uid.printCols[RMath::min(UI_COLS-1,col)] = CHAR_RIGHT;
                }
                else
                    uid.printCols[col] = CHAR_RIGHT; // Arrow right
                uid.printCols[++col] = 0;
            }
            strcpy(cache[r],uid.printCols);
            r++;
        }
	//Davinci Specific, to filter according entry type
        if (mtype==UI_MENU_TYPE_MENU_WITH_STATUS)
        {
        UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[nr-1]));   //this is status bar
        uid.printCols[0]=0;//fill with blank line if number of entries < 3(1 or 2)
        if (nr<UI_ROWS)r--;
        while(r<UI_ROWS)
            {
            strcpy(cache[r++],uid.printCols);
            }
        col=0;//init parsing
         parse((char*)pgm_read_word(&(ent->text)),false);
         strcpy(cache[UI_ROWS-1],uid.printCols);//copy status
         r=UI_ROWS;
        }
    }
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)
    {
        sdrefresh(r,cache);
    }
#endif

    uid.printCols[0] = 0;
    while(r < UI_ROWS) // delete trailing empty rows
        strcpy(cache[r++],uid.printCols);
    // cache now contains the data to show
    // Compute transition
    uint8_t transition = 0; // 0 = display, 1 = up, 2 = down, 3 = left, 4 = right
#if UI_ANIMATION
    if(menuLevel != oldMenuLevel && !PrintLine::hasLines())
    {
        if(oldMenuLevel == 0 || oldMenuLevel == -2)
            transition = 1;
        else if(menuLevel == 0)
            transition = 2;
        else if(menuLevel>oldMenuLevel)
            transition = 3;
        else
            transition = 4;
    }
#endif
    uint8_t loops = 1;
    uint8_t dt = 1,y;
    if(transition == 1 || transition == 2) loops = UI_ROWS;
    else if(transition > 2)
    {
        dt = (UI_COLS + UI_COLS - 1) / 16;
        loops = UI_COLS + 1 / dt;
    }
    uint8_t off0 = (shift <= 0 ? 0 : shift);
    uint8_t scroll = dt;
    uint8_t off[UI_ROWS];
    if(transition == 0) // Copy cache to displayCache
    {
        for(y = 0; y < UI_ROWS; y++)
            strcpy(displayCache[y],cache[y]);
    }
    for(y = 0; y < UI_ROWS; y++)
    {
        uint8_t len = strlen(displayCache[y]); // length of line content
        off[y] = len > UI_COLS ? RMath::min(len - UI_COLS,off0) : 0;
        if(len > UI_COLS)
        {
            off[y] = RMath::min(len - UI_COLS,off0);
            if(transition == 0 && (mtype == UI_MENU_TYPE_FILE_SELECTOR || mtype == UI_MENU_TYPE_SUBMENU))  // Copy first char to front
            {
                //displayCache[y][off[y]] = displayCache[y][0];
                cache[y][off[y]] = cache[y][0];
            }
        }
        else off[y] = 0;
#if UI_ANIMATION
        if(transition == 3)
        {
            for(r = len; r < MAX_COLS; r++)
            {
                displayCache[y][r] = 32;
            }
            displayCache[y][MAX_COLS] = 0;
        }
        else if(transition == 4)
        {
            for(r = strlen(cache[y]); r < MAX_COLS; r++)
            {
                cache[y][r] = 32;
            }
            cache[y][MAX_COLS] = 0;
        }
#endif
    }
    for(uint8_t l = 0; l<loops; l++)
    {
        if(uid.encoderLast != encoderStartScreen)
        {
            scroll = 200;
        }
        scroll += dt;
#if UI_DISPLAY_TYPE == DISPLAY_U8G
#define drawHProgressBar(x,y,width,height,progress) \
     {u8g_DrawFrame(&u8g,x,y, width, height);  \
     int p = ceil((width-2) * progress / 100); \
     u8g_DrawBox(&u8g,x+1,y+1, p, height-2);}


#define drawVProgressBar(x,y,width,height,progress) \
     {u8g_DrawFrame(&u8g,x,y, width, height);  \
     int p = height-1 - ceil((height-2) * progress / 100); \
     u8g_DrawBox(&u8g,x+1,y+p, width-2, (height-p));}
#if UI_DISPLAY_TYPE == DISPLAY_U8G
#if SDSUPPORT
        unsigned long sdPercent;
#endif
        //fan
        int fanPercent;
        char fanString[2];
        if(menuLevel == 0 && menuPos[0] == 0 ) // Main menu with special graphics
        {
//ext1 and ext2 animation symbols
            if(pwm_pos[extruder[0].tempControl.pwmIndex] > 0)
                cache[0][0] = Printer::isAnimation()?'\x08':'\x09';
            else
                cache[0][0] = '\x0a'; //off
#if NUM_EXTRUDER>1
            if(pwm_pos[extruder[1].tempControl.pwmIndex] > 0)
                cache[1][0] = Printer::isAnimation()?'\x08':'\x09';
            else
                cache[1][0] = '\x0a'; //off
#endif
#if HAVE_HEATED_BED
            //heatbed animated icons
            uint8_t lin = 2 - ((NUM_EXTRUDER < 2) ? 1 : 0);
            if(pwm_pos[heatedBedController.pwmIndex] > 0)
                cache[lin][0] = Printer::isAnimation() ? '\x0c' : '\x0d';
            else
                cache[lin][0] = '\x0b';
#endif
#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
            //fan
            fanPercent = Printer::getFanSpeed() * 100 / 255;
            fanString[1] = 0;
            if(fanPercent > 0)  //fan running anmation
            {
                fanString[0] = Printer::isAnimation() ? '\x0e' : '\x0f';
            }
            else
            {
                fanString[0] = '\x0e';
            }
#endif
#if SDSUPPORT
            //SD Card
            if(sd.sdactive)
            {
                if(sd.sdactive && sd.sdmode)
                {
                    if(sd.filesize < 20000000) sdPercent = sd.sdpos * 100 / sd.filesize;
                    else sdPercent = (sd.sdpos >> 8) * 100 / (sd.filesize >> 8);
                }
                else
                {
                    sdPercent = 0;
                }
            }
#endif
        }
#endif
        //u8g picture loop
        u8g_FirstPage(&u8g);
        do
        {
#endif
            if(transition == 0)
            {
#if UI_DISPLAY_TYPE == DISPLAY_U8G
                if(menuLevel==0 && menuPos[0] == 0 )
                {
                    u8g_SetFont(&u8g,UI_FONT_SMALL);
                    uint8_t py = 8;
                    for(uint8_t r = 0; r < 3; r++)
                    {
                        if(u8g_IsBBXIntersection(&u8g, 0, py-UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                            printU8GRow(0, py, cache[r]);
                        py += 10;
                    }
#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
                    //fan
                    if(u8g_IsBBXIntersection(&u8g, 0, 30 - UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(117,30,fanString);
                    drawVProgressBar(116, 0, 9, 20, fanPercent);
                    if(u8g_IsBBXIntersection(&u8g, 0, 42 - UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,42,cache[3]); //mul + extruded
                    if(u8g_IsBBXIntersection(&u8g, 0, 52 - UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,52,cache[4]); //buf
#endif
#if SDSUPPORT
                    //SD Card
                    if(sd.sdactive && u8g_IsBBXIntersection(&u8g, 66, 52 - UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                    {
                        printU8GRow(66,52,"SD");
                        drawHProgressBar(79,46, 46, 6, sdPercent);
                    }
#endif
                    //Status
                    py = u8g_GetHeight(&u8g) - 2;
                    if(u8g_IsBBXIntersection(&u8g, 70, py - UI_FONT_SMALL_HEIGHT, 1, UI_FONT_SMALL_HEIGHT))
                        printU8GRow(0,py,cache[5]);

                    //divider lines
                    u8g_DrawHLine(&u8g,0, 32, u8g_GetWidth(&u8g));
                    if ( u8g_IsBBXIntersection(&u8g, 54, 0, 1, 55) )
                    {
                        u8g_draw_vline(&u8g,112, 0, 32);
                        u8g_draw_vline(&u8g,62, 0, 54);
                    }
                    u8g_SetFont(&u8g, UI_FONT_DEFAULT);
                }
                else
                {
#endif
                    for(y = 0; y < UI_ROWS; y++)
                        printRow(y, &cache[y][off[y]], NULL, UI_COLS);
#if UI_DISPLAY_TYPE == DISPLAY_U8G
                }
#endif
            }
#if UI_ANIMATION
            else
            {
                if(transition == 1)   // up
                {
                    if(scroll > UI_ROWS)
                    {
                        scroll = UI_ROWS;
                        l = loops;
                    }
                    for(y = 0; y < UI_ROWS - scroll; y++)
                    {
                        r = y + scroll;
                        printRow(y, &displayCache[r][off[r]], NULL, UI_COLS);
                    }
                    for(y = 0; y < scroll; y++)
                    {
                        printRow(UI_ROWS - scroll + y,cache[y], NULL, UI_COLS);
                    }
                }
                else if(transition == 2)     // down
                {
                    if(scroll > UI_ROWS)
                    {
                        scroll = UI_ROWS;
                        l = loops;
                    }
                    for(y = 0; y < scroll; y++)
                    {
                        printRow(y, cache[UI_ROWS - scroll + y], NULL, UI_COLS);
                    }
                    for(y = 0; y < UI_ROWS - scroll; y++)
                    {
                        r = y + scroll;
                        printRow(y + scroll, &displayCache[y][off[y]], NULL, UI_COLS);
                    }
                }
                else if(transition == 3)     // left
                {
                    if(scroll > UI_COLS)
                    {
                        scroll = UI_COLS;
                        l = loops;
                    }
                    for(y = 0; y < UI_ROWS; y++)
                    {
                        printRow(y,&displayCache[y][off[y] + scroll], cache[y], UI_COLS - scroll);
                    }
                }
                else     // right
                {
                    if(scroll > UI_COLS)
                    {
                        scroll = UI_COLS;
                        l = loops;
                    }
                    for(y = 0; y < UI_ROWS; y++)
                    {
                        printRow(y, cache[y] + UI_COLS - scroll, &displayCache[y][off[y]], scroll);
                    }
                }
#if UI_DISPLAY_TYPE != DISPLAY_U8G
                HAL::delayMilliseconds(transition < 3 ? 200 : 70);
#endif
                HAL::pingWatchdog();
            }
#endif
#if UI_DISPLAY_TYPE == DISPLAY_U8G
        }
        while( u8g_NextPage(&u8g) );  //end picture loop
        Printer::toggleAnimation();
#endif
    } // for l
#if UI_ANIMATION
    // copy to last cache
    if(transition != 0)
        for(y = 0; y < UI_ROWS; y++)
            strcpy(displayCache[y], cache[y]);
    oldMenuLevel = menuLevel;
#endif
#endif
}

void UIDisplay::pushMenu(const UIMenu *men, bool refresh)
{
    if(men == menu[menuLevel])
    {
        refreshPage();
        return;
    }
//Davinci Specific, readability
    if(menuLevel == UI_MENU_MAXLEVEL-1) return; // Max. depth reached. No more memory to down further.
    menuLevel++;
    menu[menuLevel] = men;
    menuTop[menuLevel] = menuPos[menuLevel] = 0;
#if SDSUPPORT
    UIMenu *men2 = (UIMenu*)menu[menuLevel];
//Davinci Specific, readability
    if(pgm_read_byte(&(men2->menuType)) == UI_MENU_TYPE_FILE_SELECTOR) 
    {
        // Menu is Open files list
        updateSDFileCount();
        // Keep menu positon in file list, more user friendly.
        // If file list changed, still need to reset position.
        if (menuPos[menuLevel] > nFilesOnCard)
        {
            //This exception can happen if the card was unplugged or modified.
            menuTop[menuLevel] = 0;
            menuPos[menuLevel] = UI_MENU_BACKCNT; // if top entry is back, default to next useful item
        }
    }
    else
#endif
    {
        // With or without SDCARD, being here means the menu is not a files list
        // Reset menu to top
        menuTop[menuLevel] = 0;
        menuPos[menuLevel] = UI_MENU_BACKCNT; // if top entry is back, default to next useful item
    }
    if(refresh)
        refreshPage();
}
void UIDisplay::popMenu(bool refresh)
{
    if(menuLevel > 0) menuLevel--;
    Printer::setAutomount(false);
    activeAction = 0;
    if(refresh)
        refreshPage();
}
int UIDisplay::okAction(bool allowMoves)
{
    if(Printer::isUIErrorMessage())
    {
        Printer::setUIErrorMessage(false);
        return 0;
    }
    BEEP_SHORT
#if UI_HAS_KEYS == 1
    if(menuLevel == 0)   // Enter menu
    {
        menuLevel = 1;
        menuTop[1] = 0;
        menuPos[1] =  UI_MENU_BACKCNT; // if top entry is back, default to next useful item
        menu[1] = &ui_menu_main;
        return 0;
    }
    UIMenu *men = (UIMenu*)menu[menuLevel];
    //uint8_t nr = pgm_read_word_near(&(menu->numEntries));
    uint8_t mtype = pgm_read_byte(&(men->menuType));
    UIMenuEntry **entries;
    UIMenuEntry *ent;
    unsigned char entType;
    int action;
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)
    {
        if(menuPos[menuLevel] == 0)   // Selected back instead of file
        {
            return executeAction(UI_ACTION_BACK, allowMoves);
        }

        if(!sd.sdactive)
            return 0;
        uint8_t filePos = menuPos[menuLevel] - 1;
        char filename[LONG_FILENAME_LENGTH + 1];

        getSDFilenameAt(filePos, filename);
        if(isDirname(filename))   // Directory change selected
        {
            goDir(filename);
            menuTop[menuLevel] = 0;
            menuPos[menuLevel] = 1;
            refreshPage();
            oldMenuLevel = -1;
            return 0;
        }

        int16_t shortAction; // renamed to avoid scope confusion
        if (Printer::isAutomount())
            shortAction = UI_ACTION_SD_PRINT;
        else
        {
            men = menu[menuLevel - 1];
            entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
            ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel-1]]));
            shortAction = pgm_read_word(&(ent->action));
        }
        sd.file.close();
        sd.fat.chdir(cwd);
        switch(shortAction)
        {
        case UI_ACTION_SD_PRINT:
            if (sd.selectFile(filename, false))
            {
                sd.startPrint();
                BEEP_LONG;
                menuLevel = 0;
            }
            break;
        case UI_ACTION_SD_DELETE:
            if(sd.sdactive)
            {
                sd.sdmode = 0;
                sd.file.close();
                if(sd.fat.remove(filename))
                {
                    Com::printFLN(Com::tFileDeleted);
                    BEEP_LONG
                    if(menuPos[menuLevel] > 0)
                        menuPos[menuLevel]--;
                    updateSDFileCount();
                }
                else
                {
                    Com::printFLN(Com::tDeletionFailed);
                }
            }
            break;
        }
        return 0;
    }
#endif
    entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
    entType = pgm_read_byte(&(ent->menuType));// 0 = Info, 1 = Headline, 2 = submenu ref, 3 = direct action command, 4 = modify action
    action = pgm_read_word(&(ent->action));
    ////Davinci Specific, Bug not reported
    if(mtype == UI_MENU_TYPE_ACTION_MENU)   // action menu
    {
        action = pgm_read_word(&(men->id));
        finishAction(action);
        return executeAction(UI_ACTION_BACK, true);
    }
     //Davinci Specific, readability
    if(((mtype == UI_MENU_TYPE_SUBMENU) ||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS) )&& entType == UI_MENU_TYPE_MODIFICATION_MENU)   // Modify action
    {
        if(activeAction)   // finish action
        {
            finishAction(action);
            activeAction = 0;
        }
        else
            activeAction = action;
        return 0;
    }
    if(mtype == UI_MENU_TYPE_WIZARD)
    {
        action = pgm_read_word(&(men->id));
        switch(action)
        {
#if FEATURE_RETRACTION
//Davinci Specific, use another way to end wizard because based on Load/Unload Menu
//          case UI_ACTION_WIZARD_FILAMENTCHANGE: // filament change is finished
//            BEEP_SHORT;
//            popMenu(true);
//            Extruder::current->retractDistance(EEPROM_FLOAT(RETRACTION_LENGTH));
//#if FILAMENTCHANGE_REHOME
//#if Z_HOME_DIR > 0
//            Printer::homeAxis(true, true, FILAMENTCHANGE_REHOME == 2);
//#else
//            Printer::homeAxis(true, true, false);
//#endif
//#endif
//            Printer::GoToMemoryPosition(true, true, false, false, Printer::homingFeedrate[X_AXIS]);
//            Printer::GoToMemoryPosition(false, false, true, false, Printer::homingFeedrate[Z_AXIS]);
//            Extruder::current->retractDistance(-EEPROM_FLOAT(RETRACTION_LENGTH));
//            Printer::currentPositionSteps[E_AXIS] = Printer::popWizardVar().l; // set e to starting position
//            Printer::setBlockingReceive(false);
//#if EXTRUDER_JAM_CONTROL
//            Extruder::markAllUnjammed();
//#endif
//            Printer::setJamcontrolDisabled(false);
//            break;
#if EXTRUDER_JAM_CONTROL
        case UI_ACTION_WIZARD_JAM_REHEAT: // user saw problem and takes action
            popMenu(false);
            pushMenu(&ui_wiz_jamwaitheat, true);
            Extruder::unpauseExtruders();
            popMenu(false);
            pushMenu(&ui_wiz_filamentchange, true);
            break;
        case UI_ACTION_WIZARD_JAM_WAITHEAT: // called while heating - should do nothing user must wait
            BEEP_LONG;
            break;
#endif // EXTRUDER_JAM_CONTROL
#endif
        }
        return 0;
    }
  //Davinci Specific, readability
    if(entType==UI_MENU_TYPE_MENU_WITH_STATUS || entType==UI_MENU_TYPE_SUBMENU)   // Enter submenu
    {
        pushMenu((UIMenu*)action, false);
//        BEEP_SHORT
#if FEATURE_BABYSTEPPING
        zBabySteps = 0;
#endif
#if HAVE_HEATED_BED
        if(action == pgm_read_word(&ui_menu_conf_bed.action))  // enter Bed configuration menu
            currHeaterForSetup = &heatedBedController;
        else
#endif
            currHeaterForSetup = &(Extruder::current->tempControl);
        Printer::setMenuMode(MENU_MODE_FULL_PID, currHeaterForSetup->heatManager == 1);
        Printer::setMenuMode(MENU_MODE_DEADTIME, currHeaterForSetup->heatManager == 3);
        return 0;
    }
    if(entType == 3)
    {
        return executeAction(action, allowMoves);
    }
    return executeAction(UI_ACTION_BACK, allowMoves);
#endif
}

//#define INCREMENT_MIN_MAX(a,steps,_min,_max) if ( (increment<0) && (_min>=0) && (a<_min-increment*steps) ) {a=_min;} else { a+=increment*steps; if(a<_min) a=_min; else if(a>_max) a=_max;};

// this version not have single byte variable rollover bug
#define INCREMENT_MIN_MAX(a,steps,_min,_max) a = constrain((a + increment*steps), _min, _max);

void UIDisplay::adjustMenuPos()
{
    if(menuLevel == 0) return;
    UIMenu *men = (UIMenu*)menu[menuLevel];
    UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    uint8_t mtype = HAL::readFlashByte((PGM_P)&(men->menuType));
    int numEntries = pgm_read_word(&(men->numEntries));
//Davinci Specific, New UI entry
    int numrows=UI_ROWS;
    if(!((mtype == UI_MENU_TYPE_SUBMENU)||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS))) return;
    if (mtype == UI_MENU_TYPE_MENU_WITH_STATUS)numrows--;
    UIMenuEntry *entry;
    while(menuPos[menuLevel] > 0) // Go up until we reach visible position
    {
        entry = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
        if(pgm_read_byte((void*)&(entry->menuType)) == 1) // skip headlines
            menuPos[menuLevel]--;
        else if(entry->showEntry())
            break;
        else
            menuPos[menuLevel]--;
    }

    // with bad luck the only visible option was in the opposite direction
    while(menuPos[menuLevel] < numEntries - 1) // Go down until we reach visible position
    {
        entry = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
        if(pgm_read_byte((void*)&(entry->menuType)) == 1) // skip headlines
            menuPos[menuLevel]++;
        else if(entry->showEntry())
            break;
        else
            menuPos[menuLevel]++;
    }

    uint8_t skipped = 0;
    bool modified;
    if(menuTop[menuLevel] > menuPos[menuLevel])
        menuTop[menuLevel] = menuPos[menuLevel];
    do
    {
        skipped = 0;
        modified = false;
        for(uint8_t r = menuTop[menuLevel]; r < menuPos[menuLevel]; r++)
        {
            UIMenuEntry *ent = (UIMenuEntry *)pgm_read_word(&(entries[r]));
            if(!ent->showEntry())
                skipped++;
        }
        //Davinci Specific, new UI entry
        if(menuTop[menuLevel] + skipped + numrows - 1 < menuPos[menuLevel])
        {
            menuTop[menuLevel] = menuPos[menuLevel] + 1 - numrows;
            modified = true;
        }
    }
    while(modified);
}

bool UIDisplay::isWizardActive()
{
    UIMenu *men = (UIMenu*)menu[menuLevel];
//Davinci Specific, readability
    return HAL::readFlashByte((PGM_P)&(men->menuType)) == UI_MENU_TYPE_WIZARD;
}

bool UIDisplay::nextPreviousAction(int16_t next, bool allowMoves)
{
    if(Printer::isUIErrorMessage())
    {
        Printer::setUIErrorMessage(false);
        return true;
    }
    millis_t actTime = HAL::timeInMilliseconds();
    millis_t dtReal;
    millis_t dt = dtReal = actTime - lastNextPrev;
    lastNextPrev = actTime;
    if(dt < SPEED_MAX_MILLIS) dt = SPEED_MAX_MILLIS;
    if(dt > SPEED_MIN_MILLIS)
    {
        dt = SPEED_MIN_MILLIS;
        lastNextAccumul = 1;
    }
    float f = (float)(SPEED_MIN_MILLIS - dt) / (float)(SPEED_MIN_MILLIS - SPEED_MAX_MILLIS);
    lastNextAccumul = 1.0f + (float)SPEED_MAGNIFICATION * f * f * f;
#if UI_DYNAMIC_ENCODER_SPEED
    uint16_t dynSp = lastNextAccumul / 16;
    if(dynSp < 1)  dynSp = 1;
    if(dynSp > 30) dynSp = 30;
    next *= dynSp;
#endif

#if UI_HAS_KEYS == 1
    if(menuLevel == 0)
    {
        lastSwitch = HAL::timeInMilliseconds();
        if((UI_INVERT_MENU_DIRECTION && next < 0) || (!UI_INVERT_MENU_DIRECTION && next > 0))
        {
            menuPos[0]++;
            if(menuPos[0] >= UI_NUM_PAGES)
                menuPos[0] = 0;
        }
        else
        {
            menuPos[0] = (menuPos[0] == 0 ? UI_NUM_PAGES - 1 : menuPos[0] - 1);
        }
        return true;
    }
    UIMenu *men = (UIMenu*)menu[menuLevel];
    uint8_t nr = pgm_read_word_near(&(men->numEntries));
    uint8_t mtype = HAL::readFlashByte((PGM_P)&(men->menuType));
    UIMenuEntry **entries = (UIMenuEntry**)pgm_read_word(&(men->entries));
    UIMenuEntry *ent =(UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
    UIMenuEntry *testEnt;
    // 0 = Info, 1 = Headline, 2 = submenu ref, 3 = direct action command
    uint8_t entType = HAL::readFlashByte((PGM_P)&(ent->menuType));
    int action = pgm_read_word(&(ent->action));
    //Davinci Specific, special UI entry
    if(((mtype == UI_MENU_TYPE_SUBMENU)||(mtype == UI_MENU_TYPE_MENU_WITH_STATUS)) && activeAction == 0)   // browse through menu items
    {
        if((UI_INVERT_MENU_DIRECTION && next < 0) || (!UI_INVERT_MENU_DIRECTION && next > 0))
        {
            while(menuPos[menuLevel] + 1 < nr)
            {
                //Davinci Specific, special UI entry
		if ((mtype == UI_MENU_TYPE_MENU_WITH_STATUS)&&(menuPos[menuLevel]+2 ==nr))break;
                menuPos[menuLevel]++;
                testEnt = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
                if(testEnt->showEntry())
                    break;
            }
        }
        else if(menuPos[menuLevel] > 0)
        {
            while(menuPos[menuLevel] > 0)
            {
                menuPos[menuLevel]--;
                testEnt = (UIMenuEntry *)pgm_read_word(&(entries[menuPos[menuLevel]]));
                if(testEnt->showEntry())
                    break;
            }
        }
        shift = -2; // reset shift position
        adjustMenuPos();
        return true;
    }
#if SDSUPPORT
    if(mtype == UI_MENU_TYPE_FILE_SELECTOR)   // SD listing
    {
        if((UI_INVERT_MENU_DIRECTION && next < 0) || (!UI_INVERT_MENU_DIRECTION && next > 0))
        {
            menuPos[menuLevel] += abs(next);
            if(menuPos[menuLevel] > nFilesOnCard) menuPos[menuLevel] = nFilesOnCard;
        }
        else if(menuPos[menuLevel] > 0)
        {
            if(menuPos[menuLevel] > abs(next))
                menuPos[menuLevel] -= abs(next);
            else
                menuPos[menuLevel] = 0;
        }
        if(menuTop[menuLevel] > menuPos[menuLevel])
            menuTop[menuLevel] = menuPos[menuLevel];
        else if(menuTop[menuLevel] + UI_ROWS - 1 < menuPos[menuLevel])
            menuTop[menuLevel] = menuPos[menuLevel] + 1 - UI_ROWS;
        shift = -2; // reset shift position
        return true;
    }
#endif
//BUG ?? 
    if(mtype == UI_MENU_TYPE_ACTION_MENU || mtype == UI_MENU_TYPE_WIZARD) action = pgm_read_word(&(men->id));
    else action = activeAction;
     //Davinci Specific, use key Up for - and down for +
    int8_t increment = -next;
    switch(action)
    {
    case UI_ACTION_FANSPEED:
        Commands::setFanSpeed(Printer::getFanSpeed() + increment * 3,false);
        break;
    case UI_ACTION_XPOSITION:
        if(!allowMoves) return false;
#if UI_SPEEDDEPENDENT_POSITIONING
        {
            float d = 0.01*(float)increment * lastNextAccumul;
            if(fabs(d) * 2000 > Printer::maxFeedrate[X_AXIS] * dtReal)
                d *= Printer::maxFeedrate[X_AXIS]*dtReal / (2000 * fabs(d));
            long steps = (long)(d * Printer::axisStepsPerMM[X_AXIS]);
            steps = ( increment < 0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
            PrintLine::moveRelativeDistanceInStepsReal(steps,0,0,0,Printer::maxFeedrate[X_AXIS],true);
        }
#else
        PrintLine::moveRelativeDistanceInStepsReal(increment,0,0,0,Printer::homingFeedrate[X_AXIS],true);
#endif
        Commands::printCurrentPosition(PSTR("UI_ACTION_XPOSITION "));
        break;
    case UI_ACTION_YPOSITION:
        if(!allowMoves) return false;
#if UI_SPEEDDEPENDENT_POSITIONING
        {
            float d = 0.01 * (float)increment * lastNextAccumul;
            if(fabs(d) * 2000 > Printer::maxFeedrate[Y_AXIS] * dtReal)
                d *= Printer::maxFeedrate[Y_AXIS] * dtReal / (2000 * fabs(d));
            long steps = (long)(d * Printer::axisStepsPerMM[Y_AXIS]);
            steps = ( increment < 0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
            PrintLine::moveRelativeDistanceInStepsReal(0,steps,0,0,Printer::maxFeedrate[Y_AXIS],true);
        }
#else
        PrintLine::moveRelativeDistanceInStepsReal(0,increment,0,0,Printer::homingFeedrate[Y_AXIS],true);
#endif
        Commands::printCurrentPosition(PSTR("UI_ACTION_YPOSITION "));
        break;
    case UI_ACTION_ZPOSITION:
        if(!allowMoves) return false;
#if UI_SPEEDDEPENDENT_POSITIONING
        {
            float d = 0.01 * (float)increment * lastNextAccumul;
            if(fabs(d) * 2000 > Printer::maxFeedrate[Z_AXIS] * dtReal)
                d *= Printer::maxFeedrate[Z_AXIS] * dtReal / (2000 * fabs(d));
            long steps = (long)(d * Printer::axisStepsPerMM[Z_AXIS]);
            steps = ( increment<0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
            PrintLine::moveRelativeDistanceInStepsReal(0,0,steps,0,Printer::maxFeedrate[Z_AXIS],true);
        }
#else
        PrintLine::moveRelativeDistanceInStepsReal(0, 0, ((long)increment * Printer::axisStepsPerMM[Z_AXIS]) / 100, 0, Printer::homingFeedrate[Z_AXIS],true);
#endif
        Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION "));
        break;
    case UI_ACTION_XPOSITION_FAST:
        if(!allowMoves) return false;
        PrintLine::moveRelativeDistanceInStepsReal(Printer::axisStepsPerMM[X_AXIS] * increment,0,0,0,Printer::homingFeedrate[X_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_XPOSITION_FAST "));
        break;
    case UI_ACTION_YPOSITION_FAST:
        if(!allowMoves) return false;
        PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[Y_AXIS] * increment,0,0,Printer::homingFeedrate[Y_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_YPOSITION_FAST "));
        break;
    case UI_ACTION_ZPOSITION_FAST:
        if(!allowMoves) return false;
        PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS] * increment,0,Printer::homingFeedrate[Z_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION_FAST "));
        break;
    case UI_ACTION_EPOSITION:
        if(!allowMoves) return false;
        PrintLine::moveRelativeDistanceInSteps(0,0,0,Printer::axisStepsPerMM[E_AXIS]*increment / Printer::extrusionFactor,UI_SET_EXTRUDER_FEEDRATE,true,false);
        Commands::printCurrentPosition(PSTR("UI_ACTION_EPOSITION "));
        break;
//Davinci Specific, Use Load/Unload menu instead       
//#if FEATURE_RETRACTION
//    case UI_ACTION_WIZARD_FILAMENTCHANGE: // filament change is finished
//        Extruder::current->retractDistance(-increment);
//        Commands::waitUntilEndOfAllMoves();
//        Extruder::current->disableCurrentExtruderMotor();
//        break;
//#endif
    case UI_ACTION_ZPOSITION_NOTEST:
        if(!allowMoves) return false;
        Printer::setNoDestinationCheck(true);
#if UI_SPEEDDEPENDENT_POSITIONING
        {
            float d = 0.01 * (float)increment * lastNextAccumul;
            if(fabs(d) * 2000>Printer::maxFeedrate[Z_AXIS] * dtReal)
                d *= Printer::maxFeedrate[Z_AXIS] * dtReal / (2000 * fabs(d));
            long steps = (long)(d * Printer::axisStepsPerMM[Z_AXIS]);
            steps = ( increment < 0 ? RMath::min(steps,(long)increment) : RMath::max(steps,(long)increment));
            PrintLine::moveRelativeDistanceInStepsReal(0, 0, steps, 0, Printer::maxFeedrate[Z_AXIS], true);
        }
#else
        PrintLine::moveRelativeDistanceInStepsReal(0, 0, increment, 0, Printer::homingFeedrate[Z_AXIS], true);
#endif
        Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION_NOTEST "));
        Printer::setNoDestinationCheck(false);
        break;
    case UI_ACTION_ZPOSITION_FAST_NOTEST:
        Printer::setNoDestinationCheck(true);
        PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS]*increment,0,Printer::homingFeedrate[Z_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION_FAST_NOTEST "));
        Printer::setNoDestinationCheck(false);
        break;
    case UI_ACTION_Z_BABYSTEPS:
#if FEATURE_BABYSTEPPING
    {
        previousMillisCmd = HAL::timeInMilliseconds();
        if((abs((int)Printer::zBabystepsMissing + (increment * BABYSTEP_MULTIPLICATOR))) < 127)
        {
            Printer::zBabystepsMissing += increment * BABYSTEP_MULTIPLICATOR;
            zBabySteps += increment * BABYSTEP_MULTIPLICATOR;
        }
    }
#endif
    break;
    case UI_ACTION_HEATED_BED_TEMP:
#if HAVE_HEATED_BED
    {
        int tmp = (int)heatedBedController.targetTemperatureC;
        if(tmp < UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        if(tmp == 0 && increment > 0) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        else tmp += increment;
        if(tmp < UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp > UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        Extruder::setHeatedBedTemperature(tmp);
    }
#endif
    break;

#if NUM_EXTRUDER>2
    case UI_ACTION_EXTRUDER2_TEMP:
#endif
#if NUM_EXTRUDER>1
    case UI_ACTION_EXTRUDER1_TEMP:
#endif
    case UI_ACTION_EXTRUDER0_TEMP:
    {
        int tmp = (int)extruder[action - UI_ACTION_EXTRUDER0_TEMP].tempControl.targetTemperatureC;
        if(tmp < UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        if(tmp == 0 && increment > 0) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        else tmp += increment;
        if(tmp < UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp > UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        Extruder::setTemperatureForExtruder(tmp, action - UI_ACTION_EXTRUDER0_TEMP);
    }
    break;


 //Davinci Specific, special commands
    case UI_ACTION_X_1:
    case UI_ACTION_X_10:
    case UI_ACTION_X_100:
    {
        float tmp_pos=Printer::currentPosition[X_AXIS];
        int istep=1;
        if (action==UI_ACTION_X_10)istep=10;
        if (action==UI_ACTION_X_100)istep=100;
        if (!Printer::isXHomed())//ask for home to secure movement
        {
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_X,UI_TEXT_WARNING_POS_X_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
                    {
                     executeAction(UI_ACTION_HOME_X,true);
                    }
        }
        if(Printer::isXHomed())//check if accepted to home
        {
            if (Extruder::current->id==0)
                {
                    INCREMENT_MIN_MAX(tmp_pos,istep,Printer::xMin-ENDSTOP_X_BACK_ON_HOME,Printer::xMin+Printer::xLength);//this is for default extruder
                }
            else
                {
                    INCREMENT_MIN_MAX(tmp_pos,istep,Printer::xMin-ENDSTOP_X_BACK_ON_HOME+round(Extruder::current->xOffset/Printer::axisStepsPerMM[X_AXIS]),Printer::xMin+Printer::xLength+round(Extruder::current->xOffset/Printer::axisStepsPerMM[X_AXIS]));//this is for default extruder
                }
            if (tmp_pos!=(increment*istep)+Printer::currentPosition[X_AXIS]) break; //we are out of range so do not do
        }
        //we move under control range or not homed
        PrintLine::moveRelativeDistanceInStepsReal(Printer::axisStepsPerMM[X_AXIS]*increment*istep,0,0,0,Printer::homingFeedrate[X_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_XPOSITION "));
    break;
    }

    case UI_ACTION_Y_1:
    case UI_ACTION_Y_10:
    case UI_ACTION_Y_100:
    {
        float tmp_pos=Printer::currentPosition[Y_AXIS];
        int istep=1;
        if (action==UI_ACTION_Y_10)istep=10;
        if (action==UI_ACTION_Y_100)istep=100;
        if (!Printer::isYHomed())//ask for home to secure movement
        {
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_Y,UI_TEXT_WARNING_POS_Y_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
                    {
                     executeAction(UI_ACTION_HOME_Y,true);
                    }
        }
        if(Printer::isYHomed())//check if accepted to home
        {
            if (Extruder::current->id==0)
                {
                    INCREMENT_MIN_MAX(tmp_pos,istep,Printer::yMin-ENDSTOP_Y_BACK_ON_HOME,Printer::yMin+Printer::yLength);//this is for default extruder
                }
            else
                {
                    INCREMENT_MIN_MAX(tmp_pos,istep,Printer::yMin-ENDSTOP_Y_BACK_ON_HOME+round(Extruder::current->yOffset/Printer::axisStepsPerMM[Y_AXIS]),Printer::yMin+Printer::yLength+round(Extruder::current->yOffset/Printer::axisStepsPerMM[Y_AXIS]));//this is for default extruder
                }
            if (tmp_pos!=(increment*istep)+Printer::currentPosition[Y_AXIS]) break; //we are out of range so do not do
        }
        //we move under control range or not homed
        PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[Y_AXIS]*increment*istep,0,0,Printer::homingFeedrate[Y_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_YPOSITION "));
    break;
    }

    case UI_ACTION_Z_0_1:
    case UI_ACTION_Z_1:
    case UI_ACTION_Z_10:
    case UI_ACTION_Z_100:
    {
        float tmp_pos=Printer::currentPosition[Z_AXIS];
        float istep=0.1;
        if (action==UI_ACTION_Z_1)istep=1;
        if (action==UI_ACTION_Z_10)istep=10;
        if (action==UI_ACTION_Z_100)istep=100;
        increment=-increment; //upside down increment to allow keys to follow  Z movement, Up Key make Z going up, down key make Z going down
        if (!Printer::isZHomed())//ask for home to secure movement
        {
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_Z,UI_TEXT_WARNING_POS_Z_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
                    {
                     executeAction(UI_ACTION_HOME_Z,true);
                    }
        }
        if(Printer::isZHomed())//check if accepted to home
        {
            INCREMENT_MIN_MAX(tmp_pos,istep,Printer::zMin-ENDSTOP_Z_BACK_ON_HOME,Printer::zMin+Printer::zLength);

            if (tmp_pos!=(increment*istep)+Printer::currentPosition[Z_AXIS]) break; //we are out of range so do not do
        }
        //we move under control range or not homed
        PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS]*increment*istep,0,Printer::homingFeedrate[Z_AXIS],true);
        Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION "));
    break;
    }
    case UI_ACTION_E_1:
    case UI_ACTION_E_10:
    case UI_ACTION_E_100:
    {
        int istep=1;
        if (action==UI_ACTION_E_10)istep=10;
        if (action==UI_ACTION_E_100)istep=100;
        increment=-increment; //upside down increment to allow keys to follow  filament movement, Up Key make filament going up, down key make filament going down
        if(reportTempsensorError() or Printer::debugDryrun()) break;
        //check temperature
        if(Extruder::current->tempControl.currentTemperatureC<=MIN_EXTRUDER_TEMP)
            {
                if (confirmationDialog(UI_TEXT_WARNING ,UI_TEXT_EXTRUDER_COLD,UI_TEXT_HEAT_EXTRUDER,UI_CONFIRMATION_TYPE_YES_NO,true))
                    {
                    UI_STATUS(UI_TEXT_HEATING_EXTRUDER);
                    Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,Extruder::current->id);
                    }
                else
                    {
                    UI_STATUS(UI_TEXT_EXTRUDER_COLD);
                    }
            executeAction(UI_ACTION_TOP_MENU,true);
             break;
            }
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        //we move
        PrintLine::moveRelativeDistanceInSteps(0,0,0,Printer::axisStepsPerMM[E_AXIS]*increment*istep,UI_SET_EXTRUDER_FEEDRATE,true,false);
        Commands::printCurrentPosition(PSTR("UI_ACTION_EPOSITION "));
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
    break;
    }




 //Davinci Specific, 
    case UI_ACTION_EXT_TEMP_ABS :
    {
         int tmp = EEPROM::ftemp_ext_abs;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        EEPROM::ftemp_ext_abs=tmp;
    }
    break;
    case UI_ACTION_EXT_TEMP_PLA :
    {
         int tmp = EEPROM::ftemp_ext_pla;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
        if(tmp<UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
        EEPROM::ftemp_ext_pla=tmp;
    }
    break;
    case UI_ACTION_BED_TEMP_ABS :
    {
         int tmp = EEPROM::ftemp_bed_abs;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        EEPROM::ftemp_bed_abs=tmp;
    }
    break;
case UI_ACTION_BED_TEMP_PLA :
    {
         int tmp = EEPROM::ftemp_bed_pla;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        tmp+=increment;
        if(tmp==1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
        if(tmp<UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
        else if(tmp>UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
        EEPROM::ftemp_bed_pla=tmp;
    }
    break;
   
    case UI_ACTION_FEEDRATE_MULTIPLY:
    {
        int fr = Printer::feedrateMultiply;
        INCREMENT_MIN_MAX(fr,1,25,500);
        Commands::changeFeedrateMultiply(fr);
    }
    break;
    case UI_ACTION_FLOWRATE_MULTIPLY:
    {
        INCREMENT_MIN_MAX(Printer::extrudeMultiply,1,25,500);
        Commands::changeFlowrateMultiply(Printer::extrudeMultiply);
    }
    break;
    case UI_ACTION_STEPPER_INACTIVE:
    {
        uint8_t inactT = stepperInactiveTime / 60000;
        INCREMENT_MIN_MAX(inactT,1,0,240);
        stepperInactiveTime = inactT * 60000;
//        stepperInactiveTime -= stepperInactiveTime % 1000;
//        INCREMENT_MIN_MAX(stepperInactiveTime,60000UL,0,10080000UL);
    }
//Davinci Specific,save directly to eeprom
        EEPROM:: update(EPR_STEPPER_INACTIVE_TIME,EPR_TYPE_LONG,stepperInactiveTime,0);
    break;
    case UI_ACTION_MAX_INACTIVE:
    {
        uint8_t inactT = maxInactiveTime / 60000;
        INCREMENT_MIN_MAX(inactT,1,0,240);
        maxInactiveTime = inactT * 60000;
//        maxInactiveTime -= maxInactiveTime % 1000;
//        INCREMENT_MIN_MAX(maxInactiveTime,60000UL,0,10080000UL);
    }
//Davinci Specific,save directly to eeprom
         EEPROM:: update(EPR_MAX_INACTIVE_TIME,EPR_TYPE_LONG,maxInactiveTime,0);
    break;
//Davinci Specific, Powersave, light management
      case UI_ACTION_LIGHT_OFF_AFTER:
        {
        uint8_t inactT = EEPROM::timepowersaving / 60000;
        INCREMENT_MIN_MAX(inactT,1,0,240);
        EEPROM::timepowersaving = inactT * 60000;
        }
        //save directly to eeprom
        EEPROM:: update(EPR_POWERSAVE_AFTER_TIME,EPR_TYPE_LONG,EEPROM::timepowersaving,0);
        break;

    case UI_ACTION_PRINT_ACCEL_X:
    case UI_ACTION_PRINT_ACCEL_Y:
    case UI_ACTION_PRINT_ACCEL_Z:
#if DRIVE_SYSTEM!=DELTA
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[action - UI_ACTION_PRINT_ACCEL_X],((action == UI_ACTION_PRINT_ACCEL_Z) ? 1 : 100),0,10000);
#else
        INCREMENT_MIN_MAX(Printer::maxAccelerationMMPerSquareSecond[action - UI_ACTION_PRINT_ACCEL_X],100,0,10000);
#endif
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MOVE_ACCEL_X:
    case UI_ACTION_MOVE_ACCEL_Y:
    case UI_ACTION_MOVE_ACCEL_Z:
#if DRIVE_SYSTEM != DELTA
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[action - UI_ACTION_MOVE_ACCEL_X],((action == UI_ACTION_MOVE_ACCEL_Z) ? 1 : 100),0,10000);
#else
        INCREMENT_MIN_MAX(Printer::maxTravelAccelerationMMPerSquareSecond[action - UI_ACTION_MOVE_ACCEL_X],100,0,10000);
#endif
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_MAX_JERK:
        INCREMENT_MIN_MAX(Printer::maxJerk,0.1,1,99.9);
        break;
#if DRIVE_SYSTEM != DELTA
    case UI_ACTION_MAX_ZJERK:
        INCREMENT_MIN_MAX(Printer::maxZJerk,0.1,0.1,99.9);
        break;
#endif
    case UI_ACTION_HOMING_FEEDRATE_X:
    case UI_ACTION_HOMING_FEEDRATE_Y:
    case UI_ACTION_HOMING_FEEDRATE_Z:
        INCREMENT_MIN_MAX(Printer::homingFeedrate[action - UI_ACTION_HOMING_FEEDRATE_X], 1, 1, 1000);
        break;

    case UI_ACTION_MAX_FEEDRATE_X:
    case UI_ACTION_MAX_FEEDRATE_Y:
    case UI_ACTION_MAX_FEEDRATE_Z:
        INCREMENT_MIN_MAX(Printer::maxFeedrate[action - UI_ACTION_MAX_FEEDRATE_X], 1, 1, 1000);
        break;

    case UI_ACTION_STEPS_X:
    case UI_ACTION_STEPS_Y:
    case UI_ACTION_STEPS_Z:
        INCREMENT_MIN_MAX(Printer::axisStepsPerMM[action - UI_ACTION_STEPS_X], 0.1, 0, 999);
        Printer::updateDerivedParameter();
        break;
    case UI_ACTION_BAUDRATE:
#if EEPROM_MODE != 0
    {
        char p = 0;
        int32_t rate;
        do
        {
            rate = pgm_read_dword(&(baudrates[p]));
            if(rate == baudrate) break;
            p++;
        }
        while(rate != 0);
        if(rate == 0) p -= 2;
        p += increment;
        if(p < 0) p = 0;
        if(p > sizeof(baudrates)/4 - 2) p = sizeof(baudrates)/4 - 2;
//        rate = pgm_read_dword(&(baudrates[p]));
//        if(rate == 0) p--;
        baudrate = pgm_read_dword(&(baudrates[p]));
    }
#endif
    break;
    case UI_ACTION_SERVOPOS:
#if FEATURE_SERVO > 0  && UI_SERVO_CONTROL > 0
        INCREMENT_MIN_MAX(servoPosition, 5, 500, 2500);
        HAL::servoMicroseconds(UI_SERVO_CONTROL - 1, servoPosition, 500);
#endif
        break;
#if TEMP_PID
    case UI_ACTION_PID_PGAIN:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidPGain, 0.1, 0, 200);
        break;
    case UI_ACTION_PID_IGAIN:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidIGain, 0.01, 0, 100);
        if(&Extruder::current->tempControl == currHeaterForSetup)
            Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_PID_DGAIN:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidDGain, 0.1, 0, 200);
        break;
    case UI_ACTION_DRIVE_MIN:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidDriveMin, 1, 1, 255);
        break;
    case UI_ACTION_DRIVE_MAX:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidDriveMax, 1, 1, 255);
        break;
    case UI_ACTION_PID_MAX:
        INCREMENT_MIN_MAX(currHeaterForSetup->pidMax, 1, 1, 255);
        break;
#endif
//Davinci Specific, XYZ Length
    case UI_ACTION_X_LENGTH:
                Printer::xLength = roundf(Printer::xLength );
                INCREMENT_MIN_MAX(Printer::xLength,1,0,250);
                break;
    case UI_ACTION_Y_LENGTH:
                Printer::yLength = roundf(Printer::yLength );
                INCREMENT_MIN_MAX(Printer::yLength,1,0,250);
                break;
    case UI_ACTION_Z_LENGTH:
                Printer::zLength = roundf(Printer::zLength );
                INCREMENT_MIN_MAX(Printer::zLength,1,0,250);
                break;
    case UI_ACTION_X_MIN:
                 Printer::xMin = roundf(Printer::xMin *10)/10;
                INCREMENT_MIN_MAX(Printer::xMin,0.1,-200,250);
                  break;
    case UI_ACTION_Y_MIN:
                Printer::yMin = roundf(Printer::yMin *10)/10;
                INCREMENT_MIN_MAX(Printer::yMin,0.1,-200,250);
                 break;
    case UI_ACTION_Z_MIN:
                Printer::zMin = roundf(Printer::zMin *10)/10;
                INCREMENT_MIN_MAX( Printer::zMin,0.1,-200,250);
               break;
    case UI_ACTION_X_OFFSET:
        INCREMENT_MIN_MAX(Extruder::current->xOffset, 1, -99999, 99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_Y_OFFSET:
        INCREMENT_MIN_MAX(Extruder::current->yOffset, 1, -99999, 99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_STEPS:
        INCREMENT_MIN_MAX(Extruder::current->stepsPerMM, 0.1, 1, 9999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_ACCELERATION:
        INCREMENT_MIN_MAX(Extruder::current->maxAcceleration, 10, 10, 99999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_MAX_FEEDRATE:
        INCREMENT_MIN_MAX(Extruder::current->maxFeedrate, 1, 1, 999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_START_FEEDRATE:
        INCREMENT_MIN_MAX(Extruder::current->maxStartFeedrate, 1, 1, 999);
        Extruder::selectExtruderById(Extruder::current->id);
        break;
    case UI_ACTION_EXTR_HEATMANAGER:
        INCREMENT_MIN_MAX(currHeaterForSetup->heatManager, 1, 0, 3);
        Printer::setMenuMode(MENU_MODE_FULL_PID, currHeaterForSetup->heatManager == 1); // show PIDS only with PID controller selected
        Printer::setMenuMode(MENU_MODE_DEADTIME, currHeaterForSetup->heatManager == 3);
        break;
    case UI_ACTION_EXTR_WATCH_PERIOD:
        INCREMENT_MIN_MAX(Extruder::current->watchPeriod, 1, 0, 999);
        break;
#if RETRACT_DURING_HEATUP
    case UI_ACTION_EXTR_WAIT_RETRACT_TEMP:
        INCREMENT_MIN_MAX(Extruder::current->waitRetractTemperature, 1, 100, UI_SET_MAX_EXTRUDER_TEMP);
        break;
    case UI_ACTION_EXTR_WAIT_RETRACT_UNITS:
        INCREMENT_MIN_MAX(Extruder::current->waitRetractUnits, 1, 0, 99);
        break;
#endif
#if USE_ADVANCE
#if ENABLE_QUADRATIC_ADVANCE
    case UI_ACTION_ADVANCE_K:
        INCREMENT_MIN_MAX(Extruder::current->advanceK, 1, 0, 200);
        break;
#endif
    case UI_ACTION_ADVANCE_L:
        INCREMENT_MIN_MAX(Extruder::current->advanceL, 1, 0, 600);
        break;
#endif
    }
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    ui_autoreturn_time = HAL::timeInMilliseconds() + UI_AUTORETURN_TO_MENU_AFTER;
#endif
 //Davinci Specific, powersave/light management
#if UI_AUTOLIGHTOFF_AFTER!=0
    UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif
#endif
    return true;
}

void UIDisplay::finishAction(int action)
{
}
 //Davinci Specific, Custom dialog
bool UIDisplay::confirmationDialog(char * title,char * line1,char * line2,int type, bool defaultresponse)
{
bool response=defaultresponse;
bool process_it=true;
int previousaction=0;
int tmpmenulevel = menuLevel;
if (menuLevel>3)menuLevel=3;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
bool btmp_autoreturn=benable_autoreturn; //save current value
benable_autoreturn=false;//desactivate no need to test if active or not
#endif
//init dialog strings
col=0;
parse(title,false);
strcpy(uipagedialog[0], printCols);
col=0;
parse(line1,true);
strcpy(uipagedialog[1], printCols);
col=0;
parse(line2,true);
strcpy(uipagedialog[2], printCols);
if (response) strcpy(uipagedialog[3],UI_TEXT_YES_SELECTED); //default for response=true
else strcpy(uipagedialog[3],UI_TEXT_NO_SELECTED); //default for response=false
//push dialog
pushMenu(&ui_menu_confirmation,true);
//ensure last button pressed is not OK to have the dialog closing too fast
while(lastButtonAction==UI_ACTION_OK){
    Commands::checkForPeriodicalActions(true);
    }
delay(500);
//main loop
while (process_it)
    {
    Printer::setMenuMode(MENU_MODE_PRINTING,true);
    Commands::delay_flag_change=0;
    //process critical actions
    Commands::checkForPeriodicalActions(true);
    //be sure button is pressed and not same one
    if (lastButtonAction!=previousaction)
        {
         previousaction=lastButtonAction;
         //wake up light if power saving has been launched
        #if UI_AUTOLIGHTOFF_AFTER!=0
        if (EEPROM::timepowersaving>0)
            {
            UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
            #if CASE_LIGHTS_PIN > 0
            if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                {
                TOGGLE(CASE_LIGHTS_PIN);
                }
            #endif
            #if BADGE_LIGHT_PIN > 0
            if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
                {
                TOGGLE(BADGE_LIGHT_PIN);
                }
            #endif
            #if defined(UI_BACKLIGHT_PIN)
            if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
            #endif
            }
        #endif
        //if button ok we are done
        if (lastButtonAction==UI_ACTION_OK)
            {
            process_it=false;
            }
        //if left key then select Yes
         else if (lastButtonAction==UI_ACTION_BACK)
            {
            strcpy(uipagedialog[3],UI_TEXT_YES_SELECTED);
            response=true;
            }
        //if right key then select No
        else if (lastButtonAction==UI_ACTION_RIGHT_KEY)
            {
            strcpy(uipagedialog[3],UI_TEXT_NO_SELECTED);
            response=false;
            }
        if(previousaction!=0)BEEP_SHORT;
        refreshPage();
        }
    }//end while
 menuLevel=tmpmenulevel;
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
return response;
}
// Actions are events from user input. Depending on the current state, each
// action can behave differently. Other actions do always the same like home, disable extruder etc.
int UIDisplay::executeAction(int action, bool allowMoves)
{
    int ret = 0;
#if UI_HAS_KEYS == 1
//    bool skipBeep = false;
 //Davinci Specific, powermanagement and specific variables
    bool process_it=false;
    int previousaction=0;
    millis_t printedTime;
    millis_t currentTime;
    int load_dir=1;
    int extruderid=0;
    int tmpextruderid=0;
    int step =0;
    int counter;
#if UI_AUTOLIGHTOFF_AFTER!=0
    if (EEPROM::timepowersaving>0)
    {
    UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
    #if CASE_LIGHTS_PIN > 0
    if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
        {
        TOGGLE(CASE_LIGHTS_PIN);
        }
    #endif
    #if BADGE_LIGHT_PIN > 0
    if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::buselight && EEPROM::busebadgelight)
        {
        TOGGLE(BADGE_LIGHT_PIN);
        }
    #endif
    #if defined(UI_BACKLIGHT_PIN)
    if (!(READ(UI_BACKLIGHT_PIN)))WRITE(UI_BACKLIGHT_PIN, HIGH);
    #endif
    }
#endif
    if(action & UI_ACTION_TOPMENU)   // Go to start menu
    {
        action -= UI_ACTION_TOPMENU;
        menuLevel = 0;
 //Davinci Specific, reset all and fancy effect
        activeAction = 0;
        action=0;
        playsound(4000,240);
        playsound(5000,240);
    }
    if(action >= 2000 && action < 3000)
    {
        setStatusP(ui_action);
    }
    else
        switch(action)
        {
	 //Davinci Specific, cheatkey to fast switch menu type
        //fast switch UI without saving
        case UI_ACTION_OK_PREV_RIGHT:
        if (display_mode&ADVANCED_MODE)display_mode=EASY_MODE;
        else display_mode=ADVANCED_MODE;
        playsound(5000,240);
        playsound(5000,240);
        break;
        case UI_ACTION_RIGHT_KEY:
        case UI_ACTION_OK:
            ret = okAction(allowMoves);
//            skipBeep = true; // Prevent double beep
            break;
        case UI_ACTION_BACK:
            if(uid.isWizardActive()) break; // wizards can not exit before finished
            popMenu(false);
            //Davinci Specific, use another way to do wizard
            if( Printer::isMenuModeEx(MENU_MODE_WIZARD) && (menuLevel == 0))
                {
                  uid.executeAction(UI_ACTION_WIZARD_FILAMENTCHANGE_END, true);
                }
            break;
        case UI_ACTION_NEXT:
            if(!nextPreviousAction(1, allowMoves))
                ret = UI_ACTION_NEXT;
            break;
        case UI_ACTION_PREVIOUS:
            if(!nextPreviousAction(-1, allowMoves))
                ret = UI_ACTION_PREVIOUS;
            break;
        case UI_ACTION_MENU_UP:
            if(menuLevel > 0) menuLevel--;
            break;
        case UI_ACTION_TOP_MENU:
			//Davinci Specific, use another way to cancel wizard
			if(uid.isWizardActive()) break; // wizards can not exit before finished
            if( Printer::isMenuModeEx(MENU_MODE_WIZARD))
				{
				uid.executeAction(UI_ACTION_WIZARD_FILAMENTCHANGE_END, true);
				}
            menuLevel = 0;
//Davinci Specific, reset all and fancy effect
            menuPos[0]=0;
            activeAction = 0;
            action=0;
            playsound(4000,240);
            playsound(5000,240);
            break;
        case UI_ACTION_EMERGENCY_STOP:
            Commands::emergencyStop();
            break;

//Davinci Specific, System Version
        case UI_ACTION_VERSION:
            pushMenu(&ui_page_version,true);
            break;

        case UI_ACTION_HOME_ALL:
            {
//Davinci Specific, Home menu with main dislay when doing action
            if(!allowMoves) return UI_ACTION_HOME_ALL;
            int tmpmenu=menuLevel;
            int tmpmenupos=menuPos[menuLevel];
            UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
            menuLevel=0;
            menuPos[0] = 0;
            refreshPage();
            Printer::homeAxis(true, true, true);
            Commands::printCurrentPosition(PSTR("UI_ACTION_HOMEALL "));
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            refreshPage();
            break;
			}
        case UI_ACTION_HOME_X:
            {
//Davinci Specific, Home menu with main dislay when doing action
            if(!allowMoves) return UI_ACTION_HOME_X;
            int tmpmenu=menuLevel;
            int tmpmenupos=menuPos[menuLevel];
            UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
            menuLevel=0;
            menuPos[0] = 0;
            refreshPage();
            Printer::homeAxis(true, false, false);
            Commands::printCurrentPosition(PSTR("UI_ACTION_HOME_X "));
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            refreshPage();
            break;
            }
        case UI_ACTION_HOME_Y:
            {
//Davinci Specific, Home menu with main dislay when doing action
            if(!allowMoves) return UI_ACTION_HOME_Y;
            int tmpmenu=menuLevel;
            int tmpmenupos=menuPos[menuLevel];
            UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
            menuLevel=0;
            menuPos[0] = 0;
            refreshPage();
            Printer::homeAxis(false, true, false);
            Commands::printCurrentPosition(PSTR("UI_ACTION_HOME_Y "));
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            refreshPage();
            break;
            }
        case UI_ACTION_HOME_Z:
            {
//Davinci Specific, Home menu with main dislay when doing action
            if(!allowMoves) return UI_ACTION_HOME_Z;
            int tmpmenu=menuLevel;
            int tmpmenupos=menuPos[menuLevel];
            UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
            menuLevel=0;
            menuPos[0] = 0;
            refreshPage();
            Printer::homeAxis(false, false, true);
            Commands::printCurrentPosition(PSTR("UI_ACTION_HOME_Z "));
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            refreshPage();
            break;
            }
        case UI_ACTION_SET_ORIGIN:
            if(!allowMoves) return UI_ACTION_SET_ORIGIN;
            Printer::setOrigin(0, 0, 0);
            break;
        case UI_ACTION_DEBUG_ECHO:
            Printer::debugLevel ^= 1;
            break;
        case UI_ACTION_DEBUG_INFO:
            Printer::debugLevel ^= 2;
            break;
        case UI_ACTION_DEBUG_ERROR:
            Printer::debugLevel ^= 4;
            break;
        case UI_ACTION_DEBUG_DRYRUN:
            Printer::debugLevel ^= 8;
            if(Printer::debugDryrun())   // simulate movements without printing
            {
                Extruder::setTemperatureForExtruder(0, 0);
#if NUM_EXTRUDER > 1
                Extruder::setTemperatureForExtruder(0, 1);
#endif
#if NUM_EXTRUDER > 2
                Extruder::setTemperatureForExtruder(0, 2);
#endif
#if HAVE_HEATED_BED
                Extruder::setHeatedBedTemperature(0);
#endif
            }
            break;
        case UI_ACTION_POWER:
#if PS_ON_PIN >= 0 // avoid compiler errors when the power supply pin is disabled
            Commands::waitUntilEndOfAllMoves();
            SET_OUTPUT(PS_ON_PIN); //GND
            TOGGLE(PS_ON_PIN);
#endif
            break;
//Davinci Specific, toogle easy/advanced mode for UI
    case UI_ACTION_DISPLAY_MODE:
        if (display_mode&ADVANCED_MODE)display_mode=EASY_MODE;
        else display_mode=ADVANCED_MODE;
        //save directly to eeprom
         EEPROM:: update(EPR_DISPLAY_MODE,EPR_TYPE_BYTE,UIDisplay::display_mode,0);
    break;

    #if FEATURE_BEEPER
    case UI_ACTION_SOUND:
    HAL::enablesound=!HAL::enablesound;
    //save directly to eeprom
    EEPROM:: update(EPR_SOUND_ON,EPR_TYPE_BYTE,HAL::enablesound,0);
    UI_STATUS(UI_TEXT_SOUND_ONOF);
    break;
    #endif
#if UI_AUTOLIGHTOFF_AFTER >0
    case UI_ACTION_KEEP_LIGHT_ON:
        EEPROM::bkeeplighton=!EEPROM::bkeeplighton;
        //save directly to eeprom
        EEPROM:: update(EPR_KEEP_LIGHT_ON,EPR_TYPE_BYTE,EEPROM::bkeeplighton,0);
        UI_STATUS(UI_TEXT_KEEP_LIGHT_ON);
    break;
    case UI_ACTION_TOGGLE_POWERSAVE:
        if (EEPROM::timepowersaving==0) EEPROM::timepowersaving = 1000*60;// move to 1 min
        else if (EEPROM::timepowersaving==(1000 * 60) )EEPROM::timepowersaving = 1000*60*5;// move to 5 min
        else if (EEPROM::timepowersaving==(1000 * 60 * 5)) EEPROM::timepowersaving = 1000*60*15;// move to 15 min
        else if (EEPROM::timepowersaving==(1000 * 60 * 15)) EEPROM::timepowersaving = 1000*60*30;// move to 30 min
        else EEPROM::timepowersaving = 0;// move to off
        //reset counter
        if (EEPROM::timepowersaving>0)UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
        //save directly to eeprom 1 by one as each setting is reloaded by reading eeprom
        EEPROM:: update(EPR_POWERSAVE_AFTER_TIME,EPR_TYPE_LONG,EEPROM::timepowersaving,0);
        //apply to stepper
        stepperInactiveTime=EEPROM::timepowersaving;
        EEPROM:: update(EPR_STEPPER_INACTIVE_TIME,EPR_TYPE_LONG,stepperInactiveTime,0);
        //apply to inactivity timer
        maxInactiveTime=EEPROM::timepowersaving;
        EEPROM:: update(EPR_MAX_INACTIVE_TIME,EPR_TYPE_LONG,maxInactiveTime,0);
        UI_STATUS(UI_TEXT_POWER_SAVE);
    break;
#endif
 #if defined(FIL_SENSOR1_PIN)
     case UI_ACTION_FILAMENT_SENSOR_ONOFF:
         EEPROM::busesensor=!EEPROM::busesensor;
         //save directly to eeprom
        EEPROM:: update(EPR_FIL_SENSOR_ON,EPR_TYPE_BYTE,EEPROM::busesensor,0);
        UI_STATUS(UI_TEXT_FIL_SENSOR_ONOFF);
     break;
#endif
 #if ENABLE_WIFI
     case UI_ACTION_WIFI_ONOFF:
         HAL::bwifion=!HAL::bwifion;
         //save directly to eeprom
        EEPROM:: update(EPR_WIFI_ON,EPR_TYPE_BYTE,HAL::bwifion,0);
        UI_STATUS(UI_TEXT_WIFI_ONOFF);
     break;
#endif
 #if defined(TOP_SENSOR_PIN)
     case UI_ACTION_TOP_SENSOR_ONOFF:
         EEPROM::btopsensor=!EEPROM::btopsensor;
         //save directly to eeprom
        EEPROM:: update(EPR_TOP_SENSOR_ON,EPR_TYPE_BYTE,EEPROM::btopsensor,0);
        UI_STATUS(UI_TEXT_TOP_SENSOR_ONOFF);
     break;
#endif
#if CASE_LIGHTS_PIN >= 0
        case UI_ACTION_LIGHTS_ONOFF:
            TOGGLE(CASE_LIGHTS_PIN);
	    //Davinci Specific, save state to EEPROM
            if (READ(CASE_LIGHTS_PIN))
            EEPROM::buselight=true;
            else
            EEPROM::buselight=false;
            //save directly to eeprom
            EEPROM:: update(EPR_LIGHT_ON,EPR_TYPE_BYTE,EEPROM::buselight,0);
            Printer::reportCaseLightStatus();
            UI_STATUS(UI_TEXT_LIGHTS_ONOFF);
            break;
#endif
//Davinci Specific, save state to EEPROM
#if BADGE_LIGHT_PIN > -1
        case UI_ACTION_BADGE_LIGHT_ONOFF:
            TOGGLE(BADGE_LIGHT_PIN);
            if (READ(BADGE_LIGHT_PIN))
            EEPROM::busebadgelight=true;
            else
            EEPROM::busebadgelight=false;
            //save directly to eeprom
            EEPROM:: update(EPR_BADGE_LIGHT_ON,EPR_TYPE_BYTE,EEPROM::busebadgelight,0);
            UI_STATUS(UI_TEXT_BADGE_LIGHT_ONOFF);
            break;
#endif
//Davinci Specific
case UI_ACTION_LOAD_FAILSAFE:
            EEPROM::restoreEEPROMSettingsFromConfiguration();
            Extruder::selectExtruderById(Extruder::current->id);
            BEEP_LONG;
                    //ask for user if he wants to save to eeprom after loading
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_STORE_TO_EEPROM,UI_TEXT_LOAD_FAILSAFE2))
                    {
                    executeAction(UI_ACTION_STORE_EEPROM,true);
                    }
            else UI_STATUS(UI_TEXT_LOAD_FAILSAFE);
            //skipBeep = true;
            break;
     case UI_ACTION_BED_DOWN:
		{
		if(!allowMoves) return UI_ACTION_BED_DOWN;
		int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
         UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
		 if (!Printer::isZHomed())//ask for home to secure movement
			{
			 if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_HOME_Z,UI_TEXT_WARNING_POS_Z_UNKNOWN,UI_CONFIRMATION_TYPE_YES_NO,true))
				{
				 executeAction(UI_ACTION_HOME_Z,true);
				}
			}
		if(Printer::isZHomed())//check if accepted to home
			{
            menuLevel=0;
            menuPos[0] = 0;
            refreshPage();
			UI_STATUS(UI_TEXT_PLEASE_WAIT);
			Printer::lastCmdPos[Z_AXIS]=Printer::zMin+Printer::zMin+Printer::zLength;
			Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+Printer::zMin+Printer::zLength,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::updateCurrentPosition();
			Commands::waitUntilEndOfAllMoves();
			Commands::printCurrentPosition(PSTR("UI_ACTION_ZPOSITION "));
			UI_STATUS(UI_TEXT_BED_DOWN);
			}
		Printer::setMenuMode(MENU_MODE_PRINTING,false);
		menuLevel=tmpmenu;
		menuPos[menuLevel]=tmpmenupos;
		menu[menuLevel]=tmpmen;
		refreshPage();
		}
		 break;
#if ENABLE_CLEAN_NOZZLE==1
    case UI_ACTION_CLEAN_NOZZLE:
        {//be sure no issue
        if(reportTempsensorError() or Printer::debugDryrun()) break;
        //to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
        int status=STATUS_OK;
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        if(menuLevel>0) menuLevel--;
        pushMenu(&ui_menu_clean_nozzle_page,true);
        process_it=true;
        printedTime = HAL::timeInMilliseconds();
        step=STEP_HEATING;
        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
         switch(step)
         {
         case  STEP_HEATING:
            if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
           #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
           #endif
            step =  STEP_WAIT_FOR_TEMPERATURE;
         break;
         case STEP_WAIT_FOR_TEMPERATURE:

            UI_STATUS(UI_TEXT_HEATING);
            //no need to be extremely accurate so no need stable temperature
            if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
                {
                step = STEP_CLEAN_NOOZLE;
                }
            #if NUM_EXTRUDER==2
            if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_CLEAN_NOOZLE)))
                {
                step = STEP_WAIT_FOR_TEMPERATURE;
                }
            #endif
         break;
         case STEP_CLEAN_NOOZLE:
            //clean
            Printer::cleanNozzle(false);
            //move to a position to let user to clean manually
            #if DAVINCI==1 //be sure we cannot hit cleaner on 1.0
            if(Printer::currentPosition[Y_AXIS]<=20)
                {
                Printer::moveToReal(0,0,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);//go 0,0 or xMin,yMin ?
                Printer::moveToReal(0,20,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
                }
            #endif
            Printer::moveToReal(Printer::xLength/2,Printer::yLength-20,150,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
            Commands::waitUntilEndOfAllMoves();
            step =STEP_WAIT_FOR_OK;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
         break;
         case STEP_WAIT_FOR_OK:
         UI_STATUS(UI_TEXT_WAIT_FOR_OK);
         //just need to wait for key to be pressed
         break;
         }
         //check what key is pressed
         if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
             if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_WAIT_FOR_OK))
                {//we are done
                process_it=false;
                playsound(5000,240);
                playsound(3000,240);
                Printer::homeAxis(true,true,false);
                }
             if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
                {
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_CLEANING_NOZZLE))
                    {
                    UI_STATUS(UI_TEXT_CANCELED);
                    status=STATUS_CANCEL;
                    process_it=false;
                    }
                else
                    {//we continue as before
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_clean_nozzle_page,true);
                    }
                delay(100);
                }
             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
                {
                TOGGLE(BADGE_LIGHT_PIN);
                }
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
        }
        //cool down if necessary
        Extruder::setTemperatureForExtruder(extrudertarget1,0);
        #if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
        #endif
        if(status==STATUS_OK)
            {
             UI_STATUS(UI_TEXT_PRINTER_READY);
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            }
        else if (status==STATUS_CANCEL)
            {
            while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions(true);
            UI_STATUS(UI_TEXT_CANCELED);
            menuLevel=0;
            }
        refreshPage();
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        break;
        }
#endif

#if ENABLE_CLEAN_DRIPBOX==1
    case UI_ACTION_CLEAN_DRIPBOX:
        {
        process_it=true;
        printedTime = HAL::timeInMilliseconds();
        step=STEP_CLEAN_DRIPBOX;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        menuLevel=0;
        refreshPage();
        //need to home if not
        if(!Printer::isHomed()) Printer::homeAxis(true,true,true);
        else
            {//put proper position in case position has been manualy changed no need to home Z as cannot be manualy changed and in case of something on plate it could be catastrophic
                Printer::homeAxis(true,true,false);
            }
        UI_STATUS(UI_TEXT_PREPARING);
        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        currentTime = HAL::timeInMilliseconds();
         switch(step)
         {
         case STEP_CLEAN_DRIPBOX:
            //move to a position to let user to clean manually
            #if DAVINCI==1 //be sure we cannot hit cleaner on 1.0
            if(Printer::currentPosition[Y_AXIS]<=20)
                {
                Printer::moveToReal(Printer::xMin,Printer::yMin,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);//go 0,0 or xMin,yMin ?
                 PrintLine::moveRelativeDistanceInStepsReal(0,Printer::axisStepsPerMM[Y_AXIS]*20,0,0,Printer::homingFeedrate[Y_AXIS],true);
                }
            #endif
            Commands::waitUntilEndOfAllMoves();
            PrintLine::moveRelativeDistanceInStepsReal(0,0,Printer::axisStepsPerMM[Z_AXIS]*10,0,Printer::homingFeedrate[Z_AXIS],true);
            Printer::moveToReal(Printer::xLength/2,Printer::yLength-20,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::maxFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,150,IGNORE_COORDINATE,Printer::maxFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
            step =STEP_CLEAN_DRIPBOX_WAIT_FOR_OK;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_clean_dripbox_page,true);
         break;
         case STEP_CLEAN_DRIPBOX_WAIT_FOR_OK:
         UI_STATUS(UI_TEXT_WAIT_FOR_OK);
         //just need to wait for key to be pressed
         break;
         }
         //check what key is pressed
         if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
             if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_CLEAN_DRIPBOX_WAIT_FOR_OK))
                {//we are done
                process_it=false;
                playsound(5000,240);
                playsound(3000,240);
                menuLevel=0;
                refreshPage();
                Printer::homeAxis(true,true,true);
                }
            //there is no step that user can cancel but keep it if function become more complex
             /*if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
                {
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_CLEANING_DRIPBOX))
                    {
                    UI_STATUS(UI_TEXT_CANCELED);
                    process_it=false;
                    }
                else
                    {//we continue as before
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_clean_dripbox_page,true);
                    }
                delay(100);
                }*/
             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
					{
					TOGGLE(BADGE_LIGHT_PIN);
					}
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
        }
        menuLevel=tmpmenu;
        menuPos[menuLevel]=tmpmenupos;
        menu[menuLevel]=tmpmen;
        refreshPage();
        UI_STATUS(UI_TEXT_PRINTER_READY);
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        break;
        }
#endif

        case UI_ACTION_NO_FILAMENT:
        {
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        process_it=true;
        //if printing from SD pause
        if(sd.sdmode )sd.pausePrint(true);
        else //if printing from Host, request a pause
            {
            Com::printFLN(PSTR("RequestPause: No Filament!"));
            Commands::waitUntilEndOfAllMoves();
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::currentPositionSteps[Z_AXIS]+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
			Printer::moveToReal(Printer::xMin,Printer::yMin,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
            }
        
        //to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        playsound(3000,240);
        playsound(4000,240);
        playsound(5000,240);
        if(menuLevel>0) menuLevel--;
        pushMenu(&ui_no_filament_box,true);
        previousaction=0;
        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
            if (lastButtonAction==UI_ACTION_OK)
                {
                    process_it=false;
                }
            delay(100);
                }
             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
					{
					TOGGLE(BADGE_LIGHT_PIN);
					}
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        //menuLevel=0;
        //pushMenu(&ui_menu_load_unload,true);
        menuLevel=tmpmenu;
        menuPos[menuLevel]=tmpmenupos;
        menu[menuLevel]=tmpmen;
        if(menuLevel>0) menuLevel--;
        pushMenu(&ui_menu_load_unload,true);
        break;
        }

        case UI_ACTION_LOAD_EXTRUDER_0:
        case UI_ACTION_UNLOAD_EXTRUDER_0:
        #if NUM_EXTRUDER > 1
        case UI_ACTION_LOAD_EXTRUDER_1:
        case UI_ACTION_UNLOAD_EXTRUDER_1:
        #endif
        {
        int status=STATUS_OK;
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        float xpos,ypos;
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
#if NUM_EXTRUDER > 1
		//save current extruder to restore 
		tmpextruderid=Extruder::current->id;
#endif
		//check if homed and home if not
		if (!Printer::isXHomed())executeAction(UI_ACTION_HOME_X,true);
		if (!Printer::isYHomed())executeAction(UI_ACTION_HOME_Y,true);
        if (action== UI_ACTION_LOAD_EXTRUDER_0)
            {
            load_dir=1;
            extruderid=0;
            #if NUM_EXTRUDER == 1
            xpos=Printer::xMin;
            ypos=Printer::yMin;
            #else
            xpos=Printer::xMin+Printer::xLength;
            ypos=Printer::yMin;
            #endif
            }
        else if (action== UI_ACTION_UNLOAD_EXTRUDER_0)
            {
            load_dir=-1;
            extruderid=0;
            #if NUM_EXTRUDER == 1
            xpos=Printer::xMin;
            ypos=Printer::yMin;
            #else
            xpos=Printer::xMin+Printer::xLength;
            ypos=Printer::yMin;
            #endif
            }
        #if NUM_EXTRUDER > 1
        else if (action== UI_ACTION_LOAD_EXTRUDER_1)
            {
            load_dir=1;
            extruderid=1;
            xpos=Printer::xMin;
            ypos=Printer::yMin;
            }
        else if (action== UI_ACTION_UNLOAD_EXTRUDER_1)
            {
            load_dir=-1;
            extruderid=1;
            xpos=Printer::xMin;
            ypos=Printer::yMin;
            }
        #endif
#if NUM_EXTRUDER > 1
        Extruder::selectExtruderById(0,true);
#endif
        Printer::moveToReal(xpos,ypos,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
#if NUM_EXTRUDER > 1
        Extruder::selectExtruderById(extruderid,false);
#endif
         //save current target temp
        float extrudertarget=extruder[extruderid].tempControl.targetTemperatureC;
        //be sure no issue
        if(reportTempsensorError() or Printer::debugDryrun()) break;
        UI_STATUS(UI_TEXT_HEATING);
         if(menuLevel>0) menuLevel--;
        pushMenu(&ui_menu_heatextruder_page,true);
        process_it=true;
        printedTime = HAL::timeInMilliseconds();
        step=STEP_EXT_HEATING;
        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
         switch(step)
         {
         case STEP_EXT_HEATING:
           if (extruder[extruderid].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,extruderid);
            step =  STEP_EXT_WAIT_FOR_TEMPERATURE;
         break;
         case STEP_EXT_WAIT_FOR_TEMPERATURE:
            UI_STATUS(UI_TEXT_HEATING);
            //no need to be extremely accurate so no need stable temperature
            if(abs(extruder[extruderid].tempControl.currentTemperatureC- extruder[extruderid].tempControl.targetTemperatureC)<2)
                {
                step = STEP_EXT_ASK_FOR_FILAMENT;
                playsound(3000,240);
                playsound(4000,240);
                playsound(5000,240);
                UI_STATUS(UI_TEXT_WAIT_FILAMENT);
                }
            counter=0;
         break;
         case STEP_EXT_ASK_FOR_FILAMENT:
         if (load_dir==-1) //no need to ask
            {
            step=STEP_EXT_LOAD_UNLOAD;
            }
        else
                {
                if (extruderid==0)//filament sensor override pushing ok button to start if filament detected, if not user still can push Ok to start
                    {
                        #if defined(FIL_SENSOR1_PIN)
                            if(EEPROM::busesensor && !READ(FIL_SENSOR1_PIN))step=STEP_EXT_LOAD_UNLOAD;
                        #endif
                    }
                else
                    {
                        #if defined(FIL_SENSOR2_PIN)
                            if(EEPROM::busesensor &&!READ(FIL_SENSOR2_PIN))step=STEP_EXT_LOAD_UNLOAD;
                        #endif
                    }
                }
        if((step==STEP_EXT_LOAD_UNLOAD)&&(load_dir==1))
            {
                playsound(5000,240);
                playsound(3000,240);
                UI_STATUS(UI_TEXT_PUSH_FILAMENT);
            }
        //ok key is managed in key section so if wait for press ok - just do nothing
         break;
         case STEP_EXT_LOAD_UNLOAD:
            if(!PrintLine::hasLines())
            {
                //load or unload
                counter++;
                if (load_dir==-1)
                    {
                    UI_STATUS(UI_TEXT_UNLOADING_FILAMENT);
                    PrintLine::moveRelativeDistanceInSteps(0,0,0,load_dir * Printer::axisStepsPerMM[E_AXIS],4,false,false);
                    if (extruderid==0)//filament sensor override to stop earlier
                    {
                        #if defined(FIL_SENSOR1_PIN)
                            if(EEPROM::busesensor &&READ(FIL_SENSOR1_PIN))
                                {
                                process_it=false;
                                }
                        #endif
                    }
                else
                    {
                        #if defined(FIL_SENSOR2_PIN)
                            if(EEPROM::busesensor &&READ(FIL_SENSOR2_PIN))
                                {
                                process_it=false;
                                }
                        #endif
                    }
                    }
                else
                    {
                    UI_STATUS(UI_TEXT_LOADING_FILAMENT);
                    PrintLine::moveRelativeDistanceInSteps(0,0,0,load_dir * Printer::axisStepsPerMM[E_AXIS],2,false,false);
                    }
                if (counter>=60)step = STEP_EXT_ASK_CONTINUE;
            }
            break;
         case STEP_EXT_ASK_CONTINUE:
            if(!PrintLine::hasLines())
            {
				#if FEATURE_RETRACTION
				if (load_dir==1)
					Extruder::current->retractDistance(EEPROM_FLOAT(RETRACTION_LENGTH));
				#endif
                //ask to redo or stop
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CONTINUE_ACTION,UI_TEXT_PUSH_FILAMENT,UI_CONFIRMATION_TYPE_YES_NO,true))
                        {
                         if(menuLevel>0) menuLevel--;
                        pushMenu(&ui_menu_heatextruder_page,true);
                        counter=0;
                        step =STEP_EXT_LOAD_UNLOAD;
                        }
                    else
                        {//we end
                        process_it=false;
                        }
                    delay(100);
            }
         break;
         }
         //check what key is pressed
         if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
            if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_EXT_ASK_FOR_FILAMENT))
                {//we can continue
                step=STEP_EXT_LOAD_UNLOAD;
                playsound(5000,240);
                playsound(3000,240);
                UI_STATUS(UI_TEXT_PUSH_FILAMENT);
                }
             if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
                {
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_LOADUNLOAD_FILAMENT))
                    {
                    UI_STATUS(UI_TEXT_CANCELED);
                    status=STATUS_CANCEL;
                    process_it=false;
                    }
                else
                    {//we continue as before
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_heatextruder_page,true);
                    }
                delay(100);
                }
             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
					{
					TOGGLE(BADGE_LIGHT_PIN);
					}
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
        }

        //cool down if necessary
        Extruder::setTemperatureForExtruder(extrudertarget,extruderid);
#if NUM_EXTRUDER > 1
		Printer::moveToReal(Printer::xMin,Printer::yMin,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
        Extruder::selectExtruderById(tmpextruderid,true);
#endif
        if(status==STATUS_OK)
            {
            UI_STATUS(UI_TEXT_PRINTER_READY);
            }
        else if (status==STATUS_CANCEL)
            {
			menuLevel=0;
            while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions(true);
            UI_STATUS(UI_TEXT_CANCELED);
            refreshPage();
            }
        menuLevel=tmpmenu;
        menuPos[menuLevel]=tmpmenupos;
        menu[menuLevel]=tmpmen;
        refreshPage();
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        break;
        }
#if FEATURE_AUTOLEVEL
        case UI_ACTION_AUTOLEVEL:
        {
        Z_probe[0]=-1000;
        Z_probe[1]=-1000;
        Z_probe[2]=-1000;
        //be sure no issue
        if(reportTempsensorError() or Printer::debugDryrun()) break;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
        #if ENABLE_CLEAN_NOZZLE==1
        //ask for user if he wants to clean nozzle and plate
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_CLEAN1,UI_TEXT_CLEAN2))
                    {
                    //heat extruders first to keep them hot
                    if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
                   #if NUM_EXTRUDER>1
                   if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                    Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
                   #endif
                    executeAction(UI_ACTION_CLEAN_NOZZLE,true);
                    }

         #endif
         if(menuLevel>0) menuLevel--;
         pushMenu(&ui_menu_autolevel_page,true);
        UI_STATUS(UI_TEXT_HEATING);
        process_it=true;
        printedTime = HAL::timeInMilliseconds();
        step=STEP_AUTOLEVEL_HEATING;
        float h1,h2,h3;
        float oldFeedrate = Printer::feedrate;
        int xypoint=1;
        float z = 0;
        int32_t sum ,probeDepth;
        int32_t lastCorrection;
        float distance;
        int status=STATUS_OK;
        float currentzMin = Printer::zMin;

        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
         switch(step)
         {
         case STEP_AUTOLEVEL_HEATING:
            if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
           #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
           #endif
           #if HAVE_HEATED_BED==true
           if (heatedBedController.targetTemperatureC<EEPROM::ftemp_bed_abs)
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
           #endif
           step =  STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
         break;
         case STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE:
            UI_STATUS(UI_TEXT_HEATING);
            //no need to be extremely accurate so no need stable temperature
            if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
                {
                step = STEP_ZPROBE_SCRIPT;
                }
            #if NUM_EXTRUDER==2
            if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_ZPROBE_SCRIPT)))
                {
                step = STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
                }
            #endif
            #if HAVE_HEATED_BED==true
            if(!((abs(heatedBedController.currentTemperatureC-heatedBedController.targetTemperatureC)<2) && (step == STEP_ZPROBE_SCRIPT)))
                {
                step = STEP_AUTOLEVEL_WAIT_FOR_TEMPERATURE;
                }
            #endif
         break;
         case STEP_ZPROBE_SCRIPT:
            //Home first
            Printer::homeAxis(true,true,true);
            //then put bed +10mm
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
            Commands::waitUntilEndOfAllMoves();
            //Startup script
            GCode::executeFString(Com::tZProbeStartScript);
            step = STEP_AUTOLEVEL_START;
            break;
         case STEP_AUTOLEVEL_START:
            Printer::coordinateOffset[0] = Printer::coordinateOffset[1] = Printer::coordinateOffset[2] = 0;
            Printer::setAutolevelActive(false); // iterate
            step = STEP_AUTOLEVEL_MOVE;
            Z_probe[0]=-1000;
            Z_probe[1]=-1000;
            Z_probe[2]=-1000;
            if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_autolevel_results_page,true);
            break;

        case STEP_AUTOLEVEL_MOVE:
            UI_STATUS(UI_TEXT_ZPOSITION);
            if (xypoint==1)Printer::moveTo(EEPROM::zProbeX1(),EEPROM::zProbeY1(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());

            else if (xypoint==2) Printer::moveTo(EEPROM::zProbeX2(),EEPROM::zProbeY2(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());
            else Printer::moveTo(EEPROM::zProbeX3(),EEPROM::zProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,EEPROM::zProbeXYSpeed());
            step = STEP_AUTOLEVEL_DO_ZPROB;
            break;
        case STEP_AUTOLEVEL_DO_ZPROB:
            {
            UI_STATUS(UI_TEXT_ZPROBING);
            sum = 0;
            probeDepth = (int32_t)((float)Z_PROBE_SWITCHING_DISTANCE*Printer::axisStepsPerMM[Z_AXIS]);
            lastCorrection = Printer::currentPositionSteps[Z_AXIS];
            #if NONLINEAR_SYSTEM
                Printer::realDeltaPositionSteps[Z_AXIS] = Printer::currentDeltaPositionSteps[Z_AXIS]; // update real
            #endif
            probeDepth = 2 * (Printer::zMaxSteps - Printer::zMinSteps);
            Printer::stepsRemainingAtZHit = -1;
            Printer::setZProbingActive(true);
            PrintLine::moveRelativeDistanceInSteps(0,0,-probeDepth,0,EEPROM::zProbeSpeed(),true,true);
            if(Printer::stepsRemainingAtZHit<0)
                {
                Com::printErrorFLN(Com::tZProbeFailed);
                UI_STATUS(UI_TEXT_Z_PROBE_FAILED);
                 Z_probe[xypoint-1]=-2000;
                process_it=false;
                Printer::setZProbingActive(false);
                status=STATUS_FAIL;
                uid.refreshPage();
                PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
                playsound(3000,240);
                playsound(5000,240);
                playsound(3000,240);
                break;
                }
            Printer::setZProbingActive(false);
            Printer::currentPositionSteps[Z_AXIS] += Printer::stepsRemainingAtZHit; // now current position is correct
            sum += lastCorrection - Printer::currentPositionSteps[Z_AXIS];
            distance = (float)sum * Printer::invAxisStepsPerMM[Z_AXIS] / (float)1.00 + EEPROM::zProbeHeight();
            Com::printF(Com::tZProbe,distance);
            Com::printF(Com::tSpaceXColon,Printer::realXPosition());
            Com::printFLN(Com::tSpaceYColon,Printer::realYPosition());
            uid.setStatusP(Com::tHitZProbe);
             Z_probe[xypoint-1]=distance;
            uid.refreshPage();
            // Go back to start position
            PrintLine::moveRelativeDistanceInSteps(0,0,lastCorrection-Printer::currentPositionSteps[Z_AXIS],0,EEPROM::zProbeSpeed(),true,false);
            if(xypoint==1)
                {
                xypoint++;
                step = STEP_AUTOLEVEL_MOVE;
                h1=distance;
                }
            else if(xypoint==2)
                {
                xypoint++;
                step = STEP_AUTOLEVEL_MOVE;
                h2=distance;
                }
            else{
                h3=distance;
                Printer::buildTransformationMatrix(h1,h2,h3);
                z = -((Printer::autolevelTransformation[0]*Printer::autolevelTransformation[5]-
                         Printer::autolevelTransformation[2]*Printer::autolevelTransformation[3])*
                        (float)Printer::currentPositionSteps[Y_AXIS]*Printer::invAxisStepsPerMM[Y_AXIS]+
                        (Printer::autolevelTransformation[2]*Printer::autolevelTransformation[4]-
                         Printer::autolevelTransformation[1]*Printer::autolevelTransformation[5])*
                        (float)Printer::currentPositionSteps[X_AXIS]*Printer::invAxisStepsPerMM[X_AXIS])/
                      (Printer::autolevelTransformation[1]*Printer::autolevelTransformation[3]-Printer::autolevelTransformation[0]*Printer::autolevelTransformation[4]);

                Printer::zMin = 0;
                step = STEP_AUTOLEVEL_RESULTS;
                playsound(3000,240);
                playsound(4000,240);
                playsound(5000,240);
                UI_STATUS(UI_TEXT_WAIT_OK);
                }
            break;
            }
        case STEP_AUTOLEVEL_RESULTS:
        //just wait Ok is pushed
        break;
         case STEP_AUTOLEVEL_SAVE_RESULTS:
                playsound(3000,240);
                playsound(4000,240);
                playsound(5000,240);
            if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_SAVE,UI_TEXT_AUTOLEVEL_MATRIX))
                {
                //save matrix to memory as individual save to eeprom erase all
                float tmpmatrix[9];
                for(uint8_t ti=0; ti<9; ti++)tmpmatrix[ti]=Printer::autolevelTransformation[ti];
                //save to eeprom
                EEPROM:: update(EPR_Z_HOME_OFFSET,EPR_TYPE_FLOAT,0,Printer::zMin);
                //Set autolevel to false for saving
                 EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,0,0);
                 Printer::setAutolevelActive(false);
                //save Matrix
                 for(uint8_t ti=0; ti<9; ti++)EEPROM:: update(EPR_AUTOLEVEL_MATRIX + (((int)ti) << 2),EPR_TYPE_FLOAT,0,tmpmatrix[ti]);
                 //Set autolevel to true
                 EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,1,0);
                 Printer::setAutolevelActive(true);
                 for(uint8_t ti=0; ti<9; ti++)Printer::autolevelTransformation[ti]=tmpmatrix[ti];
                }
            else{
                //back to original value
                Printer::zMin=currentzMin;
                }
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_REDO_ACTION,UI_TEXT_AUTOLEVEL))
                    {
                    if(menuLevel>0) menuLevel--;
                     pushMenu(&ui_menu_autolevel_page,true);
                    xypoint=1;
                    z = 0;
                    currentzMin = Printer::zMin;
                    Printer::setAutolevelActive(true);
                    Printer::updateDerivedParameter();
                    Printer::updateCurrentPosition(true);
                    step=STEP_ZPROBE_SCRIPT;
                    }
            else    process_it=false;
            break;
         }
         //check what key is pressed
         if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
             if (lastButtonAction==UI_ACTION_OK  && step==STEP_AUTOLEVEL_RESULTS)
                {
                    step=STEP_AUTOLEVEL_SAVE_RESULTS;
                }
             if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
                {
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_AUTOLEVEL))
                    {
                    Printer::setZProbingActive(false);
                    status=STATUS_CANCEL;
                    PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
                    UI_STATUS(UI_TEXT_CANCELED);
                    process_it=false;
                    }
                else
                    {//we continue as before
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_autolevel_page,true);
                    }
                delay(100);
                }
             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
					{
					TOGGLE(BADGE_LIGHT_PIN);
					}
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
        }
        //cool down if necessary
        Extruder::setTemperatureForExtruder(extrudertarget1,0);
        #if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
        #endif
        #if HAVE_HEATED_BED==true
          Extruder::setHeatedBedTemperature(bedtarget);
        #endif
        Printer::setAutolevelActive(true);
        Printer::updateDerivedParameter();
        Printer::updateCurrentPosition(true);
        //home again
        Printer::homeAxis(true,true,true);
        Printer::feedrate = oldFeedrate;
        if(status==STATUS_OK)
            {
            UI_STATUS(UI_TEXT_PRINTER_READY);
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            refreshPage();
            }
        else if (status==STATUS_FAIL)
            {
            while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions(true);
            UI_STATUS(UI_TEXT_Z_PROBE_FAILED);
            menuLevel=0;
            }
        else if (status==STATUS_CANCEL)
            {
            while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions(true);
            UI_STATUS(UI_TEXT_CANCELED);
            menuLevel=0;
            }
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        refreshPage();
        break;
        }
#endif

        case UI_ACTION_MANUAL_LEVEL:
        {
        //be sure no issue
        if(reportTempsensorError() or Printer::debugDryrun()) break;
//to be sure no return menu
#if UI_AUTORETURN_TO_MENU_AFTER!=0
        bool btmp_autoreturn=benable_autoreturn; //save current value
        benable_autoreturn=false;//desactivate no need to test if active or not
#endif
        int tmpmenu=menuLevel;
        int tmpmenupos=menuPos[menuLevel];
        UIMenu *tmpmen = (UIMenu*)menu[menuLevel];
        //save current target temp
        float extrudertarget1=extruder[0].tempControl.targetTemperatureC;
        #if NUM_EXTRUDER>1
        float extrudertarget2=extruder[1].tempControl.targetTemperatureC;
        #endif
        #if HAVE_HEATED_BED==true
        float bedtarget=heatedBedController.targetTemperatureC;
        #endif
        #if ENABLE_CLEAN_NOZZLE==1
        //ask for user if he wants to clean nozzle and plate
            if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_CLEAN1,UI_TEXT_CLEAN2))
                    {
                      //heat extruders first to keep them hot
                    if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                   Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
                   #if NUM_EXTRUDER>1
                   if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
                    Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
                   #endif
                    executeAction(UI_ACTION_CLEAN_NOZZLE,true);
                    }
         #endif
         if(menuLevel>0) menuLevel--;
         pushMenu(&ui_menu_manual_level_heat_page,true);
        //Home first
        Printer::homeAxis(true,true,true);
        //then put bed +10mm
        Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[0]);
        Commands::waitUntilEndOfAllMoves();
        UI_STATUS(UI_TEXT_HEATING);
        process_it=true;
        printedTime = HAL::timeInMilliseconds();
        step=STEP_MANUAL_LEVEL_HEATING;
        int status=STATUS_OK;

        while (process_it)
        {
        Printer::setMenuMode(MENU_MODE_PRINTING,true);
        Commands::delay_flag_change=0;
        Commands::checkForPeriodicalActions(true);
        currentTime = HAL::timeInMilliseconds();
        if( (currentTime - printedTime) > 1000 )   //Print Temp Reading every 1 second while heating up.
            {
            Commands::printTemperatures();
            printedTime = currentTime;
           }
         switch(step)
         {
         case STEP_MANUAL_LEVEL_HEATING:
           if (extruder[0].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
           Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
           #if NUM_EXTRUDER>1
           if (extruder[1].tempControl.targetTemperatureC<EEPROM::ftemp_ext_abs)
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
           #endif
           #if HAVE_HEATED_BED==true
           if (heatedBedController.targetTemperatureC<EEPROM::ftemp_bed_abs)
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
           #endif
           step =  STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
         break;
         case STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE:
            UI_STATUS(UI_TEXT_HEATING);
            //no need to be extremely accurate so no need stable temperature
            if(abs(extruder[0].tempControl.currentTemperatureC- extruder[0].tempControl.targetTemperatureC)<2)
                {
                step = STEP_MANUAL_LEVEL_PAGE0;
                }
            #if NUM_EXTRUDER==2
            if(!((abs(extruder[1].tempControl.currentTemperatureC- extruder[1].tempControl.targetTemperatureC)<2) && (step == STEP_MANUAL_LEVEL_PAGE0)))
                {
                step = STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
                }
            #endif
            #if HAVE_HEATED_BED==true
            if(!((abs(heatedBedController.currentTemperatureC-heatedBedController.targetTemperatureC)<2) && (step == STEP_MANUAL_LEVEL_PAGE0)))
                {
                step = STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE;
                }
            #endif
         break;
        case  STEP_MANUAL_LEVEL_PAGE0:
            if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_page1,true);
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
          step =STEP_MANUAL_LEVEL_PAGE1;
          break;
         case  STEP_MANUAL_LEVEL_POINT_1:
            //go to point 1
            if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_heat_page,true);
            UI_STATUS(UI_TEXT_MOVING);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(EEPROM::ManualProbeX1() ,EEPROM::ManualProbeY1(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,0,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
             if(menuLevel>0) menuLevel--;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_manual_level_page6,true);
            step =STEP_MANUAL_LEVEL_PAGE6;
            break;
         case  STEP_MANUAL_LEVEL_POINT_2:
            //go to point 2
            if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_heat_page,true);
            UI_STATUS(UI_TEXT_MOVING);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(EEPROM::ManualProbeX2() ,EEPROM::ManualProbeY2(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,0,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
             if(menuLevel>0) menuLevel--;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_manual_level_page7,true);
            step =STEP_MANUAL_LEVEL_PAGE7;
            break;
         case  STEP_MANUAL_LEVEL_POINT_3:
            //go to point 3
             if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_heat_page,true);
            UI_STATUS(UI_TEXT_MOVING);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(EEPROM::ManualProbeX3() ,EEPROM::ManualProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,0,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
             if(menuLevel>0) menuLevel--;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_manual_level_page8,true);
            step =STEP_MANUAL_LEVEL_PAGE8;
            break;
         case  STEP_MANUAL_LEVEL_POINT_4:
            //go to point 4
             if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_heat_page,true);
            UI_STATUS(UI_TEXT_MOVING);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(EEPROM::ManualProbeX4() ,EEPROM::ManualProbeY4(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,0,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
             if(menuLevel>0) menuLevel--;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_manual_level_page9,true);
            step =STEP_MANUAL_LEVEL_PAGE9;
            break;
          case  STEP_MANUAL_LEVEL_POINT_5:
            //go to point 5
             if(menuLevel>0) menuLevel--;
            pushMenu(&ui_menu_manual_level_heat_page,true);
            UI_STATUS(UI_TEXT_MOVING);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(EEPROM::ManualProbeX2() ,EEPROM::ManualProbeY3(),IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
            Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,0,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
            Commands::waitUntilEndOfAllMoves();
             if(menuLevel>0) menuLevel--;
            playsound(3000,240);
            playsound(4000,240);
            playsound(5000,240);
            pushMenu(&ui_menu_manual_level_page10,true);
            step =STEP_MANUAL_LEVEL_PAGE10;
            break;
        }
         //check what key is pressed
         if (previousaction!=lastButtonAction)
            {
            previousaction=lastButtonAction;
            if(previousaction!=0)BEEP_SHORT;
            if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE2))
            {
                step=STEP_MANUAL_LEVEL_PAGE3;
             if(menuLevel>0) menuLevel--;
                pushMenu(&ui_menu_manual_level_page3,true);
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE1))
            {
                step=STEP_MANUAL_LEVEL_PAGE2;
                if(menuLevel>0) menuLevel--;
                pushMenu(&ui_menu_manual_level_page2,true);
            }

            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE3))
            {
                step=STEP_MANUAL_LEVEL_PAGE4;
                 if(menuLevel>0) menuLevel--;
                pushMenu(&ui_menu_manual_level_page4,true);
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE4))
            {
                step=STEP_MANUAL_LEVEL_PAGE5;
            if(menuLevel>0) menuLevel--;
                pushMenu(&ui_menu_manual_level_page5,true);
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE5))
            {
                step=STEP_MANUAL_LEVEL_POINT_1;
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE6))
            {
                step=STEP_MANUAL_LEVEL_POINT_2;
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE7))
            {
                step=STEP_MANUAL_LEVEL_POINT_3;
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE8))
            {
                step=STEP_MANUAL_LEVEL_POINT_4;
            }
            if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE9))
            {
                step=STEP_MANUAL_LEVEL_POINT_5;
            }
            else if ((lastButtonAction==UI_ACTION_OK) &&(step==STEP_MANUAL_LEVEL_PAGE10))
            {
                playsound(5000,240);
                playsound(3000,240);
                if (confirmationDialog(UI_TEXT_DO_YOU ,UI_TEXT_REDO_ACTION,UI_TEXT_MANUAL_LEVEL))
                    {
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_manual_level_heat_page,true);
                    step=STEP_MANUAL_LEVEL_POINT_1;
                    }
                else
                    {
                     if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_manual_level_heat_page,true);
                    Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::zMin+10,IGNORE_COORDINATE,Printer::homingFeedrate[Z_AXIS]);
                    process_it=false;
                    }
            }
            else if (lastButtonAction==UI_ACTION_BACK)//this means user want to cancel current action
                {
                if (confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_CANCEL_ACTION,UI_TEXT_MANUAL_LEVEL))
                    {
                    status=STATUS_CANCEL;
                    if(menuLevel>0) menuLevel--;
                    pushMenu(&ui_menu_manual_level_heat_page,true);
                    PrintLine::moveRelativeDistanceInSteps(0,0,10*Printer::axisStepsPerMM[Z_AXIS],0,Printer::homingFeedrate[0],true,false);
                    UI_STATUS(UI_TEXT_CANCELED);
                    process_it=false;
                    }
                else
                    {//we continue as before
                    if(menuLevel>0) menuLevel--;
                    if(step==STEP_MANUAL_LEVEL_PAGE1)pushMenu(&ui_menu_manual_level_page1,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE2)pushMenu(&ui_menu_manual_level_page2,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE3)pushMenu(&ui_menu_manual_level_page3,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE4)pushMenu(&ui_menu_manual_level_page4,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE5)pushMenu(&ui_menu_manual_level_page5,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE6)pushMenu(&ui_menu_manual_level_page6,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE7)pushMenu(&ui_menu_manual_level_page7,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE8)pushMenu(&ui_menu_manual_level_page8,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE9)pushMenu(&ui_menu_manual_level_page9,true);
                    if(step==STEP_MANUAL_LEVEL_PAGE10)pushMenu(&ui_menu_manual_level_page10,true);
                    if(step==STEP_MANUAL_LEVEL_WAIT_FOR_TEMPERATURE)pushMenu(&ui_menu_manual_level_heat_page,true);
                    }
                    }
            delay(100);
            }

             //wake up light if power saving has been launched
            #if UI_AUTOLIGHTOFF_AFTER!=0
            if (EEPROM::timepowersaving>0)
                {
                UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
                #if CASE_LIGHTS_PIN > 0
                if (!(READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
                    {
                    TOGGLE(CASE_LIGHTS_PIN);
                    }
                #endif
                #if BADGE_LIGHT_PIN > 0
				if (!(READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
					{
					TOGGLE(BADGE_LIGHT_PIN);
					}
				#endif
                #if defined(UI_BACKLIGHT_PIN)
                if (!(READ(UI_BACKLIGHT_PIN))) WRITE(UI_BACKLIGHT_PIN, HIGH);
                #endif
                }
            #endif
            }
        //cool down if necessary
        Extruder::setTemperatureForExtruder(extrudertarget1,0);
        #if NUM_EXTRUDER>1
        Extruder::setTemperatureForExtruder(extrudertarget2,1);
        #endif
        #if HAVE_HEATED_BED==true
          Extruder::setHeatedBedTemperature(bedtarget);
        #endif
        //home again
        Printer::homeAxis(true,true,true);
        if(status==STATUS_OK)
            {
             UI_STATUS(UI_TEXT_PRINTER_READY);
            menuLevel=tmpmenu;
            menuPos[menuLevel]=tmpmenupos;
            menu[menuLevel]=tmpmen;
            }
        else if (status==STATUS_CANCEL)
            {
            while (Printer::isMenuMode(MENU_MODE_PRINTING))Commands::checkForPeriodicalActions(true);
            UI_STATUS(UI_TEXT_CANCELED);
            menuLevel=0;
            }
//restore autoreturn function
#if UI_AUTORETURN_TO_MENU_AFTER!=0
            if (btmp_autoreturn)//if was activated restore it - if not do nothing - stay desactivate
            {
            benable_autoreturn=true;//reactivate
            ui_autoreturn_time=HAL::timeInMilliseconds()+UI_AUTORETURN_TO_MENU_AFTER;//reset counter
            }
 #endif
        refreshPage();
        break;
        }

        case UI_ACTION_PREHEAT_PLA:
            {
//Davinci Specific, preheat menu with feedback need or not,
//read temperature in EEPROM as more easy to change
            UI_STATUS(UI_TEXT_PREHEAT_PLA);
            bool allheat=true;
            if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,0);
#if NUM_EXTRUDER > 1
            if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,1);
#endif
#if NUM_EXTRUDER > 2
            if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_pla)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_pla,2);
#endif
#if HAVE_HEATED_BED
            if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_pla)allheat=false;
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_pla);
#endif
            if (allheat)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
                playsound(5000,240);
                }
            break;
        }
        case UI_ACTION_PREHEAT_ABS:
        {
//Davinci Specific, preheat menu with feedback need or not,
//read temperature in EEPROM as more easy to change
            UI_STATUS(UI_TEXT_PREHEAT_ABS);
            bool allheat=true;
            if(extruder[0].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,0);
#if NUM_EXTRUDER > 1
            if(extruder[1].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,1);
#endif
#if NUM_EXTRUDER > 2
            if(extruder[2].tempControl.targetTemperatureC!=EEPROM::ftemp_ext_abs)allheat=false;
            Extruder::setTemperatureForExtruder(EEPROM::ftemp_ext_abs,2);
#endif
#if HAVE_HEATED_BED
            if(heatedBedController.targetTemperatureC!=EEPROM::ftemp_bed_abs)allheat=false;
            Extruder::setHeatedBedTemperature(EEPROM::ftemp_bed_abs);
#endif
           //Davinci Specific, fancy sound if no need or to confirm it is done
           if (allheat)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
                playsound(5000,240);
                }
            break;
        }
        case UI_ACTION_COOLDOWN:
        {
//Davinci Specific, cooldown menu with feedback need or not
            UI_STATUS(UI_TEXT_COOLDOWN);
            bool alloff=true;
            if(extruder[0].tempControl.targetTemperatureC>0)alloff=false;
            Extruder::setTemperatureForExtruder(0, 0);
#if NUM_EXTRUDER > 1
            if(extruder[1].tempControl.targetTemperatureC>0)alloff=false;
            Extruder::setTemperatureForExtruder(0, 1);
#endif
#if NUM_EXTRUDER > 2
            if(extruder[2].tempControl.targetTemperatureC>0)alloff=false;
            Extruder::setTemperatureForExtruder(0, 2);
#endif
#if HAVE_HEATED_BED
            if (heatedBedController.targetTemperatureC>0)alloff=false;
            Extruder::setHeatedBedTemperature(0);
#endif
           //Davinci Specific, fancy sound if no need or to confirm it is done
             if (alloff)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
                playsound(5000,240);
                }
//do not allows to heatjust after cooling to avoid cached commands
            Extruder::disableheat_time =HAL::timeInMilliseconds() +3000;
            break;
        }
        case UI_ACTION_HEATED_BED_OFF:
#if HAVE_HEATED_BED
           //Davinci Specific, fancy sound if no need or to confirm it is done
          
            if (heatedBedController.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
                playsound(5000,240);
                }
            Extruder::setHeatedBedTemperature(0);
#endif
            break;
        case UI_ACTION_EXTRUDER0_OFF:
#if NUM_EXTRUDER > 1
        case UI_ACTION_EXTRUDER1_OFF:
#endif
#if NUM_EXTRUDER>2
        case UI_ACTION_EXTRUDER2_OFF:
#endif
           //Davinci Specific, fancy sound if no need or to confirm it is done

            if (extruder[action - UI_ACTION_EXTRUDER0_OFF].tempControl.targetTemperatureC==0)
                {
                playsound(1000,140);
                playsound(1000,140);
                }
            else
                {
                playsound(4000,240);
                playsound(5000,240);
                }
            Extruder::setTemperatureForExtruder(0, action - UI_ACTION_EXTRUDER0_OFF);
            break;
        case UI_ACTION_DISABLE_STEPPER:
            Printer::kill(true);
            break;
        case UI_ACTION_RESET_EXTRUDER:
            Printer::currentPositionSteps[E_AXIS] = 0;
            break;
        case UI_ACTION_EXTRUDER_RELATIVE:
            Printer::relativeExtruderCoordinateMode=!Printer::relativeExtruderCoordinateMode;
            break;
        case UI_ACTION_SELECT_EXTRUDER0:
#if NUM_EXTRUDER > 1
        case UI_ACTION_SELECT_EXTRUDER1:
#endif
#if NUM_EXTRUDER > 2
        case UI_ACTION_SELECT_EXTRUDER2:
#endif
            if(!allowMoves) return action;
            Extruder::selectExtruderById(action - UI_ACTION_SELECT_EXTRUDER0);
            currHeaterForSetup = &(Extruder::current->tempControl);
            Printer::setMenuMode(MENU_MODE_FULL_PID, currHeaterForSetup->heatManager == 1);
            Printer::setMenuMode(MENU_MODE_DEADTIME, currHeaterForSetup->heatManager == 3);
            break;
#if EEPROM_MODE != 0
        case UI_ACTION_STORE_EEPROM:
            EEPROM::storeDataIntoEEPROM(false);
            pushMenu(&ui_menu_eeprom_saved, false);
            BEEP_LONG;
//            skipBeep = true;
            break;
        case UI_ACTION_LOAD_EEPROM:
            EEPROM::readDataFromEEPROM();
            Extruder::selectExtruderById(Extruder::current->id);
            pushMenu(&ui_menu_eeprom_loaded, false);
            BEEP_LONG;
//            skipBeep = true;
            break;
#endif
#if SDSUPPORT
        case UI_ACTION_SD_DELETE:
            if(sd.sdactive)
            {
                pushMenu(&ui_menu_sd_fileselector, false);
            }
            else
            {
                UI_ERROR(UI_TEXT_NOSDCARD);
            }
            break;
        case UI_ACTION_SD_PRINT:
            if(sd.sdactive)
            {
                pushMenu(&ui_menu_sd_fileselector, false);
            }
            break;
        case UI_ACTION_SD_PAUSE:
           //Davinci Specific, fancy sound to confirm it is done
			playsound(3000,240);
                        playsound(4000,240);
			Com::printFLN(PSTR("Pause requested"));
            if(!allowMoves)
                ret = UI_ACTION_SD_PAUSE;
            else
                sd.pausePrint(true);
            break;
        case UI_ACTION_SD_CONTINUE:
            if(!allowMoves) ret = UI_ACTION_SD_CONTINUE;
            else sd.continuePrint(true);
            break;
        case UI_ACTION_SD_PRI_PAU_CONT:
            if(!allowMoves) ret = UI_ACTION_SD_PRI_PAU_CONT;
            else
            {
                if(Printer::isMenuMode(MENU_MODE_SD_PRINTING + MENU_MODE_SD_PAUSED))
                    sd.continuePrint();
                else if(Printer::isMenuMode(MENU_MODE_SD_PRINTING))
                    sd.pausePrint(true);
                else if(sd.sdactive)
                    pushMenu(&ui_menu_sd_fileselector,false);
            }
            break;
        case UI_ACTION_STOP_PRINTING_FROM_MENU:
			//ask for confirmation
            if (!confirmationDialog(UI_TEXT_PLEASE_CONFIRM ,UI_TEXT_STOP_PRINT,"",UI_CONFIRMATION_TYPE_YES_NO,false))break;
        case UI_ACTION_SD_STOP:
            {
           //Davinci Specific, Immediate stop
            //if(!allowMoves) ret = false;
            //else sd.stopPrint();
             playsound(400,400);
             //reset connect with host if any
            Com::printFLN(Com::tReset);
            //we are printing from sdcard or from host ?
            if(Printer::isMenuMode(MENU_MODE_SD_PAUSED) || sd.sdmode )sd.stopPrint();
            else Com::printFLN(PSTR("Host Print stopped by user."));
            menuLevel=0;
            menuPos[0]=0;
            refreshPage();
             Printer::setMenuModeEx(MENU_MODE_STOP_REQUESTED,true);
             Printer::setMenuModeEx(MENU_MODE_STOP_DONE,false);
             Printer::setMenuMode(MENU_MODE_SD_PAUSED,false);
             //to be sure no moving  by dummy movement
              Printer::moveToReal(IGNORE_COORDINATE,IGNORE_COORDINATE,IGNORE_COORDINATE,IGNORE_COORDINATE,Printer::homingFeedrate[X_AXIS]);
             GCode::bufferReadIndex=0; ///< Read position in gcode_buffer.
             GCode::bufferWriteIndex=0; ///< Write position in gcode_buffer.
             GCode::commandsReceivingWritePosition=0; ///< Writing position in gcode_transbuffer
             GCode::lastLineNumber=0; ///< Last line number received.
             GCode::bufferLength=0; ///< Number of commands stored in gcode_buffer
             PrintLine::nlFlag = false;
             PrintLine::linesCount=0;
             PrintLine::linesWritePos = 0;            ///< Position where we write the next cached line move.
             PrintLine::linesPos = 0;                 ///< Position for executing line movement
             PrintLine::lines[0].block();
            #if SD_STOP_HEATER_AND_MOTORS_ON_STOP
            executeAction(UI_ACTION_COOLDOWN,true);
            #endif
            Printer::setMenuModeEx(MENU_MODE_STOP_DONE,true);
            UI_STATUS(UI_TEXT_CANCELED);
            }
            break;
        case UI_ACTION_SD_UNMOUNT:
            sd.unmount();
            break;
        case UI_ACTION_SD_MOUNT:
            sd.mount();
            break;
        case UI_ACTION_MENU_SDCARD:
            pushMenu(&ui_menu_sd, false);
            break;
#endif
#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
        case UI_ACTION_FAN_OFF:
        case UI_ACTION_FAN_25:
        case UI_ACTION_FAN_50:
        case UI_ACTION_FAN_75:
            Commands::setFanSpeed((action - UI_ACTION_FAN_OFF) * 64, false);
            break;
        case UI_ACTION_FAN_FULL:
            Commands::setFanSpeed(255, false);
            break;
        case UI_ACTION_FAN_SUSPEND:
        {
            static uint8_t lastFanSpeed = 255;
            if(Printer::getFanSpeed()==0)
                Commands::setFanSpeed(lastFanSpeed,false);
            else
            {
                lastFanSpeed = Printer::getFanSpeed();
                Commands::setFanSpeed(0,false);
            }
        }
        break;
        case UI_ACTION_IGNORE_M106:
            Printer::flag2 ^= PRINTER_FLAG2_IGNORE_M106_COMMAND;
            break;
#endif
        case UI_ACTION_MENU_XPOS:
            pushMenu(&ui_menu_xpos, false);
            break;
        case UI_ACTION_MENU_YPOS:
            pushMenu(&ui_menu_ypos, false);
            break;
        case UI_ACTION_MENU_ZPOS:
            pushMenu(&ui_menu_zpos, false);
            break;
        case UI_ACTION_MENU_XPOSFAST:
            pushMenu(&ui_menu_xpos_fast, false);
            break;
        case UI_ACTION_MENU_YPOSFAST:
            pushMenu(&ui_menu_ypos_fast, false);
            break;
        case UI_ACTION_MENU_ZPOSFAST:
            pushMenu(&ui_menu_zpos_fast, false);
            break;
        case UI_ACTION_MENU_QUICKSETTINGS:
	    //Davinci Specific, No quicksettings for Davinci UI
            //pushMenu(&ui_menu_quick,false);
            break;
        case UI_ACTION_MENU_EXTRUDER:
            pushMenu(&ui_menu_extruder, false);
            break;
        case UI_ACTION_MENU_POSITIONS:
            pushMenu(&ui_menu_positions, false);
            break;
#ifdef UI_USERMENU1
        case UI_ACTION_SHOW_USERMENU1:
            pushMenu(&UI_USERMENU1, false);
            break;
#endif
#ifdef UI_USERMENU2
        case UI_ACTION_SHOW_USERMENU2:
            pushMenu(&UI_USERMENU2, false);
            break;
#endif
#ifdef UI_USERMENU3
        case UI_ACTION_SHOW_USERMENU3:
            pushMenu(&UI_USERMENU3, false);
            break;
#endif
#ifdef UI_USERMENU4
        case UI_ACTION_SHOW_USERMENU4:
            pushMenu(&UI_USERMENU4, false);
            break;
#endif
#ifdef UI_USERMENU5
        case UI_ACTION_SHOW_USERMENU5:
            pushMenu(&UI_USERMENU5, false);
            break;
#endif
#ifdef UI_USERMENU6
        case UI_ACTION_SHOW_USERMENU6:
            pushMenu(&UI_USERMENU6, false);
            break;
#endif
#ifdef UI_USERMENU7
        case UI_ACTION_SHOW_USERMENU7:
            pushMenu(&UI_USERMENU7, false);
            break;
#endif
#ifdef UI_USERMENU8
        case UI_ACTION_SHOW_USERMENU8:
            pushMenu(&UI_USERMENU8, false);
            break;
#endif
#ifdef UI_USERMENU9
        case UI_ACTION_SHOW_USERMENU9:
            pushMenu(&UI_USERMENU9, false);
            break;
#endif
#ifdef UI_USERMENU10
        case UI_ACTION_SHOW_USERMENU10:
            pushMenu(&UI_USERMENU10, false);
            break;
#endif
#if FEATURE_RETRACTION
        case UI_ACTION_WIZARD_FILAMENTCHANGE:
        {    
			//Davinci Specific, use wizard differently           
			menuLevel = 0;
            menuPos[0]=0;
            benable_autoreturn=false;//desactivate
            Printer::setMenuModeEx(MENU_MODE_WIZARD,true);
            Printer::setJamcontrolDisabled(true);
            Com::printFLN(PSTR("important: Filament change required!"));
            Printer::setBlockingReceive(true);
            BEEP_LONG;
            //Davinci Specific, use existing load/unload menu
            //pushMenu(&ui_wiz_filamentchange, true);
            pushMenu(&ui_menu_load_unload, true);
            Printer::resetWizardStack();
            Printer::pushWizardVar(Printer::currentPositionSteps[E_AXIS]);
            Printer::MemoryPosition();
            Extruder::current->retractDistance(FILAMENTCHANGE_SHORTRETRACT);
            float newZ = FILAMENTCHANGE_Z_ADD + Printer::currentPosition[Z_AXIS];
            Printer::currentPositionSteps[E_AXIS] = 0;
            Printer::moveToReal(Printer::currentPosition[X_AXIS], Printer::currentPosition[Y_AXIS], newZ, 0, Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(FILAMENTCHANGE_X_POS, FILAMENTCHANGE_Y_POS, newZ, 0, Printer::homingFeedrate[X_AXIS]);
            Extruder::current->retractDistance(FILAMENTCHANGE_LONGRETRACT);
            Extruder::current->disableCurrentExtruderMotor();
            //Davinci Specific, Fancy effect
            playsound(1000,140);
            playsound(3000,240);
        }
        break;
        //Davinci Specific, End the Filament change Wizard
        case UI_ACTION_WIZARD_FILAMENTCHANGE_END:
            Extruder::current->retractDistance(EEPROM_FLOAT(RETRACTION_LENGTH));
#if FILAMENTCHANGE_REHOME
#if Z_HOME_DIR > 0
            Printer::homeAxis(true, true, FILAMENTCHANGE_REHOME == 2);
#else
            Printer::homeAxis(true, true, false);
#endif
#endif
            Printer::GoToMemoryPosition(true, true, false, false, Printer::homingFeedrate[X_AXIS]);
            Printer::GoToMemoryPosition(false, false, true, false, Printer::homingFeedrate[Z_AXIS]);
            Extruder::current->retractDistance(-EEPROM_FLOAT(RETRACTION_LENGTH));
            Printer::currentPositionSteps[E_AXIS] = Printer::popWizardVar().l; // set e to starting position
            Printer::setBlockingReceive(false);
#if EXTRUDER_JAM_CONTROL
            Extruder::markAllUnjammed();
#endif
            Printer::setJamcontrolDisabled(false);
            Printer::setMenuModeEx(MENU_MODE_WIZARD,false);
            benable_autoreturn=true;//reactivate
        break;
#if EXTRUDER_JAM_CONTROL
        case UI_ACTION_WIZARD_JAM_EOF:
        {
            Extruder::markAllUnjammed();
            Printer::setJamcontrolDisabled(true);
            Printer::setBlockingReceive(true);
            pushMenu(&ui_wiz_jamreheat, true);
            Printer::resetWizardStack();
            Printer::pushWizardVar(Printer::currentPositionSteps[E_AXIS]);
            Printer::MemoryPosition();
            Extruder::current->retractDistance(FILAMENTCHANGE_SHORTRETRACT);
            float newZ = FILAMENTCHANGE_Z_ADD + Printer::currentPosition[Z_AXIS];
            Printer::currentPositionSteps[E_AXIS] = 0;
            Printer::moveToReal(Printer::currentPosition[X_AXIS], Printer::currentPosition[Y_AXIS], newZ, 0, Printer::homingFeedrate[Z_AXIS]);
            Printer::moveToReal(FILAMENTCHANGE_X_POS, FILAMENTCHANGE_Y_POS, newZ, 0, Printer::homingFeedrate[X_AXIS]);
            //Extruder::current->retractDistance(FILAMENTCHANGE_LONGRETRACT);
            Extruder::pauseExtruders();
            Commands::waitUntilEndOfAllMoves();
#if FILAMENTCHANGE_REHOME
            Printer::disableXStepper();
            Printer::disableYStepper();
#if Z_HOME_DIR > 0 && FILAMENTCHANGE_REHOME == 2
            Printer::disableZStepper();
#endif
#endif
        }
        break;
#endif // EXTRUDER_JAM_CONTROL
#endif // FEATURE_RETRACTION
        case UI_ACTION_X_UP:
        case UI_ACTION_X_DOWN:
            if(!allowMoves) return action;
            PrintLine::moveRelativeDistanceInStepsReal(((action == UI_ACTION_X_UP) ? 1.0 : -1.0) * Printer::axisStepsPerMM[X_AXIS], 0, 0, 0, Printer::homingFeedrate[X_AXIS], false);
            break;
        case UI_ACTION_Y_UP:
        case UI_ACTION_Y_DOWN:
            if(!allowMoves) return action;
            PrintLine::moveRelativeDistanceInStepsReal(0, ((action == UI_ACTION_Y_UP) ? 1.0 : -1.0) * Printer::axisStepsPerMM[Y_AXIS], 0, 0, Printer::homingFeedrate[Y_AXIS], false);
            break;
        case UI_ACTION_Z_UP:
        case UI_ACTION_Z_DOWN:
            if(!allowMoves) return action;
            PrintLine::moveRelativeDistanceInStepsReal(0, 0, ((action == UI_ACTION_Z_UP) ? 1.0 : -1.0) * Printer::axisStepsPerMM[Z_AXIS], 0, Printer::homingFeedrate[Z_AXIS], false);
            break;
        case UI_ACTION_EXTRUDER_UP:
        case UI_ACTION_EXTRUDER_DOWN:
            if(!allowMoves) return action;
            PrintLine::moveRelativeDistanceInStepsReal(0, 0, 0, ((action == UI_ACTION_EXTRUDER_UP) ? 1.0 : -1.0) * Printer::axisStepsPerMM[E_AXIS], UI_SET_EXTRUDER_FEEDRATE, false);
            break;
        case UI_ACTION_EXTRUDER_TEMP_UP:
        {
            int tmp = (int)(Extruder::current->tempControl.targetTemperatureC) + 1;
            if(tmp == 1) tmp = UI_SET_MIN_EXTRUDER_TEMP;
            else if(tmp > UI_SET_MAX_EXTRUDER_TEMP) tmp = UI_SET_MAX_EXTRUDER_TEMP;
            Extruder::setTemperatureForExtruder(tmp, Extruder::current->id);
        }
        break;
        case UI_ACTION_EXTRUDER_TEMP_DOWN:
        {
            int tmp = (int)(Extruder::current->tempControl.targetTemperatureC) - 1;
            if(tmp < UI_SET_MIN_EXTRUDER_TEMP) tmp = 0;
            Extruder::setTemperatureForExtruder(tmp, Extruder::current->id);
        }
        break;
        case UI_ACTION_HEATED_BED_UP:
#if HAVE_HEATED_BED
        {
            int tmp = (int)heatedBedController.targetTemperatureC + 1;
            if(tmp == 1) tmp = UI_SET_MIN_HEATED_BED_TEMP;
            else if(tmp > UI_SET_MAX_HEATED_BED_TEMP) tmp = UI_SET_MAX_HEATED_BED_TEMP;
            Extruder::setHeatedBedTemperature(tmp);
        }
#endif
        break;
#if MAX_HARDWARE_ENDSTOP_Z
        case UI_ACTION_SET_MEASURED_ORIGIN:
        {
            Printer::updateCurrentPosition();
            Printer::zLength -= Printer::currentPosition[Z_AXIS];
            Printer::currentPositionSteps[Z_AXIS] = 0;
            Printer::updateDerivedParameter();
#if NONLINEAR_SYSTEM
            transformCartesianStepsToDeltaSteps(Printer::currentPositionSteps, Printer::currentDeltaPositionSteps);
#endif
            Printer::updateCurrentPosition(true);
            Com::printFLN(Com::tZProbePrinterHeight, Printer::zLength);
#if EEPROM_MODE != 0
            EEPROM::storeDataIntoEEPROM(false);
            Com::printFLN(Com::tEEPROMUpdated);
#endif
            Commands::printCurrentPosition(PSTR("UI_ACTION_SET_MEASURED_ORIGIN "));
        }
        break;
#endif
        case UI_ACTION_SET_P1:
#if SOFTWARE_LEVELING
            for (uint8_t i = 0; i < 3; i++)
            {
                Printer::levelingP1[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_SET_P2:
#if SOFTWARE_LEVELING
            for (uint8_t i = 0; i < 3; i++)
            {
                Printer::levelingP2[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_SET_P3:
#if SOFTWARE_LEVELING
            for (uint8_t i = 0; i < 3; i++)
            {
                Printer::levelingP3[i] = Printer::currentPositionSteps[i];
            }
#endif
            break;
        case UI_ACTION_CALC_LEVEL:
#if SOFTWARE_LEVELING
            int32_t factors[4];
            PrintLine::calculatePlane(factors, Printer::levelingP1, Printer::levelingP2, Printer::levelingP3);
            Com::printFLN(Com::tLevelingCalc);
            Com::printFLN(Com::tTower1, PrintLine::calcZOffset(factors, Printer::deltaAPosXSteps, Printer::deltaAPosYSteps) * Printer::invAxisStepsPerMM[Z_AXIS]);
            Com::printFLN(Com::tTower2, PrintLine::calcZOffset(factors, Printer::deltaBPosXSteps, Printer::deltaBPosYSteps) * Printer::invAxisStepsPerMM[Z_AXIS]);
            Com::printFLN(Com::tTower3, PrintLine::calcZOffset(factors, Printer::deltaCPosXSteps, Printer::deltaCPosYSteps) * Printer::invAxisStepsPerMM[Z_AXIS]);
#endif
            break;
        case UI_ACTION_HEATED_BED_DOWN:
#if HAVE_HEATED_BED
        {
            int tmp = (int)heatedBedController.targetTemperatureC - 1;
            if(tmp < UI_SET_MIN_HEATED_BED_TEMP) tmp = 0;
            Extruder::setHeatedBedTemperature(tmp);
        }
#endif
        break;
        case UI_ACTION_FAN_UP:
            Commands::setFanSpeed(Printer::getFanSpeed() + 32,false);
            break;
        case UI_ACTION_FAN_DOWN:
            Commands::setFanSpeed(Printer::getFanSpeed() - 32,false);
            break;
        case UI_ACTION_KILL:
            Commands::emergencyStop();
            break;
        case UI_ACTION_RESET:
            HAL::resetHardware();
            break;
        case UI_ACTION_PAUSE:
            Com::printFLN(PSTR("RequestPause:"));
            break;
#if FEATURE_AUTOLEVEL
        case UI_ACTION_AUTOLEVEL_ONOFF:
            Printer::setAutolevelActive(!Printer::isAutolevelActive());
	    //Davinci Specific, direct EEPROM update
            EEPROM:: update(EPR_AUTOLEVEL_ACTIVE,EPR_TYPE_BYTE,Printer::isAutolevelActive(),0);
            break;
#endif
#ifdef DEBUG_PRINT
        case UI_ACTION_WRITE_DEBUG:
            Com::printF(PSTR("Buf. Read Idx:"),(int)GCode::bufferReadIndex);
            Com::printF(PSTR(" Buf. Write Idx:"),(int)GCode::bufferWriteIndex);
            Com::printF(PSTR(" Comment:"),(int)GCode::commentDetected);
            Com::printF(PSTR(" Buf. Len:"),(int)GCode::bufferLength);
            Com::printF(PSTR(" Wait resend:"),(int)GCode::waitingForResend);
            Com::printFLN(PSTR(" Recv. Write Pos:"),(int)GCode::commandsReceivingWritePosition);
            Com::printF(PSTR("Min. XY Speed:"),Printer::minimumSpeed);
            Com::printF(PSTR(" Min. Z Speed:"),Printer::minimumZSpeed);
            Com::printF(PSTR(" Buffer:"),PrintLine::linesCount);
            Com::printF(PSTR(" Lines pos:"),(int)PrintLine::linesPos);
            Com::printFLN(PSTR(" Write Pos:"),(int)PrintLine::linesWritePos);
            Com::printFLN(PSTR("Wait loop:"),debugWaitLoop);
            Com::printF(PSTR("sd mode:"),(int)sd.sdmode);
            Com::printF(PSTR(" pos:"),sd.sdpos);
            Com::printFLN(PSTR(" of "),sd.filesize);
            break;
#endif
        case UI_ACTION_TEMP_DEFECT:
            Printer::setAnyTempsensorDefect();
            break;
        }
    refreshPage();
//    if(!skipBeep)
//        BEEP_SHORT
#if UI_AUTORETURN_TO_MENU_AFTER!=0
    ui_autoreturn_time = HAL::timeInMilliseconds() + UI_AUTORETURN_TO_MENU_AFTER;
#endif
//Davinci Specific, powersave light management
#if UI_AUTOLIGHTOFF_AFTER!=0
        UIDisplay::ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
#endif
#endif
    return ret;
}
void UIDisplay::mediumAction()
{
#if UI_HAS_I2C_ENCODER>0
    uiCheckSlowEncoder();
#endif
}

// Gets calles from main tread
void UIDisplay::slowAction(bool allowMoves)
{
    millis_t time = HAL::timeInMilliseconds();
    uint8_t refresh = 0;
#if UI_HAS_KEYS == 1
    // delayed action open?
    if(allowMoves && delayedAction != 0)
    {
        executeAction(delayedAction, true);
        delayedAction = 0;
    }

    // Update key buffer
    InterruptProtectedBlock noInts;
    if((flags & (UI_FLAG_FAST_KEY_ACTION + UI_FLAG_KEY_TEST_RUNNING)) == 0)
    {
        flags |= UI_FLAG_KEY_TEST_RUNNING;
        noInts.unprotect();
#if defined(UI_I2C_HOTEND_LED) || defined(UI_I2C_HEATBED_LED) || defined(UI_I2C_FAN_LED)
        {
            // check temps and set appropriate leds
            int led = 0;
#if NUM_EXTRUDER>0 && defined(UI_I2C_HOTEND_LED)
            led |= (tempController[Extruder::current->id]->targetTemperatureC > 0 ? UI_I2C_HOTEND_LED : 0);
#endif
#if HAVE_HEATED_BED && defined(UI_I2C_HEATBED_LED)
            led |= (heatedBedController.targetTemperatureC > 0 ? UI_I2C_HEATBED_LED : 0);
#endif
#if FAN_PIN>=0 && defined(UI_I2C_FAN_LED)
            led |= (Printer::getFanSpeed() > 0 ? UI_I2C_FAN_LED : 0);
#endif
            // update the leds
            uid.outputMask= ~led & (UI_I2C_HEATBED_LED | UI_I2C_HOTEND_LED | UI_I2C_FAN_LED);
        }
#endif
        int nextAction = 0;
        uiCheckSlowKeys(nextAction);
        ui_check_Ukeys(nextAction);
        if(lastButtonAction != nextAction)
        {
            lastButtonStart = time;
            lastButtonAction = nextAction;
            noInts.protect();
            flags |= UI_FLAG_SLOW_KEY_ACTION; // Mark slow action
        }
        noInts.protect();
        flags &= ~UI_FLAG_KEY_TEST_RUNNING;
    }
    noInts.protect();
    if((flags & UI_FLAG_SLOW_ACTION_RUNNING) == 0)
    {
        flags |= UI_FLAG_SLOW_ACTION_RUNNING;
        // Reset click encoder
        noInts.protect();
        int16_t encodeChange = encoderPos;
        encoderPos = 0;
        noInts.unprotect();
        int newAction;
        if(encodeChange) // encoder changed
        {
            nextPreviousAction(encodeChange, allowMoves);
            BEEP_SHORT
            refresh = 1;
        }
        if(lastAction != lastButtonAction)
        {
            if(lastButtonAction == 0)
            {
                if(lastAction >= 2000 && lastAction < 3000)
                    statusMsg[0] = 0;
                lastAction = 0;
                noInts.protect();
                flags &= ~(UI_FLAG_FAST_KEY_ACTION + UI_FLAG_SLOW_KEY_ACTION);
            }
            else if(time - lastButtonStart > UI_KEY_BOUNCETIME)     // New key pressed
            {
                lastAction = lastButtonAction;
                BEEP_SHORT
                if((newAction = executeAction(lastAction, allowMoves)) == 0)
                {
                    nextRepeat = time + UI_KEY_FIRST_REPEAT;
                    repeatDuration = UI_KEY_FIRST_REPEAT;
                }
                else
                {
                    if(delayedAction == 0)
                        delayedAction = newAction;
                }
            }
        }
        else if(lastAction < 1000 && lastAction)     // Repeatable key
        {
            if(time - nextRepeat < 10000)
            {
                if(delayedAction == 0)
                    delayedAction = executeAction(lastAction, allowMoves);
                else
                    executeAction(lastAction, allowMoves);
                repeatDuration -= UI_KEY_REDUCE_REPEAT;
                if(repeatDuration < UI_KEY_MIN_REPEAT) repeatDuration = UI_KEY_MIN_REPEAT;
                nextRepeat = time + repeatDuration;
                BEEP_SHORT
            }
        }
        noInts.protect();
        flags &= ~UI_FLAG_SLOW_ACTION_RUNNING;
    }
    noInts.unprotect();
#endif
#if UI_AUTORETURN_TO_MENU_AFTER != 0
//Davinci Specific, to be able to disable autoreturn
    if(menuLevel > 0 && ui_autoreturn_time < time && benable_autoreturn && !uid.isWizardActive()) // Go to top menu after x seoonds
    {
        lastSwitch = time;
        menuLevel = 0;
        activeAction = 0;
    }
#endif
    if(uid.isWizardActive())
        previousMillisCmd = HAL::timeInMilliseconds(); // prevent stepper/heater disable from timeout during active wizard
//Davinci Specific, powersave and light management
#if UI_AUTOLIGHTOFF_AFTER!=0
if (ui_autolightoff_time==-1) ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving;
if ((ui_autolightoff_time<time) && (EEPROM::timepowersaving>0) )
    {//if printing and keep light on do not swich off
    if(!(EEPROM::bkeeplighton  &&((Printer::menuMode&MENU_MODE_SD_PRINTING)||(Printer::menuMode&MENU_MODE_PRINTING)||(Printer::menuMode&MENU_MODE_SD_PAUSED))))
    {
        #if CASE_LIGHTS_PIN > 0
        if ((READ(CASE_LIGHTS_PIN)) && EEPROM::buselight)
            {
            TOGGLE(CASE_LIGHTS_PIN);
            }
        #endif
        #if BADGE_LIGHT_PIN > 0
		if ((READ(BADGE_LIGHT_PIN)) && EEPROM::busebadgelight && EEPROM::buselight)
			{
			TOGGLE(BADGE_LIGHT_PIN);
			}
		#endif
        #if defined(UI_BACKLIGHT_PIN)
        WRITE(UI_BACKLIGHT_PIN, LOW);
        #endif
        }
    if((EEPROM::bkeeplighton  &&((Printer::menuMode&MENU_MODE_SD_PRINTING)||(Printer::menuMode&MENU_MODE_PRINTING)||(Printer::menuMode&MENU_MODE_SD_PAUSED))))ui_autolightoff_time=HAL::timeInMilliseconds()+EEPROM::timepowersaving+10000;
    }
#endif
    if(menuLevel == 0 && time > 4000) // Top menu refresh/switch
    {
        if(time - lastSwitch > UI_PAGES_DURATION)
        {
            lastSwitch = time;
#if !defined(UI_DISABLE_AUTO_PAGESWITCH) || !UI_DISABLE_AUTO_PAGESWITCH
            menuPos[0]++;
            if(menuPos[0] >= UI_NUM_PAGES)
                menuPos[0] = 0;
#endif
            refresh = 1;
        }
        else if(time - lastRefresh >= 1000) refresh = 1;
    }
    else if(time - lastRefresh >= 800)
    {
        UIMenu *men = (UIMenu*)menu[menuLevel];
        uint8_t mtype = pgm_read_byte((void*)&(men->menuType));
        //if(mtype!=1)
        refresh = 1;
    }
    if(refresh) // does lcd need a refresh?
    {
        if (menuLevel > 1 || Printer::isAutomount())
        {
            shift++;
            if(shift + UI_COLS > MAX_COLS + 1)
                shift = -2;
        }
        else
            shift = -2;

        refreshPage();
        lastRefresh = time;
    }
}


// Gets called from inside an interrupt with interrupts allowed!
void UIDisplay::fastAction()
{
#if UI_HAS_KEYS == 1
    // Check keys
    InterruptProtectedBlock noInts;
    if((flags & (UI_FLAG_KEY_TEST_RUNNING + UI_FLAG_SLOW_KEY_ACTION)) == 0)
    {
        flags |= UI_FLAG_KEY_TEST_RUNNING;
        int nextAction = 0;
        uiCheckKeys(nextAction);
//        ui_check_Ukeys(nextAction);
        if(lastButtonAction != nextAction)
        {
            lastButtonStart = HAL::timeInMilliseconds();
            lastButtonAction = nextAction;
            flags |= UI_FLAG_FAST_KEY_ACTION;
        }
        flags &= ~UI_FLAG_KEY_TEST_RUNNING;
    }
#endif
}

#if defined(UI_REVERSE_ENCODER) && UI_REVERSE_ENCODER == 1
#if UI_ENCODER_SPEED==0
const int8_t encoder_table[16] PROGMEM = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; // Full speed
#elif UI_ENCODER_SPEED==1
const int8_t encoder_table[16] PROGMEM = {0,0,1,0,0,0,0,-1,-1,0,0,0,0,1,0,0}; // Half speed
#else
const int8_t encoder_table[16] PROGMEM = {0,0,0,0,0,0,0,0,0,0,0,1,0,0,-1,0}; // Quart speed
#endif
#else
#if UI_ENCODER_SPEED==0
const int8_t encoder_table[16] PROGMEM = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0}; // Full speed
#elif UI_ENCODER_SPEED==1
const int8_t encoder_table[16] PROGMEM = {0,0,-1,0,0,0,0,1,1,0,0,0,0,-1,0,0}; // Half speed
#else
//const int8_t encoder_table[16] PROGMEM = {0,0,0,0,0,0,0,0,1,0,0,0,0,-1,0,0}; // Quart speed
//const int8_t encoder_table[16] PROGMEM = {0,1,0,0,-1,0,0,0,0,0,0,0,0,0,0,0}; // Quart speed
const int8_t encoder_table[16] PROGMEM = {0,0,0,0,0,0,0,0,0,0,0,-1,0,0,1,0}; // Quart speed
#endif
#endif
#endif

