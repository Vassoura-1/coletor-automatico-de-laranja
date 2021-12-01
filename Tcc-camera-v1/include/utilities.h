#if !defined(_UTILITIES_H_)
#define _UTILITIES_H_
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

// estutura de dados para filtro de média móvel
typedef struct {
        size_t size; //number of values used for filtering
        size_t index; //current value index
        size_t count; //value count
        int sum;
        int * values; //array to be filled with values
} ra_filter_t;

// Inicializa filtro de média móvel
ra_filter_t * ra_filter_init(ra_filter_t * filter, size_t sample_size);

// Aplicação de uma iteração do filtro, a partir de um novo valor
int ra_filter_run(ra_filter_t * filter, int value);

#endif // _UTILITIES_H_
