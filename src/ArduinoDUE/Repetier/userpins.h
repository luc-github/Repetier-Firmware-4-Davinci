//Davinci 1.0 and 2.0 pins
#define CPU_ARCH    ARCH_ARM
//axis
#define ORIG_X_STEP_PIN     15
#define ORIG_X_DIR_PIN      14
#define ORIG_X_MIN_PIN      11
#define ORIG_X_MAX_PIN      -1
#define ORIG_X_ENABLE_PIN   29

#define ORIG_Y_STEP_PIN     30
#define ORIG_Y_DIR_PIN      12
#define ORIG_Y_MIN_PIN      68
#define ORIG_Y_MAX_PIN      -1
#define ORIG_Y_ENABLE_PIN   69

#define ORIG_Z_STEP_PIN     119
#define ORIG_Z_DIR_PIN      118
#if MODEL==0
#define ORIG_Z_MIN_PIN      124
#else
#define ORIG_Z_MIN_PIN      128
#endif
#define ORIG_Z_MAX_PIN      -1
#define ORIG_Z_ENABLE_PIN   120
//bed
#if MODEL==0
#define HEATER_1_PIN        17
#else
#define HEATER_1_PIN      67
#endif

#define TEMP_1_PIN      14 // ADC channel #, not a PIN #

//extruders and sensors
#if NUM_EXTRUDER==1
#define HEATER_0_PIN        16
#define TEMP_0_PIN      13 // ADC channel #, not a PIN #
#define FIL_SENSOR1_PIN          24
#define ORIG_E0_ENABLE_PIN  123
#define ORIG_E0_STEP_PIN    122
#define ORIG_E0_DIR_PIN     121
#else
//for davinci 2.0 reference is left extruder so need to exchange pins compare to 1.0
#define HEATER_2_PIN        16//HEATER_0_PIN //switch pin value
#define TEMP_2_PIN      13//    TEMP_0_PIN  //switch pin value
#define HEATER_0_PIN            20
#define TEMP_0_PIN               9 // ADC channel #, not a PIN #
#define FIL_SENSOR1_PIN          129
#define FIL_SENSOR2_PIN          24
#define ORIG_E1_ENABLE_PIN  123//ORIG_E0_ENABLE_PIN //switch pin value
#define ORIG_E1_STEP_PIN    122//ORIG_E0_STEP_PIN   //switch pin value
#define ORIG_E1_DIR_PIN     121//ORIG_E0_DIR_PIN    //switch pin value
#define ORIG_E0_STEP_PIN        53
#define ORIG_E0_DIR_PIN          3
#if MODEL==0
#define ORIG_E0_ENABLE_PIN     128
#else
#define ORIG_E0_ENABLE_PIN     124 
#endif
#endif

//fan PINS
#define FAN_BOARD_PIN -1
#define ORIG_FAN_PIN        25
#define ORIG_FAN2_PIN       4

//additionnal PINS
#define LED_PIN         -1
#if MODEL==0
#define LIGHT_PIN    85
#else
#define LIGHT_PIN    17
#endif
#define ORIG_PS_ON_PIN      -1
#define PS_ON_PIN       ORIG_PS_ON_PIN
#if MODEL==0
#define TOP_SENSOR_PIN      6
#else
#define TOP_SENSOR_PIN      64
#endif
#if DAVINCI == 4
#define BADGE_LIGHT_PIN		6
#define LASER1_PIN 101 
#define LED_LASER1_PIN 7
#define LASER2_PIN   2
#define LED_LASER2_PIN 108
#else
#define BADGE_LIGHT_PIN		-1
#endif
#undef SDSS
#define SDSS            55
#define MOSI_PIN        43
#define MISO_PIN        73
#define SCK_PIN         42
#define SDPOWER                 -1
#define ORIG_SDCARDDETECT            74
#define SDSUPPORT       true
#define SDCARDDETECTINVERTED    0
#define DUE_SOFTWARE_SPI

/*
// LCD PINS - reported in uiconfig.h line 200
 #define UI_DISPLAY_RS_PIN      8       // PINK.1, 88, D_RS
#define UI_DISPLAY_RW_PIN       45 //but do not use it just set state to LOW
#define UI_DISPLAY_ENABLE_PIN           125     // PINK.3, 86, D_E
#define UI_DISPLAY_D0_PIN       34      // PINF.5, 92, D_D4
#define UI_DISPLAY_D1_PIN       35      // PINK.2, 87, D_D5
#define UI_DISPLAY_D2_PIN       36      // PINL.5, 40, D_D6
#define UI_DISPLAY_D3_PIN       37      // PINK.4, 85, D_D7
#define UI_DISPLAY_D4_PIN       38      // PINF.5, 92, D_D4
#define UI_DISPLAY_D5_PIN       39      // PINK.2, 87, D_D5
#define UI_DISPLAY_D6_PIN       40      // PINL.5, 40, D_D6
#define UI_DISPLAY_D7_PIN       41      // PINK.4, 85, D_D7
#define UI_DELAYPERCHAR             320
//back light  PIN - reported in uiconfig.h
#define UI_BACKLIGHT_PIN                78
#define BEEPER_PIN 66
 */
 
//USB Connection 
#define RFSERIAL SerialUSB  // Native USB Port of the due 
//EEPROM is on SDCard
#define EEPROM_AVAILABLE EEPROM_SDCARD
//EEprom on  SDCard
#define SDEEPROM

#define E0_PINS ORIG_E0_STEP_PIN,ORIG_E0_DIR_PIN,ORIG_E0_ENABLE_PIN,
#define E1_PINS ORIG_E1_STEP_PIN,ORIG_E1_DIR_PIN,ORIG_E1_ENABLE_PIN,

