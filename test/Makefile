CC := aarch64-linux-gnu-gcc
BINS := test_ptree test_ptree_null test_ptree_zero testproc_nonmain_fork

.PHONY: all
all: $(BINS)

.PHONY: clean
clean:
	rm -rf $(BINS)

%: %.c
	$(CC) $^ -static -o $@ -lpthread
