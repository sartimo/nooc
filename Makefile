# --------------------------------------------------------------------------
#
# New Object Oriented C Makefile
#

ifndef TOP
 TOP = .
 INCLUDED = no
endif

ifeq ($(findstring $(MAKECMDGOALS),clean distclean),)
 include $(TOP)/config.mak
endif

ifeq (-$(GCC_MAJOR)-$(findstring $(GCC_MINOR),56789)-,-4--)
 CFLAGS += -D_FORTIFY_SOURCE=0
endif

LIBNOOC = libnooc.a
LIBNOOC1 = libnooc1.a
LINK_LIBNOOC =
LIBS =
CFLAGS += -I$(TOP) -Wunused-variable -Wmultichar
CFLAGS += $(CPPFLAGS)
VPATH = $(TOPSRC)

ifdef CONFIG_WIN32
 CFG = -win
 ifneq ($(CONFIG_static),yes)
  LIBNOOC = libnooc$(DLLSUF)
  LIBNOOCDEF = libnooc.def
 endif
 ifneq ($(CONFIG_debug),yes)
  LDFLAGS += -s
 endif
 NATIVE_TARGET = $(ARCH)-win$(if $(findstring arm,$(ARCH)),ce,32)
else
 CFG = -unx
 LIBS+=-lm
 ifneq ($(CONFIG_ldl),no)
  LIBS+=-ldl
 endif
 ifneq ($(CONFIG_pthread),no)
  LIBS+=-lpthread
 endif
 # make libnooc as static or dynamic library?
 ifeq ($(CONFIG_static),no)
  LIBNOOC=libnooc$(DLLSUF)
  export LD_LIBRARY_PATH := $(CURDIR)/$(TOP)
  ifneq ($(CONFIG_rpath),no)
    ifndef CONFIG_OSX
      LINK_LIBNOOC += -Wl,-rpath,"$(libdir)"
    else
      # macOS doesn't support env-vars libdir out of the box - which we need for
      # `make test' when libnooc.dylib is used (configure --disable-static), so
      # we bake a relative path into the binary. $libdir is used after install.
      LINK_LIBNOOC += -Wl,-rpath,"@executable_path/$(TOP)" -Wl,-rpath,"$(libdir)"
      DYLIBVER += -current_version $(VERSION)
      DYLIBVER += -compatibility_version $(VERSION)
    endif
  endif
 endif
 NATIVE_TARGET = $(ARCH)
 ifdef CONFIG_OSX
  NATIVE_TARGET = $(ARCH)-osx
  ifneq ($(CC_NAME),nooc)
    LDFLAGS += -flat_namespace -undefined warning
  endif
  export MACOSX_DEPLOYMENT_TARGET := 10.6
 endif
endif

# run local version of nooc with local libraries and includes
NOOCFLAGS-unx = -B$(TOP) -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP)
NOOCFLAGS-win = -B$(TOPSRC)/win32 -I$(TOPSRC)/include -I$(TOPSRC) -I$(TOP) -L$(TOP)
NOOCFLAGS = $(NOOCFLAGS$(CFG))
NOOC_LOCAL = $(TOP)/nooc$(EXESUF)
NOOC = $(NOOC_LOCAL) $(NOOCFLAGS)

CFLAGS_P = $(CFLAGS) -pg -static -DCONFIG_NOOC_STATIC -DNOOC_PROFILE
LIBS_P = $(LIBS)
LDFLAGS_P = $(LDFLAGS)

CONFIG_$(ARCH) = yes
NATIVE_DEFINES_$(CONFIG_i386) += -DNOOC_TARGET_I386
NATIVE_DEFINES_$(CONFIG_x86_64) += -DNOOC_TARGET_X86_64
NATIVE_DEFINES_$(CONFIG_WIN32) += -DNOOC_TARGET_PE
NATIVE_DEFINES_$(CONFIG_OSX) += -DNOOC_TARGET_MACHO
NATIVE_DEFINES_$(CONFIG_uClibc) += -DNOOC_UCLIBC
NATIVE_DEFINES_$(CONFIG_musl) += -DNOOC_MUSL
NATIVE_DEFINES_$(CONFIG_libgcc) += -DCONFIG_USE_LIBGCC
NATIVE_DEFINES_$(CONFIG_selinux) += -DHAVE_SELINUX
NATIVE_DEFINES_$(CONFIG_arm) += -DNOOC_TARGET_ARM
NATIVE_DEFINES_$(CONFIG_arm_eabihf) += -DNOOC_ARM_EABI -DNOOC_ARM_HARDFLOAT
NATIVE_DEFINES_$(CONFIG_arm_eabi) += -DNOOC_ARM_EABI
NATIVE_DEFINES_$(CONFIG_arm_vfp) += -DNOOC_ARM_VFP
NATIVE_DEFINES_$(CONFIG_arm64) += -DNOOC_TARGET_ARM64
NATIVE_DEFINES_$(CONFIG_riscv64) += -DNOOC_TARGET_RISCV64
NATIVE_DEFINES_$(CONFIG_BSD) += -DTARGETOS_$(TARGETOS)
NATIVE_DEFINES_$(CONFIG_Android) += -DTARGETOS_ANDROID
NATIVE_DEFINES_$(CONFIG_pie) += -DCONFIG_NOOC_PIE
NATIVE_DEFINES_$(CONFIG_pic) += -DCONFIG_NOOC_PIC
NATIVE_DEFINES_no_$(CONFIG_new_macho) += -DCONFIG_NEW_MACHO=0
NATIVE_DEFINES_$(CONFIG_codesign) += -DCONFIG_CODESIGN
NATIVE_DEFINES_$(CONFIG_new-dtags) += -DCONFIG_NEW_DTAGS
NATIVE_DEFINES_no_$(CONFIG_bcheck) += -DCONFIG_NOOC_BCHECK=0
NATIVE_DEFINES_no_$(CONFIG_backtrace) += -DCONFIG_NOOC_BACKTRACE=0
NATIVE_DEFINES += $(NATIVE_DEFINES_yes) $(NATIVE_DEFINES_no_no)

DEF-i386           = -DNOOC_TARGET_I386
DEF-i386-win32     = -DNOOC_TARGET_I386 -DNOOC_TARGET_PE
DEF-i386-OpenBSD   = $(DEF-i386) -DTARGETOS_OpenBSD
DEF-x86_64         = -DNOOC_TARGET_X86_64
DEF-x86_64-win32   = -DNOOC_TARGET_X86_64 -DNOOC_TARGET_PE
DEF-x86_64-osx     = -DNOOC_TARGET_X86_64 -DNOOC_TARGET_MACHO
DEF-arm-fpa        = -DNOOC_TARGET_ARM
DEF-arm-fpa-ld     = -DNOOC_TARGET_ARM -DLDOUBLE_SIZE=12
DEF-arm-vfp        = -DNOOC_TARGET_ARM -DNOOC_ARM_VFP
DEF-arm-eabi       = -DNOOC_TARGET_ARM -DNOOC_ARM_VFP -DNOOC_ARM_EABI
DEF-arm-eabihf     = $(DEF-arm-eabi) -DNOOC_ARM_HARDFLOAT
DEF-arm            = $(DEF-arm-eabihf)
DEF-arm-NetBSD     = $(DEF-arm-eabihf) -DTARGETOS_NetBSD
DEF-arm-wince      = $(DEF-arm-eabihf) -DNOOC_TARGET_PE
DEF-arm64          = -DNOOC_TARGET_ARM64
DEF-arm64-osx      = $(DEF-arm64) -DNOOC_TARGET_MACHO
DEF-arm64-FreeBSD  = $(DEF-arm64) -DTARGETOS_FreeBSD
DEF-arm64-NetBSD   = $(DEF-arm64) -DTARGETOS_NetBSD
DEF-arm64-OpenBSD  = $(DEF-arm64) -DTARGETOS_OpenBSD
DEF-riscv64        = -DNOOC_TARGET_RISCV64
DEF-c67            = -DNOOC_TARGET_C67 -w # disable warnigs
DEF-x86_64-FreeBSD = $(DEF-x86_64) -DTARGETOS_FreeBSD
DEF-x86_64-NetBSD  = $(DEF-x86_64) -DTARGETOS_NetBSD
DEF-x86_64-OpenBSD = $(DEF-x86_64) -DTARGETOS_OpenBSD

DEF-$(NATIVE_TARGET) = $(NATIVE_DEFINES)

ifeq ($(INCLUDED),no)
# --------------------------------------------------------------------------
# running top Makefile

PROGS = nooc$(EXESUF)
NOOCLIBS = $(LIBNOOCDEF) $(LIBNOOC) $(LIBNOOC1)
NOOCDOCS = nooc.1 nooc-doc.html nooc-doc.info

all: $(PROGS) $(NOOCLIBS) $(NOOCDOCS)

# cross compiler targets to build
NOOC_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm-wince c67
NOOC_X += riscv64 arm64-osx
# NOOC_X += arm-fpa arm-fpa-ld arm-vfp arm-eabi

# cross libnooc1.a targets to build
LIBNOOC1_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm-wince
LIBNOOC1_X += riscv64 arm64-osx

PROGS_CROSS = $(foreach X,$(NOOC_X),$X-nooc$(EXESUF))
LIBNOOC1_CROSS = $(foreach X,$(LIBNOOC1_X),$X-libnooc1.a)

# build cross compilers & libs
cross: $(LIBNOOC1_CROSS) $(PROGS_CROSS)

# build specific cross compiler & lib
cross-%: %-nooc$(EXESUF) %-libnooc1.a ;

install: ; @$(MAKE) --no-print-directory  install$(CFG)
install-strip: ; @$(MAKE) --no-print-directory  install$(CFG) CONFIG_strip=yes
uninstall: ; @$(MAKE) --no-print-directory uninstall$(CFG)

ifdef CONFIG_cross
all : cross
endif

# --------------------------------------------

T = $(or $(CROSS_TARGET),$(NATIVE_TARGET),unknown)
X = $(if $(CROSS_TARGET),$(CROSS_TARGET)-)

DEFINES += $(DEF-$T) $(DEF-all)
DEFINES += $(if $(ROOT-$T),-DCONFIG_SYSROOT="\"$(ROOT-$T)\"")
DEFINES += $(if $(CRT-$T),-DCONFIG_NOOC_CRTPREFIX="\"$(CRT-$T)\"")
DEFINES += $(if $(LIB-$T),-DCONFIG_NOOC_LIBPATHS="\"$(LIB-$T)\"")
DEFINES += $(if $(INC-$T),-DCONFIG_NOOC_SYSINCLUDEPATHS="\"$(INC-$T)\"")
DEFINES += $(if $(ELF-$T),-DCONFIG_NOOC_ELFINTERP="\"$(ELF-$T)\"")
DEFINES += $(DEF-$(or $(findstring win,$T),unx))

ifneq ($(X),)
DEF-all += -DCONFIG_NOOC_CROSSPREFIX="\"$X\""
ifneq ($(CONFIG_WIN32),yes)
DEF-win += -DCONFIG_NOOCDIR="\"$(noocdir)/win32\""
endif
endif

# include custom configuration (see make help)
-include config-extra.mak

ifneq ($(X),)
ifneq ($(T),$(NATIVE_TARGET))
# assume support files for cross-targets in "/usr/<triplet>" by default
TRIPLET-i386 ?= i686-linux-gnu
TRIPLET-x86_64 ?= x86_64-linux-gnu
TRIPLET-arm ?= arm-linux-gnueabi
TRIPLET-arm64 ?= aarch64-linux-gnu
TRIPLET-riscv64 ?= riscv64-linux-gnu
TR = $(if $(TRIPLET-$T),$T,ignored)
CRT-$(TR) ?= /usr/$(TRIPLET-$T)/lib
LIB-$(TR) ?= {B}:/usr/$(TRIPLET-$T)/lib
INC-$(TR) ?= {B}/include:/usr/$(TRIPLET-$T)/include
endif
endif

CORE_FILES = nooc.c nooctools.c libnooc.c noocpp.c noocgen.c noocdbg.c noocelf.c noocasm.c noocrun.c
CORE_FILES += nooc.h config.h libnooc.h nooctok.h
i386_FILES = $(CORE_FILES) i386-gen.c i386-link.c i386-asm.c i386-asm.h i386-tok.h
i386-win32_FILES = $(i386_FILES) noocpe.c
x86_64_FILES = $(CORE_FILES) x86_64-gen.c x86_64-link.c i386-asm.c x86_64-asm.h
x86_64-win32_FILES = $(x86_64_FILES) noocpe.c
x86_64-osx_FILES = $(x86_64_FILES) noocmacho.c
arm_FILES = $(CORE_FILES) arm-gen.c arm-link.c arm-asm.c arm-tok.h
arm-wince_FILES = $(arm_FILES) noocpe.c
arm-eabihf_FILES = $(arm_FILES)
arm-fpa_FILES     = $(arm_FILES)
arm-fpa-ld_FILES  = $(arm_FILES)
arm-vfp_FILES     = $(arm_FILES)
arm-eabi_FILES    = $(arm_FILES)
arm-eabihf_FILES  = $(arm_FILES)
arm64_FILES = $(CORE_FILES) arm64-gen.c arm64-link.c arm64-asm.c
arm64-osx_FILES = $(arm64_FILES) noocmacho.c
c67_FILES = $(CORE_FILES) c67-gen.c c67-link.c nooccoff.c
riscv64_FILES = $(CORE_FILES) riscv64-gen.c riscv64-link.c riscv64-asm.c

NOOCDEFS_H$(subst yes,,$(CONFIG_predefs)) = noocdefs_.h

# libnooc sources
LIBNOOC_SRC = $(filter-out nooc.c nooctools.c,$(filter %.c,$($T_FILES)))

ifeq ($(ONE_SOURCE),yes)
LIBNOOC_OBJ = $(X)libnooc.o
LIBNOOC_INC = $($T_FILES)
NOOC_FILES = $(X)nooc.o
nooc.o : DEFINES += -DONE_SOURCE=0
$(X)nooc.o $(X)libnooc.o  : $(NOOCDEFS_H)
else
LIBNOOC_OBJ = $(patsubst %.c,$(X)%.o,$(LIBNOOC_SRC))
LIBNOOC_INC = $(filter %.h %-gen.c %-link.c,$($T_FILES))
NOOC_FILES = $(X)nooc.o $(LIBNOOC_OBJ)
$(NOOC_FILES) : DEFINES += -DONE_SOURCE=0
$(X)noocpp.o : $(NOOCDEFS_H)
endif

GITHASH:=$(shell git rev-parse --abbrev-ref HEAD 2>/dev/null || echo no)
ifneq ($(GITHASH),no)
GITHASH:=$(shell git log -1 --date=short --pretty='format:%cd $(GITHASH)@%h')
GITMODF:=$(shell git diff --quiet || echo '*')
DEF_GITHASH:= -DNOOC_GITHASH="\"$(GITHASH)$(GITMODF)\""
endif

ifeq ($(CONFIG_debug),yes)
CFLAGS += -g
LDFLAGS += -g
endif

# convert "include/noocdefs.h" to "noocdefs_.h"
%_.h : include/%.h conftest.c
	$S$(CC) -DC2STR $(filter %.c,$^) -o c2str.exe && ./c2str.exe $< $@

# target specific object rule
$(X)%.o : %.c $(LIBNOOC_INC)
	$S$(CC) -o $@ -c $< $(DEFINES) $(CFLAGS)

# additional dependencies
$(X)nooc.o : nooctools.c
$(X)nooc.o : DEFINES += $(DEF_GITHASH)

# Host New Object Oriented C
nooc$(EXESUF): nooc.o $(LIBNOOC)
	$S$(CC) -o $@ $^ $(LIBS) $(LDFLAGS) $(LINK_LIBNOOC)

# Cross New Object Oriented Cs
# (the NOOCDEFS_H dependency is only necessary for parallel makes,
# ala 'make -j x86_64-nooc i386-nooc nooc', which would create multiple
# c2str.exe and noocdefs_.h files in parallel, leading to access errors.
# This forces it to be made only once.  Make normally tracks multiple paths
# to the same goals and only remakes it once, but that doesn't work over
# sub-makes like in this target)
%-nooc$(EXESUF): $(NOOCDEFS_H) FORCE
	@$(MAKE) --no-print-directory $@ CROSS_TARGET=$* ONE_SOURCE=$(or $(ONE_SOURCE),yes)

$(CROSS_TARGET)-nooc$(EXESUF): $(NOOC_FILES)
	$S$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

# profiling version
nooc_p$(EXESUF): $($T_FILES)
	$S$(CC) -o $@ $< $(DEFINES) $(CFLAGS_P) $(LIBS_P) $(LDFLAGS_P)

# static libnooc library
libnooc.a: $(LIBNOOC_OBJ)
	$S$(AR) rcs $@ $^

# dynamic libnooc library
libnooc.so: $(LIBNOOC_OBJ)
	$S$(CC) -shared -Wl,-soname,$@ -o $@ $^ $(LIBS) $(LDFLAGS)

libnooc.so: CFLAGS+=-fPIC
libnooc.so: LDFLAGS+=-fPIC

# OSX dynamic libnooc library
libnooc.dylib: $(LIBNOOC_OBJ)
	$S$(CC) -dynamiclib $(DYLIBVER) -install_name @rpath/$@ -o $@ $^ $(LDFLAGS) 

# OSX libnooc.dylib (without rpath/ prefix)
libnooc.osx: $(LIBNOOC_OBJ)
	$S$(CC) -shared -install_name libnooc.dylib -o libnooc.dylib $^ $(LDFLAGS) 

# windows dynamic libnooc library
libnooc.dll : $(LIBNOOC_OBJ)
	$S$(CC) -shared -o $@ $^ $(LDFLAGS)
libnooc.dll : DEFINES += -DLIBNOOC_AS_DLL

# import file for windows libnooc.dll
libnooc.def : libnooc.dll nooc$(EXESUF)
	$S$(XNOOC) -impdef $< -o $@
XNOOC ?= ./nooc$(EXESUF)

# TinyCC runtime libraries
libnooc1.a : nooc$(EXESUF) FORCE
	@$(MAKE) -C lib

# Cross libnooc1.a
%-libnooc1.a : %-nooc$(EXESUF) FORCE
	@$(MAKE) -C lib CROSS_TARGET=$*

.PRECIOUS: %-libnooc1.a
FORCE:

# WHICH = which $1 2>/dev/null
# some versions of gnu-make do not recognize 'command' as a shell builtin
WHICH = sh -c 'command -v $1'

run-if = $(if $(shell $(call WHICH,$1)),$S $1 $2)
S = $(if $(findstring yes,$(SILENT)),@$(info * $@))

# --------------------------------------------------------------------------
# documentation and man page
nooc-doc.html: nooc-doc.texi
	$(call run-if,makeinfo,--no-split --html --number-sections -o $@ $<)

nooc-doc.info: nooc-doc.texi
	$(call run-if,makeinfo,$< || true)

nooc.1 : nooc-doc.pod
	$(call run-if,pod2man,--section=1 --center="New Object Oriented C" \
		--release="$(VERSION)" $< >$@)
%.pod : %.texi
	$(call run-if,perl,$(TOPSRC)/texi2pod.pl $< $@)

doc : $(NOOCDOCS)

# --------------------------------------------------------------------------
# install

INSTALL = install -m644
INSTALLBIN = install -m755 $(STRIP_$(CONFIG_strip))
STRIP_yes = -s

LIBNOOC1_W = $(filter %-win32-libnooc1.a %-wince-libnooc1.a,$(LIBNOOC1_CROSS))
LIBNOOC1_U = $(filter-out $(LIBNOOC1_W),$(wildcard *-libnooc1.a))
IB = $(if $1,$(IM) mkdir -p $2 && $(INSTALLBIN) $1 $2)
IBw = $(call IB,$(wildcard $1),$2)
IF = $(if $1,$(IM) mkdir -p $2 && $(INSTALL) $1 $2)
IFw = $(call IF,$(wildcard $1),$2)
IR = $(IM) mkdir -p $2 && cp -r $1/. $2
IM = @echo "-> $2 : $1" ;
BINCHECK = $(if $(wildcard $(PROGS) *-nooc$(EXESUF)),,@echo "Makefile: nothing found to install" && exit 1)

B_O = bcheck.o bt-exe.o bt-log.o bt-dll.o

# install progs & libs
install-unx:
	$(call BINCHECK)
	$(call IBw,$(PROGS) *-nooc,"$(bindir)")
	$(call IFw,$(LIBNOOC1) $(B_O) $(LIBNOOC1_U),"$(noocdir)")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/nooclib.h,"$(noocdir)/include")
	$(call $(if $(findstring .so,$(LIBNOOC)),IBw,IFw),$(LIBNOOC),"$(libdir)")
	$(call IF,$(TOPSRC)/libnooc.h,"$(includedir)")
	$(call IFw,nooc.1,"$(mandir)/man1")
	$(call IFw,nooc-doc.info,"$(infodir)")
	$(call IFw,nooc-doc.html,"$(docdir)")
ifneq "$(wildcard $(LIBNOOC1_W))" ""
	$(call IFw,$(TOPSRC)/win32/lib/*.def $(LIBNOOC1_W),"$(noocdir)/win32/lib")
	$(call IR,$(TOPSRC)/win32/include,"$(noocdir)/win32/include")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/nooclib.h,"$(noocdir)/win32/include")
endif

# uninstall
uninstall-unx:
	@rm -fv $(addprefix "$(bindir)/",$(PROGS) $(PROGS_CROSS))
	@rm -fv $(addprefix "$(libdir)/", libnooc*.a libnooc*.so libnooc.dylib,$P)
	@rm -fv $(addprefix "$(includedir)/", libnooc.h)
	@rm -fv "$(mandir)/man1/nooc.1" "$(infodir)/nooc-doc.info"
	@rm -fv "$(docdir)/nooc-doc.html"
	@rm -frv "$(noocdir)"

# install progs & libs on windows
install-win:
	$(call BINCHECK)
	$(call IBw,$(PROGS) *-nooc.exe libnooc.dll,"$(bindir)")
	$(call IF,$(TOPSRC)/win32/lib/*.def,"$(noocdir)/lib")
	$(call IFw,libnooc1.a $(B_O) $(LIBNOOC1_W),"$(noocdir)/lib")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/nooclib.h,"$(noocdir)/include")
	$(call IR,$(TOPSRC)/win32/include,"$(noocdir)/include")
	$(call IR,$(TOPSRC)/win32/examples,"$(noocdir)/examples")
	$(call IF,$(TOPSRC)/tests/libnooc_test.c,"$(noocdir)/examples")
	$(call IFw,$(TOPSRC)/libnooc.h libnooc.def,"$(libdir)")
	$(call IFw,$(TOPSRC)/win32/nooc-win32.txt nooc-doc.html,"$(docdir)")
ifneq "$(wildcard $(LIBNOOC1_U))" ""
	$(call IFw,$(LIBNOOC1_U),"$(noocdir)/lib")
	$(call IF,$(TOPSRC)/include/*.h $(TOPSRC)/nooclib.h,"$(noocdir)/lib/include")
endif

# uninstall on windows
uninstall-win:
	@rm -fv $(addprefix "$(bindir)/", libnooc*.dll $(PROGS) *-nooc.exe)
	@rm -fr $(foreach P,doc examples include lib libnooc,"$(noocdir)/$P/*")
	@rm -frv $(addprefix "$(noocdir)/", doc examples include lib libnooc)

# the msys-git shell works to configure && make except it does not have install
ifeq ($(OS),Windows_NT)
ifeq ($(shell $(call WHICH,install) || echo no),no)
INSTALL = cp
INSTALLBIN = cp
endif
endif

# --------------------------------------------------------------------------
# other stuff

TAGFILES = *.[ch] include/*.h lib/*.[chS]
tags : ; ctags $(TAGFILES)
# cannot have both tags and TAGS on windows
ETAGS : ; etags $(TAGFILES)

# create release tarball from *current* git branch (including nooc-doc.html
# and converting two files to CRLF)
NOOC-VERSION = nooc-$(VERSION)
NOOC-VERSION = tinycc-mob-$(shell git rev-parse --short=7 HEAD)
tar:    nooc-doc.html
	mkdir -p $(NOOC-VERSION)
	( cd $(NOOC-VERSION) && git --git-dir ../.git checkout -f )
	cp nooc-doc.html $(NOOC-VERSION)
	for f in nooc-win32.txt build-nooc.bat ; do \
	    cat win32/$$f | sed 's,\(.*\),\1\r,g' > $(NOOC-VERSION)/win32/$$f ; \
	done
	tar cjf $(NOOC-VERSION).tar.bz2 $(NOOC-VERSION)
	rm -rf $(NOOC-VERSION)
	git reset

config.mak:
	$(if $(wildcard $@),,@echo "Please run ./configure." && exit 1)

# run all tests
test:
	@$(MAKE) -C tests
# run test(s) from tests2 subdir (see make help)
tests2.%:
	@$(MAKE) -C tests/tests2 $@
# run test(s) from testspp subdir (see make help)
testspp.%:
	@$(MAKE) -C tests/pp $@
# run tests with code coverage
tcov-tes% : nooc_c$(EXESUF)
	@rm -f $<.tcov
	@$(MAKE) --no-print-directory NOOC_LOCAL=$(CURDIR)/$< tes$*
nooc_c$(EXESUF): $($T_FILES)
	$S$(NOOC) nooc.c -o $@ -ftest-coverage $(DEFINES)

clean:
	@rm -f nooc$(EXESUF) nooc_c$(EXESUF) nooc_p$(EXESUF) *-nooc$(EXESUF)
	@rm -f tags ETAGS *.o *.a *.so* *.out *.log lib*.def *.exe *.dll
	@rm -f a.out *.dylib *_.h *.pod *.tcov
	@$(MAKE) -s -C lib $@
	@$(MAKE) -s -C tests $@

distclean: clean
	@rm -vf config.h config.mak config.texi
	@rm -vf $(NOOCDOCS)

.PHONY: all clean test tar tags ETAGS doc distclean install uninstall FORCE

help:
	@echo "make"
	@echo "   build native compiler (from separate objects)"
	@echo "make cross"
	@echo "   build cross compilers (from one source)"
	@echo "make ONE_SOURCE=no/yes SILENT=no/yes"
	@echo "   force building from separate/one object(s), less/more silently"
	@echo "make cross-TARGET"
	@echo "   build one specific cross compiler for 'TARGET'. Currently supported:"
	@echo "   $(wordlist 1,8,$(NOOC_X))"
	@echo "   $(wordlist 9,99,$(NOOC_X))"
	@echo "make test"
	@echo "   run all tests"
	@echo "make tests2.all / make tests2.37 / make tests2.37+"
	@echo "   run all/single test(s) from tests2, optionally update .expect"
	@echo "make testspp.all / make testspp.17"
	@echo "   run all/single test(s) from tests/pp"
	@echo "make tcov-test / tcov-tests2... / tcov-testspp..."
	@echo "   run tests as above with code coverage. After test(s) see nooc_c$(EXESUF).tcov"
	@echo "Other supported make targets:"
	@echo "   install install-strip doc clean tags ETAGS tar distclean help"
	@echo "Custom configuration:"
	@echo "   The makefile includes a file 'config-extra.mak' if it is present."
	@echo "   This file may contain some custom configuration.  For example:"
	@echo "      NATIVE_DEFINES += -D..."
	@echo "   Or for example to configure the search paths for a cross-compiler"
	@echo "   assuming the support files in /usr/i686-linux-gnu:"
	@echo "      ROOT-i386 = /usr/i686-linux-gnu"
	@echo "      CRT-i386  = {R}/lib"
	@echo "      LIB-i386  = {B}:{R}/lib"
	@echo "      INC-i386  = {B}/include:{R}/include (*)"
	@echo "      DEF-i386  += -D__linux__"
	@echo "   Or also, for the cross platform files in /usr/<triplet>"
	@echo "      TRIPLET-i386 = i686-linux-gnu"
	@echo "   (*) nooc replaces {B} by 'noocdir' and {R} by 'CONFIG_SYSROOT'"

# --------------------------------------------------------------------------
endif # ($(INCLUDED),no)
