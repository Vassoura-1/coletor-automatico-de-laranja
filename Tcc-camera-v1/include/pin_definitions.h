#if !defined(PIN_DEFINITIONS_H)
#define PIN_DEFINITIONS_H

    // Controlador principal
    // 12 - Laranja Detectada
    // 13 - Enable
    #define CAM_WRITE_P 13
    #define CAM_ENABLE_P 12

    // Flash interno (destruido)
    // #define FLASH_P 04

    // Led interno
    #define LED_B_P 33

    // I2C Pins (Qualquer um pode ser)
    // 02 - SDA ; 04 - SCL
    #define SPI_SDA_P 2
    #define SPI_SCL_P 4
    
    // VL53L0X XShut
    // 14, 15
    #define XShut1_P 14
    #define XShut2_P 15

#endif // PIN_DEFINITIONS_H
