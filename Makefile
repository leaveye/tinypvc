
TGTS := testpvc testshortclt testshortsrv

PROFILE?=0

CC := gcc
CFLAGS := -Wall -g -O0 -DPROFILE=$(PROFILE)
LDADD := -lpthread

.PHONY: all check clean

all: $(TGTS)

$(TGTS): pvc.c pvc.h data.c data.h
testpvc: testpvc.c
testshortsrv: testshortsrv.c
testshortclt: testshortclt.c sender.c sender.h recver.c recver.h

$(TGTS):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c %.o,$^) $(LDADD)

.PHONY: $(addprefix check-,$(TGTS))
check: $(addprefix check-,$(TGTS))
$(addprefix check-,$(TGTS)): check-%: %
check-testpvc:
	./runtest.sh ./$< 2000 /dev/null

clean:
	-rm -rf $(TGTS) $(wildcard *.o *.dSYM)

