#
# Makefile for libmediasession.so
#
#

TARGET = libmediasession.so

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -lmediahal_tsplayer

# build
all: $(TARGET)
	install -m 0755 $(TARGET) $(STAGING_DIR)/usr/lib/
	install -m 0755 mediasession.h $(STAGING_DIR)/usr/include/

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -shared -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean install uninstall

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(TARGET_DIR)/usr/lib/

uninstall:
	rm $(TARGET_DIR)/usr/lib/$(TARGET)
	rm $(STAGING_DIR)/usr/lib/$(TARGET)
	rm $(STAGING_DIR)/usr/include/mediasession.h
