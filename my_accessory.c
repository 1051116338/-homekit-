/*
 * my_accessory.c
 * Define the accessory in C language using the Macro in characteristics.h
 *
 *  Created on: 2020-05-15
 *      Author: Mixiaoxiao (Wang Bin)
 */

#include <homekit/homekit.h>
#include <homekit/characteristics.h>


void my_accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}

// Switch (HAP section 8.38)
// required: ON
// optional: NAME

// format: bool; HAP section 9.70; write the .setter function to get the switch-event sent from iOS Home APP.
homekit_characteristic_t cha_switch_on = HOMEKIT_CHARACTERISTIC_(ON, false);

// format: string; HAP section 9.62; max length 64
//homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, "Switch1");
//
//homekit_accessory_t *accessories[] = {
//    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
//        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
//            HOMEKIT_CHARACTERISTIC(NAME, "yyxbc_Switch"),
//            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "esp-01s HomeKit"),
//            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
//            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
//            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
//            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
//            NULL
//        }),
//		HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
//			&cha_switch_on,
//			&cha_name,
//			NULL
//		}),
//      NULL
//    }),
//    NULL
//};

homekit_server_config_t config = {
//		.accessories = accessories,
		.password = "111-11-111"
};

homekit_accessory_t *accessories_pri = NULL;
void init_accessory_identify(const char* serial_number,const char* password){
  char szName[50];
  char szSn[50];
  sprintf(szName,"yyxbc_Switch%s", serial_number ) ;
  sprintf(szSn,"SN-%s", serial_number ) ;
//  long id =atol(serial_number);
  homekit_accessory_t *accessories[] = {
        NEW_HOMEKIT_ACCESSORY(.id= 1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                NEW_HOMEKIT_CHARACTERISTIC(NAME, szName),
                NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "esp-01s HomeKit"),
                NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, szSn),
                NEW_HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
                NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                NULL
            }),
          NEW_HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
          &cha_switch_on,
//          &cha_name,
            NEW_HOMEKIT_CHARACTERISTIC(NAME, szName),
          NULL
        }),
          NULL
        }),
        NULL
    };
   accessories_pri = homekit_accessory_clone(accessories);
   config.accessories = accessories_pri;
   config.password = password;
//   homekit_accessories_init(accessories_pri);
   arduino_homekit_setup(&config);
}
