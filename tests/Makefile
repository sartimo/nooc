#
# New Object Oriented C Makefile - tests
#

TOP = ..
include $(TOP)/Makefile
VPATH = $(TOPSRC)/tests $(TOPSRC) $(TOP)
CFLAGS := $(filter-out -g% -O%,$(CFLAGS)) -I$(TOPSRC) $(LDFLAGS)

# what tests to run
TESTS = \
 hello-exe \
 hello-run \
 libtest \
 libtest_mt \
 test3 \
 memtest \
 dlltest \
 abitest \
 asm-c-connect-test \
 vla_test-run \
 cross-test \
 tests2-dir \
 pp-dir

# test4_static -- Not all relocation types are implemented yet.
# asmtest / asmtest2 -- minor differences with gcc

ifneq ($(CONFIG_bcheck),no)
 TESTS += btest test1b
endif
ifeq ($(CONFIG_dll),no)
 TESTS := $(filter-out dlltest, $(TESTS))
endif
ifeq (-$(CONFIG_arm_eabi)-$(CONFIG_arm_vfp)-,-yes--)
 TESTS := $(filter-out test3 test1b,$(TESTS))
endif
ifeq (,$(filter i386 x86_64,$(ARCH)))
 TESTS := $(filter-out asm-c-connect-test,$(TESTS))
endif
ifeq ($(OS),Windows_NT) # for libnooc_test to find libnooc.dll
 PATH := $(CURDIR)/$(TOP)$(if $(findstring ;,$(PATH)),;,:)$(PATH)
endif
ifdef CONFIG_OSX
LIBS += $(LINK_LIBNOOC)
endif
ifeq ($(ARCH),arm)
# nooctest refers to the alignment of functions, and with thumb mode
# the low bit of code addresses selects the mode, so the "alignment"
# of functions via bit masking comes out as 1.  Just disable thumb.
test.ref: CFLAGS+=-marm
endif
ifeq ($(ARCH)$(CONFIG_WIN32),i386)
# nooctest.c:get_asm_string uses a construct that is checked too strictly
# by GCC in 32bit mode when PIC is enabled.
test.ref: CFLAGS+=-fno-PIC -fno-PIE -Wl,-z,notext
endif
ifeq ($(CC_NAME),msvc)
test.ref abitest : CC = gcc
endif
ifeq ($(TARGETOS),OpenBSD)
dlltest: CFLAGS+=-fno-stack-protector
endif
ifneq (,$(filter FreeBSD NetBSD,$(TARGETOS)))
  # test3 has dlsym problems
  TESTS := $(filter-out test3,$(TESTS))
  TESTS += test1
endif

RUN_NOOC = $(NATIVE_DEFINES) -run $(TOPSRC)/nooc.c $(NOOCFLAGS)
DISAS = objdump -d
ifdef CONFIG_OSX
DUMPNOOC = (set -x; $(TOP)/nooc -vv; otool -L $(TOP)/nooc; exit 1)
else
DUMPNOOC = (set -x; $(TOP)/nooc -vv; ldd $(TOP)/nooc; exit 1)
endif

all test :
	@$(MAKE) --no-print-directory -s clean
	@$(MAKE) --no-print-directory -s -r $(TESTS)

hello-exe: ../examples/ex1.c
	@echo ------------ $@ ------------
	$(NOOC) $< -o hello$(EXESUF) && ./hello$(EXESUF) || $(DUMPNOOC)

hello-run: ../examples/ex1.c
	@echo ------------ $@ ------------
	$(NOOC) -run $< || $(DUMPNOOC)

libtes%: libnooc_tes%$(EXESUF)
	@echo ------------ $@ ------------
	./libnooc_tes$*$(EXESUF) $(TOPSRC)/nooc.c $(NOOCFLAGS) $(NATIVE_DEFINES)

libnooc_test$(EXESUF): libnooc_test.c $(LIBNOOC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

libnooc_test_mt$(EXESUF): libnooc_test_mt.c $(LIBNOOC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%-dir:
	@echo ------------ $@ ------------
	$(MAKE) -k -C $*

# test.ref - generate using cc
test.ref: nooctest.c
	$(CC) -o nooctest.gcc $< $(NATIVE_DEFINES) $(CFLAGS) -w -O0 -std=gnu99 -fno-omit-frame-pointer
	./nooctest.gcc > $@

# auto test
test1 test1b: nooctest.c test.ref
	@echo ------------ $@ ------------
	$(NOOC) $(RUN_NOOC) -w -run $< > test.out1
	@diff -u test.ref test.out1 && echo "$(AUTO_TEST) OK"

# iterated test2 (compile nooc then compile nooctest.c !)
test2 test2b: nooctest.c test.ref
	@echo ------------ $@ ------------
	$(NOOC) $(RUN_NOOC) $(RUN_NOOC) -w -run $< > test.out2
	@diff -u test.ref test.out2 && echo "$(AUTO_TEST)2 OK"

# iterated test3 (compile nooc then compile nooc then compile nooctest.c !)
test3 test3b: nooctest.c test.ref
	@echo ------------ $@ ------------
	$(NOOC) $(RUN_NOOC) $(RUN_NOOC) $(RUN_NOOC) -w -run $< > test.out3
	@diff -u test.ref test.out3 && echo "$(AUTO_TEST)3 OK"

AUTO_TEST = Auto Test
test%b : NOOCFLAGS += -b -bt1
test%b : AUTO_TEST = Auto Bound-Test

# binary output test
test4: nooctest.c test.ref
	@echo ------------ $@ ------------
# object + link output
	$(NOOC) -c -o nooctest3.o $<
	$(NOOC) -o nooctest3 nooctest3.o
	./nooctest3 > test3.out
	@if diff -u test.ref test3.out ; then echo "Object $(AUTO_TEST) OK"; fi
# dynamic output
	$(NOOC) -o nooctest1 $<
	./nooctest1 > test1.out
	@if diff -u test.ref test1.out ; then echo "Dynamic $(AUTO_TEST) OK"; fi
# dynamic output + bound check
	$(NOOC) -b -o nooctest4 $<
	./nooctest4 > test4.out
	@if diff -u test.ref test4.out ; then echo "BCheck $(AUTO_TEST) OK"; fi

test4_static: nooctest.c test.ref
	@echo ------------ $@ ------------
# static output.
	$(NOOC) -static -o nooctest2 $<
	./nooctest2 > test2.out
	@if diff -u test.ref test2.out ; then echo "Static $(AUTO_TEST) OK"; fi

# use nooc to create libnooc.so/.dll and the nooc(.exe) frontend and run them
dlltest:
	@echo ------------ $@ ------------
	$(NOOC) $(NATIVE_DEFINES) -DLIBNOOC_AS_DLL $(TOPSRC)/libnooc.c $(LIBS) -shared -o libnooc2$(DLLSUF)
	$(NOOC) $(NATIVE_DEFINES) -DONE_SOURCE=0 $(TOPSRC)/nooc.c libnooc2$(DLLSUF) $(LIBS) -Wl,-rpath=. -o nooc2$(EXESUF)
	./nooc2$(EXESUF) $(NOOCFLAGS) $(RUN_NOOC) -run $(TOPSRC)/examples/ex1.c
ifeq (,$(filter Darwin WIN32,$(TARGETOS)))
	@echo ------------ $@ with PIC ------------
	$(CC) $(CFLAGS) -fPIC $(NATIVE_DEFINES) -DLIBNOOC_AS_DLL -c $(TOPSRC)/libnooc.c
	$(NOOC) libnooc.o $(LIBS) -shared -o libnooc2$(DLLSUF)
	$(NOOC) $(NATIVE_DEFINES) -DONE_SOURCE=0 $(TOPSRC)/nooc.c libnooc2$(DLLSUF) $(LIBS) -Wl,-rpath=. -o nooc2$(EXESUF)
	./nooc2$(EXESUF) $(NOOCFLAGS) $(RUN_NOOC) -run $(TOPSRC)/examples/ex1.c
endif
	@rm nooc2$(EXESUF) libnooc2$(DLLSUF)

memtest:
	@echo ------------ $@ ------------
	$(CC) $(CFLAGS) $(NATIVE_DEFINES) -DMEM_DEBUG=2 $(TOPSRC)/nooc.c $(LIBS) -o memtest-nooc$(EXESUF)
	./memtest-nooc$(EXESUF) $(NOOCFLAGS) $(NATIVE_DEFINES) $(TOPSRC)/nooc.c $(LIBS)
	./memtest-nooc$(EXESUF) $(NOOCFLAGS) $(NATIVE_DEFINES) -run $(TOPSRC)/nooc.c $(NOOCFLAGS) -w $(TOPSRC)/tests/nooctest.c
	@echo OK

# memory and bound check auto test
BOUNDS_OK  = 1 4 8 10 14 16
BOUNDS_FAIL= 2 5 6 7 9 11 12 13 15 17 18

btest: boundtest.c
	@echo ------------ $@ ------------
	@for i in $(BOUNDS_OK); do \
	   if $(NOOC) -b -run $< $$i >/dev/null 2>&1 ; then \
	       echo "Test $$i succeeded as expected" ; \
	   else\
	       echo "Failed positive test $$i" ; exit 1 ; \
	   fi ;\
	done ;\
	for i in $(BOUNDS_FAIL); do \
	   if $(NOOC) -b -bt1 -run $< $$i >/dev/null 2>&1 ; then \
	       echo "Failed negative test $$i" ; exit 1 ;\
	   else\
	       echo "Test $$i failed as expected" ; \
	   fi ;\
	done ;\
	echo Bound test OK

# speed test
speedtest: ex2 ex3
	@echo ------------ $@ ------------
	time ./ex2 1238 2 3 4 10 13 4
	time $(NOOC) -run $(TOPSRC)/examples/ex2.c 1238 2 3 4 10 13 4
	time ./ex3 35
	time $(NOOC) -run $(TOPSRC)/examples/ex3.c 35

weaktest: nooctest.c test.ref
	@echo ------------ $@ ------------
	$(NOOC) -c $< -o weaktest.nooc.o
	$(CC) -c $< -o weaktest.gcc.o $(NATIVE_DEFINES) $(CFLAGS) -w -O0 -std=gnu99 -fno-omit-frame-pointer
	objdump -t weaktest.nooc.o | grep ' w ' | sed -e 's/.* \([a-zA-Z0-9_]*\)$$/\1/' | LC_ALL=C sort > weaktest.nooc.o.txt
	objdump -t weaktest.gcc.o | grep ' w ' | sed -e 's/.* \([a-zA-Z0-9_]*\)$$/\1/' | LC_ALL=C sort > weaktest.gcc.o.txt
	diff weaktest.gcc.o.txt weaktest.nooc.o.txt && echo "Weak Auto Test OK"

ex%: $(TOPSRC)/examples/ex%.c
	$(CC) -o $@ $< $(CFLAGS)

# tiny assembler testing
asmtest.ref: asmtest.S
	$(CC) -Wa,-W -Wa,-mx86-used-note=no -o asmtest.ref.o -c asmtest.S
	objdump -D asmtest.ref.o > asmtest.ref

ifeq ($(ARCH),arm)
asmtest asmtest2:
	NOOC="${NOOC}" ./arm-asm-testsuite.sh
else
asmtest asmtest2: asmtest.ref
	@echo ------------ $@ ------------
	$(NOOC) $(MAYBE_RUN_NOOC) -c asmtest.S
	objdump -D asmtest.o > asmtest.out
	@if diff -u --ignore-matching-lines="file format" asmtest.ref asmtest.out ; then echo "ASM Auto Test OK"; fi
endif

# test assembler with nooc compiled by itself
asmtest2: MAYBE_RUN_NOOC = $(RUN_NOOC)

# Check that code generated by libnooc is binary compatible with
# that generated by CC
abitest-cc.exe: abitest.c $(LIBNOOC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) -w

abitest-nooc.exe: abitest.c libnooc.c
	$(NOOC) -o $@ $^ $(NATIVE_DEFINES) $(LIBS)

abitest-% : abitest-%.exe
	@echo ------------ $@ ------------
	./$< $(NOOCFLAGS)

abitest: abitest-cc
ifneq (-$(CONFIG_arm_eabi)-$(CONFIG_arm_vfp)-,-yes--)
abitest: abitest-nooc
endif

vla_test$(EXESUF): vla_test.c
	$(NOOC) -o $@ $^

vla_test-run: vla_test$(EXESUF)
	@echo ------------ $@ ------------
	./vla_test$(EXESUF)

.PHONY: abitest vla_test

asm-c-connect$(EXESUF): asm-c-connect-1.c asm-c-connect-2.c
	$(NOOC) -o $@ $^

asm-c-connect-%.o: asm-c-connect-%.c
	$(NOOC) -c -o $@ $<

asm-c-connect-sep$(EXESUF): asm-c-connect-1.o asm-c-connect-2.o
	$(NOOC) -o $@ $^

asm-c-connect-test: asm-c-connect$(EXESUF) asm-c-connect-sep$(EXESUF)
	@echo ------------ $@ ------------
	./asm-c-connect$(EXESUF) > asm-c-connect.out1 && cat asm-c-connect.out1
	./asm-c-connect-sep$(EXESUF) > asm-c-connect.out2 && cat asm-c-connect.out2
	@diff -u asm-c-connect.out1 asm-c-connect.out2 || (echo "error"; exit 1)

# quick sanity check for cross-compilers
cross-test : nooctest.c examples/ex3.c
	@echo ------------ $@ ------------
	$(foreach T,$(CROSS-TGTS),$(call CROSS-COMPILE,$T))

CROSS-TGTS = \
 i386 \
 i386-win32 \
 i386-OpenBSD \
 x86_64 \
 x86_64-win32 \
 x86_64-osx \
 x86_64-FreeBSD \
 x86_64-NetBSD \
 x86_64-OpenBSD \
 arm-fpa \
 arm-eabihf \
 arm-NetBSD \
 arm-wince \
 arm64 \
 arm64-osx \
 arm64-FreeBSD \
 arm64-NetBSD \
 arm64-OpenBSD \
 riscv64 \
 c67

define CROSS-COMPILE
 @echo " . $(1)"
 $(NOOC) $(DEF-$1) -DNOOC_CROSS_TEST -run $(TOPSRC)/nooc.c \
    -c $(if $(findstring c67,$1),$(filter %/ex3.c,$^),$<) -w $(NOOCFLAGS)

endef

# targets for development
%.bin: %.c nooc
	$(NOOC) -g -o $@ $<
	$(DISAS) $@

instr: instr.o
	objdump -d instr.o

instr.o: instr.S
	$(CC) -o $@ -c $< -O2 -Wall -g

cache: nooc_g
	cachegrind ./nooc_g -o /tmp/linpack -lm bench/linpack.c
	vg_annotate nooc.c > /tmp/linpack.cache.log

# clean
clean:
	rm -f *~ *.o *.a *.bin *.i *.ref *.out *.out? *.out?b *.cc *.gcc
	rm -f *-cc *-gcc *-nooc *.exe hello libnooc_test vla_test nooctest[1234]
	rm -f asm-c-connect$(EXESUF) asm-c-connect-sep$(EXESUF)
	rm -f ex? nooc_g weaktest.*.txt *.def *.pdb *.obj libnooc_test_mt
	@$(MAKE) -C tests2 $@
	@$(MAKE) -C pp $@

