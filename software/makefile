UNAME := $(shell uname -n)
ifeq ($(UNAME),raspberrypi)
LDFLAGS += -lpigpio
CFLAGS += -DRASPBERRY
else
ADD_OBJS = linux/portable.o
LDFLAGS += 
endif

src = $(wildcard *.c)
obj = $(src:.c=.o) $(ADD_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -I. -o $@ -c $<

romhat: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) romhat


