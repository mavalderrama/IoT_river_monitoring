DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

# all: mqtt-example
CONTIKI_PROJECT = node1
all: $(CONTIKI_PROJECT)
PROJECT_SOURCEFILES += add-on.c
APPS += mqtt

# Linker size optimization
SMALL = 1

CONTIKI_WITH_IPV6 = 1

# CONTIKI = /home/fito/contiki
include ../contiki.location

include $(CONTIKI)/Makefile.include
