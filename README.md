# samd21-gpio_high_and_low_durations
This arduino code for SAMD21 will record a gpio's high and low duration for multiple pulses. Each high and low duration will be captured up to 4 seconds in duration. One microsecond pulses can be reliably captured (faster than that is not possilbe with the samd21, use the samd51 which has a faster clock).

There are two uf2 files. The one without a suffix is for Seeeduino Xiao and the one with the qt_py suffix is for the Adafruit QT PY. On both boards pin #7 (A7/D7/RX) is used to measure the pulse widths. On Xiao this pin is PB09 and on QT PY it is PA07.
