# Makefile for building the Nagios MQTT NEB module

CC = gcc
CFLAGS = -fPIC -Wall -I$(NAGIOS_SRC_DIR) -I$(NAGIOS_SRC_DIR)/include -I$(NAGIOS_SRC_DIR)/lib -I/usr/include/cjson
LDFLAGS = -shared -lmosquitto -lcjson
TARGET = nagios2mqtt.so
SRC = nagios2mqtt.c

# Nagios source code variables
NAGIOS_VERSION = 4.4.14
NAGIOS_TAR = nagios-$(NAGIOS_VERSION).tar.gz
NAGIOS_SRC_URL = https://assets.nagios.com/downloads/nagioscore/releases/$(NAGIOS_TAR)
NAGIOS_SRC_DIR = nagios-$(NAGIOS_VERSION)

all: $(TARGET)

$(TARGET): $(SRC) $(NAGIOS_SRC_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(NAGIOS_SRC_DIR):
	wget $(NAGIOS_SRC_URL)
	tar -xzf $(NAGIOS_TAR)
	cp   $(NAGIOS_SRC_DIR)/lib/iobroker.h.in  $(NAGIOS_SRC_DIR)/lib/iobroker.h
	cp   $(NAGIOS_SRC_DIR)/include/locations.h.in  $(NAGIOS_SRC_DIR)/include/locations.h
	touch $(NAGIOS_SRC_DIR)/lib/snprintf.h


deps:
	sudo apt-get update
	sudo apt-get install -y libmosquitto-dev libcjson-dev wget

clean:
	rm -f $(TARGET)
	rm -rf $(NAGIOS_SRC_DIR)
	rm -f $(NAGIOS_TAR)

install:
	cp $(TARGET) /usr/lib/nagios/plugins/

uninstall:
	rm -f /usr/lib/nagios/plugins/$(TARGET)

