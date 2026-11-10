#ifndef command_servo_h
#define command_servo_h

#ifdef ENABLE_COMMAND_SERVO
typedef struct {
  uint8_t version;
  uint8_t reserved; // Preserve EEPROM layout and existing saved angles.
  uint8_t closed_degrees;
  uint8_t open_degrees;
} command_servo_settings_t;
extern command_servo_settings_t command_servo_settings;
void command_servo_settings_load(void);
void command_servo_settings_restore(void);
uint8_t command_servo_store_setting(uint8_t parameter, float value);
void command_servo_init(void);
void command_servo_set(uint8_t open);
#endif

#endif
