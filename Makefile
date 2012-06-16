
TGTS := testpvc testshortsrv testshortclt

CC := gcc
CFLAGS := -Wall -g -O0
LDADD := -lpthread

.PHONY: all test clean

all: $(TGTS)

$(TGTS): pvc.c pvc.h data.c data.h
testpvc: testpvc.c
testshortsrv: testshortsrv.c
testshortclt: testshortclt.c sender.c sender.h recver.c recver.h

$(TGTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDADD)

test: $(TGTS)
	./ntestpvc.sh /dev/null 2000

clean:
	-rm -rf $(TGTS) $(wildcard *.o *.dSYM)

