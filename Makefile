
TGTS := testpvc testshortclt testshortsrv

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

.PHONY: $(addprefix runtest-,$(TGTS))
test: $(addprefix runtest-,$(TGTS))
$(addprefix runtest-,$(TGTS)): runtest-%: %
$(addprefix runtest-,testpvc testshortclt):
	./runtest.sh ./$< 2000 /dev/null

clean:
	-rm -rf $(TGTS) $(wildcard *.o *.dSYM)

