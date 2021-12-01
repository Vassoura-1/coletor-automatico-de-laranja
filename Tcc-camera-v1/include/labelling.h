#if !defined(LABELLING_H)
#define LABELLING_H
    #include <Arduino.h>

    uint8_t LabelImage(unsigned char width, unsigned char height,
                uint8_t * input, uint8_t * output,
                unsigned short max_size, unsigned short min_size);
#endif // LABELLING_H
