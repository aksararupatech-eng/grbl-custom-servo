# Hobby servo commands (Arduino Uno)

Recompile and upload examples/grblUpload/grblUpload.ino with this Grbl library.

* `$OS`: open, nominal 90 degrees (1500 microsecond pulse).
* `$TS`: close, nominal 0 degrees (1000 microsecond pulse).
* `$141=0`: closed angle, whole degrees from 0 through 180 (default 0).
* `$142=90`: open angle, whole degrees from 0 through 180 (default 90).
  Both angles may be adjusted independently, including reversing their order.
* `$$`: show both angle settings. They persist across power cycles in a separate
  EEPROM record without changing the original Grbl settings layout/version.
  `$RST=$` and `$RST=*` restore servo defaults too.
* Signal: Arduino **A4 / PC4**, also usable as digital pin 18.
  Set by `COMMAND_SERVO_PIN 18` in config.h, with no runtime pin setting.
  Only A4 is currently supported; other pins conflict with existing Grbl uses.
  Identify the actual A4 header/pad on your CNC shield; shield layouts vary.
  Do not use an X/Y/Z step, direction, or limit header for this signal.
* Servo power: a regulated supply appropriate for the servo's voltage and
  current. Connect supply ground, servo ground, and Arduino ground together.
  Do not connect servo power to the CNC shield motor-voltage terminal.

Commands must match exactly and return `ok` when accepted. Send them from
Candle's command console after homing (or unlocking if appropriate). They are
accepted only in Idle with no queued motion and the safety door closed.
They are not synchronized G-code accessory commands: wait for Idle before
sending them. `ok` means the target was accepted, not that physical movement
has finished. `$C` retains its existing check-mode behavior; `$CS` is not used.
Servo setting writes also require Idle with no queued motion. Editing
the angle for the current target immediately updates its pulse width.

The servo starts CLOSED at power-up and returns CLOSED on a Grbl soft reset.
It continues holding its last target during an alarm or feed hold; these
conditions do not disconnect servo power. Test with the linkage disconnected
first because power-up/reset can move it. Calibrate COMMAND_SERVO_MIN_US and
COMMAND_SERVO_MAX_US in config.h (nominal 0 and 180 degree endpoints, default
1000 and 2000 microseconds) to suit the actual servo and linkage; angles
are nominal and there is no position feedback.

Implementation uses Timer2 overflow and compare-B interrupts for a 20.48 ms
frame on A4. Timer0/Timer1 remain dedicated to the steppers, and Timer2's
existing spindle PWM on D11 remains available. Servo pulses have some software
interrupt jitter; physical operation still needs testing on the machine.
Do not add Arduino Servo or analogWrite calls. M7 and firmware ENABLE_DUAL_AXIS
are incompatible with this A4 assignment and are checked at compile time.
Hardware jumper cloning of Y to the shield's A driver remains usable.

To remove this feature, comment out ENABLE_COMMAND_SERVO in config.h.
