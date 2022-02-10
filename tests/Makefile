#
# Makefile for gst_test
#
#

TARGET = gst_test

SRCS = $(wildcard *.c)
OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Werror -fPIC -disable-static

export PKG_CONFIG_PATH=$(TARGET_DIR)/../host/$(CROSSCOMPILE)/sysroot/usr/lib/pkgconfig
export PKG_CONFIG=$(TARGET_DIR)/../host/bin/pkg-config

CFLAGS += \
	$(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0) \
	-I$(TARGET_DIR)/target/usr/include

LDFLAGS += \
	$(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0) \
	-L$(TARGET_DIR)/target/usr/lib

LDFLAGS += -lpthread

# build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean install uninstall

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(TARGET_DIR)/usr/bin/

uninstall:
	rm $(TARGET_DIR)/usr/bin/$(TARGET)
