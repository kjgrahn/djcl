#
# Makefile
#
# Copyright (c) 2010--2024 Jörgen Grahn
# All rights reserved.

SHELL=/bin/bash
INSTALLBASE=/usr/local
CXXFLAGS=-Wall -Wextra -pedantic -std=c++17 -g -Os -Wold-style-cast
CPPFLAGS=
ARFLAGS=rTP

.PHONY: all
all: djcl
all: tests

.PHONY: install
install: djcl djcl.1
	install -m755 djcl $(INSTALLBASE)/bin/
	install -m644 djcl.1 $(INSTALLBASE)/man/man1/

.PHONY: check checkv
check: tests
	./tests
checkv: tests
	valgrind -q ./tests -v

libjcl.a: split.o
libjcl.a: timepoint.o
libjcl.a: log.o
libjcl.a: parent.o
libjcl.a: schedule.o
libjcl.a: sigpipe.o
libjcl.a: pipes.o
libjcl.a: spider.o
libjcl.a: textread.o
libjcl.a: sigpipe.o
	$(AR) $(ARFLAGS) $@ $^

djcl: djcl.o libjcl.a
	$(CXX) $(CXXFLAGS) -o $@ djcl.o -L. -ljcl

libtest.a: test/log.o
libtest.a: test/stealfd.o
libtest.a: test/split.o
	$(AR) $(ARFLAGS) $@ $^

test/%.o: CPPFLAGS+=-I.

test.cc: libtest.a
	orchis -o$@ $^

tests: test.o libjcl.a libtest.a
	$(CXX) $(CXXFLAGS) -o $@ test.o -L. -ltest -ljcl

.PHONY: tags TAGS
tags: TAGS
TAGS:
	ctags --exclude='test' -eR . --extra=q

.PHONY: clean
clean:
	$(RM) djcl
	$(RM) {,test/}*.o
	$(RM) lib*.a
	$(RM) test.cc tests
	$(RM) TAGS
	$(RM) -r dep/

love:
	@echo "not war?"

$(shell mkdir -p dep/test)
DEPFLAGS=-MT $@ -MMD -MP -MF dep/$*.Td
COMPILE.cc=$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o: %.cc
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	@mv dep/$*.{Td,d}

dep/%.d: ;
dep/test/%.d: ;
-include dep/*.d
-include dep/test/*.d
