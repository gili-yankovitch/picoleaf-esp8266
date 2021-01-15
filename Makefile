# Arduino Make file. Refer to https://github.com/sudar/Arduino-Makefile

# BOARD_TAG    = pro5v328
# include $(ARDMK_DIR)/Arduino.mk

# --- pro mini
# BOARD_TAG    = pro5v328
# BOARD_TAG    = pro328
# MONITOR_PORT = /dev/ttyUSB1
# NO_CORE_MAIN_CPP = 1
# include /usr/share/arduino/Arduino.mk

# ESP8266 flash
UPLOAD_PORT = /dev/ttyUSB1
include /usr/share/arduino/hardware/espressif/makeEspArduino.mk
