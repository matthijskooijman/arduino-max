ARDUINO_LIBS = SPI RF22 LiquidCrystal_I2C Wire Wire/utility TStreaming Ethernet Ethernet/utility
#BOARD_TAG    = uno
#ARDUINO_PORT = /dev/ttyACM*
BOARD_TAG    = ethernet
ARDUINO_PORT = /dev/ttyUSB0

# Default to uploading
#upload:

include $(ARDMK_DIR)/arduino-mk/Arduino.mk

CXXFLAGS+=-std=c++11 -flto
LDFLAGS+=-flto

# Make sure the tty is freed before starting an upload
# This probably does not work with a clean Arduino-mk, but needs some
# minor patches
do_upload: size free_tty

TTY_PIDS=$(shell lsof -t $(ARDUINO_PORT))

free_tty:
	[ -z "$(TTY_PIDS)" ] || (kill $(TTY_PIDS) && sleep 3)

.PHONY: free_tty

