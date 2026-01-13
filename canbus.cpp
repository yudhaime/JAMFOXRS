#include "canbus.h"
#include "config.h"
#include <Arduino.h>

#ifdef ESP32
#include <driver/twai.h>
#endif

int tempCtrl = DEFAULT_TEMP;
int tempMotor = DEFAULT_TEMP;
int tempBatt = DEFAULT_TEMP;

bool initCAN() {
#ifdef ESP32
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        (twai_mode_t)CAN_MODE
    );
    
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("Gagal install CAN driver");
        return false;
    }
    
    if (twai_start() != ESP_OK) {
        Serial.println("Gagal start CAN bus");
        return false;
    }

    Serial.println("CAN siap");
    return true;
#else
    Serial.println("CAN hanya untuk ESP32");
    return false;
#endif
}

void updateCAN() {
#ifdef ESP32
    twai_message_t message;
    
    if(twai_receive(&message, pdMS_TO_TICKS(50)) == ESP_OK){
        if(message.identifier == ID_CTRL_MOTOR && message.data_length_code >= 6){
            tempCtrl = message.data[4];
            tempMotor = message.data[5];
        }
        else if(message.identifier == ID_BATT_5S && message.data_length_code >= 5){
            int maxTemp = -100;
            for(int i = 0; i < 5; ++i){
                if(message.data[i] > maxTemp) maxTemp = message.data[i];
            }
            if(maxTemp != -100) tempBatt = maxTemp;
        }
        else if(message.identifier == ID_BATT_SINGLE && message.data_length_code >= 6){
            int battTemp = message.data[5];
            if(battTemp > tempBatt) tempBatt = battTemp;
        }
    }
#endif
}