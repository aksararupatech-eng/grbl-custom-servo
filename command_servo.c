#include "grbl.h"

#ifdef ENABLE_COMMAND_SERVO
#if COMMAND_SERVO_PIN != 18
#error "Command servo currently supports A4 (Arduino pin 18); other pins are reserved by Grbl."
#endif
#if !defined(CPU_MAP_ATMEGA328P) || !defined(VARIABLE_SPINDLE)
#error "Command servo requires Uno and VARIABLE_SPINDLE Timer2 configuration."
#endif
#if defined(ENABLE_M7) || defined(ENABLE_DUAL_AXIS)
#error "Command servo A4 conflicts with M7 or firmware dual-axis pins."
#endif
#if F_CPU != 16000000UL
#error "Command servo timing requires a 16 MHz Uno."
#endif
#if SPINDLE_TCCRA_INIT_MASK != ((1<<WGM20) | (1<<WGM21)) || SPINDLE_TCCRB_INIT_MASK != (1<<CS22)
#error "Command servo requires Timer2 fast PWM with a /64 prescaler."
#endif
#if COMMAND_SERVO_MIN_US < 500 || COMMAND_SERVO_MAX_US > 2500 || COMMAND_SERVO_MIN_US >= COMMAND_SERVO_MAX_US
#error "Servo pulse widths must be between 500 and 2500 microseconds."
#endif

// Timer2 fast PWM /64: 4 us ticks and 1024 us overflows. Twenty
// overflows give a 20.48 ms servo frame, preserving spindle PWM on OC2A.
// OC2B stays disconnected: its compare interrupt controls A4 in software.
static volatile uint16_t requested_ticks;
static volatile uint8_t frame;
static volatile uint8_t pulse_end_frame;
static uint8_t current_open;

// Separate extension record: preserve the original settings_t and version 10.
#define COMMAND_SERVO_EEPROM_ADDR 256U
typedef char servo_eeprom_must_not_overlap[(EEPROM_ADDR_GLOBAL + sizeof(settings_t) + 1 <= COMMAND_SERVO_EEPROM_ADDR && COMMAND_SERVO_EEPROM_ADDR + sizeof(command_servo_settings_t) + 1 <= EEPROM_ADDR_PARAMETERS) ? 1 : -1];
command_servo_settings_t command_servo_settings;

static void save_servo_settings(void)
{
  memcpy_to_eeprom_with_checksum(COMMAND_SERVO_EEPROM_ADDR,
    (char *)&command_servo_settings, sizeof(command_servo_settings));
}

void command_servo_settings_restore(void)
{
  command_servo_settings.version = 1;
  command_servo_settings.reserved = 0;
  command_servo_settings.closed_degrees = 0;
  command_servo_settings.open_degrees = 90;
  save_servo_settings();
}

void command_servo_settings_load(void)
{
  if (!memcpy_from_eeprom_with_checksum((char *)&command_servo_settings,
      COMMAND_SERVO_EEPROM_ADDR, sizeof(command_servo_settings)) ||
      command_servo_settings.version != 1 ||
      command_servo_settings.closed_degrees > 180 || command_servo_settings.open_degrees > 180) {
    command_servo_settings_restore();
  }
}

uint8_t command_servo_store_setting(uint8_t parameter, float value)
{
  if (!isfinite(value) || value != truncf(value)) { return(STATUS_INVALID_STATEMENT); }
  if (sys.state != STATE_IDLE || plan_get_current_block() != NULL) { return(STATUS_IDLE_ERROR); }
  if (value < 0 || value > 180) { return(STATUS_INVALID_STATEMENT); }
  if (parameter == 141) { command_servo_settings.closed_degrees = (uint8_t)value; }
  else if (parameter == 142) { command_servo_settings.open_degrees = (uint8_t)value; }
  else { return(STATUS_INVALID_STATEMENT); }
  save_servo_settings();
  command_servo_set(current_open);
  return(STATUS_OK);
}

void command_servo_init(void)
{
  uint8_t saved_sreg = SREG;
  cli();
  TIMSK2 &= ~((1<<TOIE2) | (1<<OCIE2B));
  PORTC &= ~(1<<PC4);
  DDRC |= (1<<PC4);
  current_open = 0; // Power-up and soft reset return to closed.
  command_servo_set(0);
  frame = 18; // Prepare buffered compare before the first pulse.
  pulse_end_frame = 0;
  TIFR2 = (1<<TOV2) | (1<<OCF2B);
  TIMSK2 |= (1<<TOIE2) | (1<<OCIE2B);
  SREG = saved_sreg;
}

void command_servo_set(uint8_t open)
{
  current_open = open;
  uint8_t degrees = open ? command_servo_settings.open_degrees : command_servo_settings.closed_degrees;
  uint16_t ticks = (COMMAND_SERVO_MIN_US +
    ((uint32_t)(COMMAND_SERVO_MAX_US-COMMAND_SERVO_MIN_US)*degrees)/180)/4;
  uint8_t saved_sreg = SREG;
  cli();
  requested_ticks = ticks; // Atomic update; latch only at the next frame.
  SREG = saved_sreg;
}

ISR(TIMER2_OVF_vect)
{
  ++frame;
  if (frame == 19) {
    // Fast PWM buffers OCR2B until BOTTOM. Prepare one overflow early.
    uint16_t ticks = requested_ticks;
    pulse_end_frame = ticks >> 8;
    OCR2B = (uint8_t)ticks;
  } else if (frame == 20) {
    frame = 0;
    PORTC |= (1<<PC4);
  }
  // A compare at BOTTOM runs before this overflow ISR; handle exact
  // multiples of 256 ticks here after advancing the frame counter.
  if (frame == pulse_end_frame && OCR2B == 0) { PORTC &= ~(1<<PC4); }
}

ISR(TIMER2_COMPB_vect)
{
  if (frame == pulse_end_frame) { PORTC &= ~(1<<PC4); }
}
#endif
