#
# Makefile for libgstamltspasink.so
#
#

TARGET = libgstamltspasink.so

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
CFLAGS += \
	$(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0) \
	-I$(STAGING_DIR)/usr/include/

LDFLAGS += \
	$(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0) \
	-L$(STAGING_DIR)/usr/lib/ -lmediasession \
	-L$(STAGING_DIR)/usr/lib/ -lmediahal_tsplayer

# build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -shared -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean install uninstall

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(TARGET_DIR)/usr/lib/gstreamer-1.0/

uninstall:
	rm $(TARGET_DIR)/usr/lib/gstreamer-1.0/$(TARGET)
