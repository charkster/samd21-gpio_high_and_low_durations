# samd21-gpio_high_and_low_durations
This arduino code for SAMD21 will record a gpio's high and low duration for multiple pulses. Each high and low duration will be captured up to 4 seconds in duration. One microsecond pulses can be reliably captured (faster than that is not possilbe with the samd21, use the samd51 which has a faster clock).

The uf2 file works for both Seeeduino Xiao and Adafruit QT PY. SAMD21 pin PA7 is used to measure the pulse widths. On Xiao this pin is A8/D8/SCK and on QT PY it is A7/D7/RX.
