CC := aarch64-linux-gnu-gcc
CFLAGS := --std=c11 -Wall -Wextra -Wpedantic -Og -g -static

TMP := tmp

SOURCES := $(wildcard *.c)
BINS := $(SOURCES:.c=)

.PHONY: all
all: $(BINS)

.PHONY: push
push: $(TMP)

$(TMP): $(BINS)
	mkdir -p $(TMP)
	sudo mount ../tizen-image/rootfs.img $(TMP)
	sudo cp $(BINS) "$(TMP)/root/"
	sudo umount "$(TMP)"
	touch $(TMP)

.PHONY: clean
clean:
	rm -rf $(BINS) $(TMP)
