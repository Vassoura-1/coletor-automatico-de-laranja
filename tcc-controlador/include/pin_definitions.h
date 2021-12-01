#if !defined(PIN_DEFINITIONS_H)
#define PIN_DEFINITIONS_H
    // Digital Input Pins
    // 36,39,34,35 - Limit Switches (Interupts) (Input Only pins)
    #define LIMIT21_P 39
    #define LIMIT22_P 36
    #define LIMIT11_P 34
    #define LIMIT12_P 35
    // 27 - Start Switch
    #define START_P 2

    // Rotary Encoder
    // 21 - CLK ; 19 - DT
    #define ENC_CLK_P 21
    #define ENC_DT_P 19
    
    // Load Cell 
    // 32 - DT ; 33 - SCK
    #define HX711_DT_P 32
    #define HX711_SCK_P 33

    // Ultrassounds
    // 25, 05 - Trigg 1, Echo 1
    #define US1_ECHO_P 5
    #define US1_TRIG_P 18
    // 26, 18 - Trigg 2, Echo 2
    #define US2_ECHO_P 26
    #define US2_TRIG_P 25

    // I2C Pins (Qualquer um pode ser)
    // 02 - SDA ; 04 - SCL
    #define SPI_SDA_P 16
    #define SPI_SCL_P 17
    
    // VL53L0X XShut
    // 14, 15
    #define XShut1_P 14
    #define XShut2_P 15

    // Digital Output Pins
    // 27,14 - Single Solenoid Valves (garra)
    // 22,23 - Single Solenoid Valves (avanço)
    #define SOLE11_P 27 //check 27
    #define SOLE12_P 14 //check 14
    #define SOLE21_P 23 //check 12
    #define SOLE22_P 22 //check 22

    // PWM Pins (Pode ser qualquer um)
    // 7,8 - H-Bridge 1
    #define HMotor11_P 4
    // #define HMotor12_P 4
    // #define HMotor1Enable_P 4
    
    // ESP CAM
    // 12 - Laranja Detectada
    // 13 - Enable
    #define CAM_READ_P 13
    #define CAM_ENABLE_P 12

    // // SPI Pins (ESP CAM)
    // // 12 - MISO ; 13 - MOSI
    // // 14 - CLK ; 15 - SS
    // #define SPI_MISO_P 12
    // #define SPI_MOSI_P 13
    // #define SPI_CLK_P 14
    // #define SPI_CS_P 15

#endif // PIN_DEFINITIONS_H