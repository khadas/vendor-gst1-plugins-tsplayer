#
## Makefile for libgstamltspvsink.so
#
#

TARGET=libgstamltspvsink.so
SRCS=$(wildcard *.c)
OBJS=$(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-plugins-bad-1.0)
CFLAGS += -I$(STAGING_DIR)/usr/include/
CFLAGS += $(EXT_CFLAGS)

LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-plugins-bad-1.0) -lgstcodecparsers-1.0
LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -lmediahal_tsplayer
LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -lmediasession
LDFLAGS += $(EXT_LDFLAGS)

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
