#include "state_machine.h"
#include "variaveis_globais.h"

state_t state;
uint64_t ext_T1;
uint64_t ext_T2;
uint64_t ext_T3;
int ext_T1_icnt=0;
int ext_T2_icnt=0;
int ext_T3_icnt=0;

// constante de distância do ultrassom usada em uma das transições
#define TR_DISTANCIA_ULTRASSOM 10 //cm
#define TR_ULTRASSOM_MIN_DIST_VALIDA 2 //cm

// constantes de posição do encoder usadas em transições
#define TR_POS_ENCODER_MINIMO 10 //meia volta
#define TR_POS_ENCODER_DESTAQUE 80 //quatro voltas 

// constantes relacionadas ao movimento do motor
#define MOTOR_VEL_DESTACAMENTO 200 //duty cicly (0 a 255)
#define MOTOR_PARADO 0 

// constantes de tempo para os timer
#define TEMPO_TIMER_1 500 //tempo em ms
#define TEMPO_TIMER_2 200 //tempo em ms
#define TEMPO_TIMER_3 1000 //tempo em ms

// Timer de transição 1
volatile bool Timer_1_flag;
hw_timer_t * hw_timer_1 = NULL;
void IRAM_ATTR Timer_1_isr() {
    // ISR do timer usado no estado S4
    Timer_1_flag = (state == S4) && true;
    ext_T1_icnt++;
}

// Timer de transição 2
volatile bool Timer_2_flag;
hw_timer_t * hw_timer_2 = NULL;
void IRAM_ATTR onTimer_2_isr() {
    // ISR do timer usado no estado S3
    Timer_2_flag = (state == S3) && true;
    ext_T2_icnt++;
}

// Timer de transição 2
volatile bool Timer_3_flag;
hw_timer_t * hw_timer_3 = NULL;
void IRAM_ATTR onTimer_3_isr() {
    // ISR do timer usado no estado S3
    Timer_3_flag = (state == S5) && true;
    ext_T3_icnt++;
}

state_t initStateMachine(void){
    log_v(F("função initStateMachine"));
    state = S0;

    Timer_1_flag = false;
    Timer_2_flag = false;
    Timer_3_flag = false;

    // Configuração dos alarmes baseados em timer
    // os timers rodam a 80Mz, presscaler de 80 faz cada tick equivaler a 1us

    // configura o timer usado na transição temporal T1
    // utiliza o hardware timer 1, com preescaler de 80, edge triggered
    hw_timer_1 = timerBegin(1, 80, true);
    // define a função a ser chamada na interrupt
    timerAttachInterrupt(hw_timer_1, &Timer_1_isr, true);
    // define o tempo de chamada para a interrupt, e desativa o rearme automático
    timerAlarmWrite(hw_timer_1, TEMPO_TIMER_1*1000, false);

    // timer da transição temporal t2
    // utiliza o hardware timer 2, com preescaler de 80, edge triggered
    hw_timer_2 = timerBegin(2, 80, true);
    // define a função a ser chamada na interrupt
    timerAttachInterrupt(hw_timer_2, &onTimer_2_isr, true);
    // define o tempo de chamada para a interrupt, e desativa o rearme automático
    timerAlarmWrite(hw_timer_2, TEMPO_TIMER_2*1000, false);

    // timer da transição temporal t2
    // utiliza o hardware timer 2, com preescaler de 80, edge triggered
    hw_timer_2 = timerBegin(3, 80, true);
    // define a função a ser chamada na interrupt
    timerAttachInterrupt(hw_timer_3, &onTimer_3_isr, true);
    // define o tempo de chamada para a interrupt, e desativa o rearme automático
    timerAlarmWrite(hw_timer_3, TEMPO_TIMER_3*1000, false);


    return state;
};

state_t resetStateMachine(void){
    log_v(F("função resetStateMachine"));
    state = S0;

    Timer_1_flag = false;
    Timer_2_flag = false;
    Timer_3_flag = false;

    return state;
};

void setTimersInState(){
    log_v(F("função updateStateMachine"));

    switch (state)
    {
    // estados em que não há transições temporais
    default:
    case S0:
    case S1:
    case S2:
    case S6:
    case S7:
        break;
    
    case S3:
        // arma o contador T1 para a transição temporal
        timerWrite(hw_timer_2, 0);
        timerAlarmEnable(hw_timer_2);
        timerWrite(hw_timer_2, 0);
        break;
    
    case S4:
        // arma o contador T1 para a transição temporal
        timerWrite(hw_timer_1, 0);
        timerAlarmEnable(hw_timer_1);
        timerWrite(hw_timer_1, 0);
        break;

    case S5:
        // arma o contador T1 para a transição temporal
        timerWrite(hw_timer_3, 0);
        timerAlarmEnable(hw_timer_3);
        timerWrite(hw_timer_3, 0);
        break;
    }
}

state_t updateStateMachine(sensor_readings_t * readings){
    log_v(F("função updateStateMachine"));
    state_t old_state = state;

    // este switch contém as definições de transições de cada estado
    switch (state)
    {
    case S0:
        if (readings->start == HIGH)
            state = S1;
        break;

    case S1:
        if (readings->start == LOW)
            state = S0;
        else if (readings->cam == HIGH)
            state = S2;
        break;

    case S2:
        if ((readings->u1 <= TR_DISTANCIA_ULTRASSOM && readings->u1 >= TR_ULTRASSOM_MIN_DIST_VALIDA)
            || (readings->u2 <= TR_DISTANCIA_ULTRASSOM && readings->u1 >= TR_ULTRASSOM_MIN_DIST_VALIDA))
            state = S3;
        else if ((readings->stop22 == HIGH && readings->stop21 == LOW))
            state = S7;
        break;

    case S3:
        if ((readings->stop11 == HIGH && readings->stop12 == LOW) || Timer_2_flag == true)
        {
            timerAlarmDisable(hw_timer_2);
            state = S4;
        }
        Timer_2_flag = false;
        break;

    case S4:
        if (readings->encpos >= TR_POS_ENCODER_MINIMO){
            timerAlarmDisable(hw_timer_1);
            state = S5;
        }
        else if (Timer_1_flag == true)
        {
            state = S7;
        }
        Timer_1_flag = false;
        break;

    case S5:
        if ((readings->stop21 == HIGH && readings->stop22 == LOW) 
            && (readings->encpos > TR_POS_ENCODER_DESTAQUE )){
            timerAlarmDisable(hw_timer_3);
            state = S6;
        } else if (Timer_3_flag == true)
        {
            state = S7;
        }
        Timer_3_flag = false;
        break;

    case S6:
        if ((readings->stop21 == HIGH && readings->stop22 == LOW) 
            && (readings->stop12 == HIGH && readings->stop11 == LOW)){
            if (readings->start == HIGH)
                state = S1;
            else
                state = S0;
        }
        break;

    case S7:
        if((readings->stop21 == HIGH && readings->stop22 == LOW)){
            if (readings->start == HIGH)
                state = S1;
            else
                state = S0;
        }
        break;

    default:
        break;
    }

    // função chamadas sempre que há uma transição de estado
    if (old_state != state)
        setTimersInState();
    
    ext_T1 = timerRead(hw_timer_1)/1000;
    ext_T2 = timerRead(hw_timer_2)/1000;
    ext_T3 = timerRead(hw_timer_3)/1000;

    return state;
};

actuator_outputs_t getOutPutsFromState(void){
    log_v(F("função getOutPutsFromState"));
    actuator_outputs_t saidas;

    // este switch contém os valores para as saídas em cada estado
    switch (state)
    {
    case S0:
        saidas.cam = LOW;
        saidas.sole11 = LOW;
        saidas.sole12 = LOW;
        saidas.sole21 = LOW;
        saidas.sole22 = LOW;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;
    
        saidas.reset_encoder = true;
        break;

    case S1:
        saidas.cam = HIGH;
        saidas.sole11 = LOW;
        saidas.sole12 = HIGH;
        saidas.sole21 = LOW;
        saidas.sole22 = HIGH;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;

        saidas.reset_encoder = true;
        break;

    case S2:
        saidas.cam = HIGH;
        saidas.sole11 = LOW;
        saidas.sole12 = HIGH;
        saidas.sole21 = HIGH;
        saidas.sole22 = LOW;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;

        saidas.reset_encoder = true;
        break;

    case S3:
        saidas.cam = LOW;
        saidas.sole11 = HIGH;
        saidas.sole12 = LOW;
        saidas.sole21 = HIGH;
        saidas.sole22 = LOW;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;

        saidas.reset_encoder = true;
        break;

    case S4:
        saidas.cam = LOW;
        saidas.sole11 = HIGH;
        saidas.sole12 = HIGH;
        saidas.sole21 = HIGH;
        saidas.sole22 = LOW;
        saidas.motor1_vel = MOTOR_VEL_DESTACAMENTO;
        saidas.motor11 = HIGH;
        saidas.motor12 = LOW;

        saidas.reset_encoder = false;
        break;

    case S5:
        saidas.cam = LOW;
        saidas.sole11 = HIGH;
        saidas.sole12 = HIGH;
        saidas.sole21 = LOW;
        saidas.sole22 = HIGH;
        saidas.motor1_vel = MOTOR_VEL_DESTACAMENTO;
        saidas.motor11 = HIGH;
        saidas.motor12 = LOW;

        saidas.reset_encoder = false;
        break;

    case S6:
        saidas.cam = HIGH;
        saidas.sole11 = LOW;
        saidas.sole12 = HIGH;
        saidas.sole21 = LOW;
        saidas.sole22 = HIGH;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;

        saidas.reset_encoder = true;
        break;

    case S7:
        saidas.cam = LOW;
        saidas.sole11 = LOW;
        saidas.sole12 = HIGH;
        saidas.sole21 = LOW;
        saidas.sole22 = HIGH;
        saidas.motor1_vel = MOTOR_PARADO;
        saidas.motor11 = LOW;
        saidas.motor12 = LOW;

        saidas.reset_encoder = true;
        break;
    }
    return saidas;
}