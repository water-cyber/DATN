#ifndef GPIO_IN_OUT_H
#define GPIO_IN_OUT_H

#define PIN_TEST GPIO_NUM_0  // Chân test
#define LED_OUT GPIO_NUM_2
#define PIN_IN_1 GPIO_NUM_32
#define PIN_IN_2 GPIO_NUM_35
#define PIN_IN_3 GPIO_NUM_34
#define PIN_IN_4 GPIO_NUM_39
#define PIN_IN_5 GPIO_NUM_36
#define PIN_OUT GPIO_NUM_33
#define GPIO_IN_PIN ( (1ULL << PIN_TEST) | (1ULL << PIN_IN_1) | (1ULL << PIN_IN_2) | (1ULL << PIN_IN_3) | (1ULL << PIN_IN_4) | (1ULL << PIN_IN_5)) 
#define GPIO_OUT_PIN ( (1ULL << LED_OUT) | (1ULL << PIN_OUT)) 
void gpio_start();
// void check_gpio_and_publish(void);
#endif /* GPIO_IN_OUT_H */