#
## Makefile for libgstamltspvsink.so
#
#

TARGET=libgstamltspvsink.so
SRCS=$(wildcard *.c)
OBJS=$(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
export PKG_CONFIG_PATH=$(TARGET_DIR)/../host/$(CROSSCOMPILE)/sysroot/usr/lib/pkgconfig
export PKG_CONFIG=$(TARGET_DIR)/../host/bin/pkg-config
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0)
LDFLAGS += -lmediahal_tsplayer
LDFLAGS += -lmediasession

# rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -shared -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(TARGET_DIR)/usr/lib/gstreamer-1.0/

uninstall:
	rm $(TARGET_DIR)/usr/lib/gstreamer-1.0/$(TARGET)
