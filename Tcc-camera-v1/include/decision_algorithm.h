#if !defined(DECISION_ALGORITHM_H)
    #define DECISION_ALGORITHM_H

    #include <Arduino.h>
    
    bool decision_algorithm(const uint8_t width, const uint8_t height,
                            const uint8_t * circle_buff, const uint8_t circle_buff_len,
                            const uint16_t d1, const uint16_t d2, bool * fruit_detected_flag);
#endif // DECISION_ALGORITHM_H
