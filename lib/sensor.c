#include "sensor.h"

void sensor_init(int gpio){
    adc_init();
    adc_gpio_init(gpio);
}
float read_sensor(int gpio){
    uint64_t soma = 0;
    adc_select_input(2);
    for (int i =0; i <500; i++){
        soma += adc_read();
        }    
    float nivel = (float)soma/500.0f;
    return nivel;
}
void calibra_sensor(int gpio){
    float leitura = read_sensor(gpio);
    printf("VALOR LIDO: %f \n",leitura);
}

float get_nivel(int gpio, float MAX, float MIN){
    float leitura = read_sensor(gpio);
    float nivel = (float)(leitura - MIN)/ (MAX - MIN) * 100;
    printf("LEITURA DO SENSOR (%%): %.2f \n", nivel);  
    return nivel;
}

float get_nivel_cm(int gpio, float MAX, float MIN, float MAX_cm, float MIN_cm){
    float nivel =  get_nivel(gpio,MAX,MIN);
    float nivel_cm =  (nivel/ 100) * (MAX_cm - MIN_cm) + MIN_cm; 
    return nivel_cm;

}