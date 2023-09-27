#!/bin/sh

if [ -z "$TESTSUITE_PATH" ]
then
  if [ -d "$HOME/gcc/gcc-3.2/gcc/testsuite/gcc.c-torture" ]
  then
    TESTSUITE_PATH="$HOME/gcc/gcc-3.2/gcc/testsuite/gcc.c-torture"
  fi
fi

if [ -z "$TESTSUITE_PATH" ]
then
  echo "gcc testsuite not found."
  echo "define TESTSUITE_PATH to point to the gcc.c-torture directory"
  exit 1
fi

if [ -z "$NOOC_SOURCE_PATH" ]
then
  if [ -f "include/noocdefs.h" ]
  then
    NOOC_SOURCE_PATH="."
  elif [ -f "../include/noocdefs.h" ]
  then
    NOOC_SOURCE_PATH=".."
  elif [ -f "../tinycc/include/noocdefs.h" ]
  then
    NOOC_SOURCE_PATH="../tinycc"
  fi
fi

if [ -z "$RUNTIME_DIR" ]
then
   RUNTIME_DIR=$XDG_RUNTIME_DIR
fi
if [ -z "$RUNTIME_DIR" ]
then
  RUNTIME_DIR="/tmp"
fi

if [ -z "$CC" ]
then
  if [ -z "$NOOC_SOURCE_PATH" ]
  then
      echo "nooc not found."
      echo "define NOOC_SOURCE_PATH to point to the nooc source path"
      exit 1
  fi

  NOOC="./nooc -B. -I$NOOC_SOURCE_PATH/ -I$NOOC_SOURCE_PATH/include -DNO_TRAMPOLINES"
else
  NOOC="$CC -O1 -Wno-implicit-int $CFLAGS"
fi

rm -f nooc.sum nooc.fail
nb_ok="0"
nb_skipped="0"
nb_failed="0"
nb_exe_failed="0"

# skip some failed tests not implemented in nooc
# builtin: gcc "__builtins_*"
# ieee: gcc "__builtins_*" in the ieee directory
# complex: C99 "_Complex" and gcc "__complex__"
# misc: stdc features, other arch, gcc extensions (example: gnu_inline in c89)
#

old_pwd="`pwd`"
cd "$TESTSUITE_PATH"

skip_builtin="`grep "_builtin_"  compile/*.c  execute/*.c  execute/ieee/*.c | cut -d ':' -f1 | cut -d '/' -f2 |  sort -u `"
skip_ieee="`grep "_builtin_"   execute/ieee/*.c | cut -d ':' -f1 | cut -d '/' -f3 |  sort -u `"
skip_complex="`grep -i "_Complex"  compile/*.c  execute/*.c  execute/ieee/*.c | cut -d ':' -f1 | cut -d '/' -f2 |  sort -u `"
skip_misc="20000120-2.c mipscop-1.c mipscop-2.c mipscop-3.c mipscop-4.c
   fp-cmp-4f.c fp-cmp-4l.c fp-cmp-8f.c fp-cmp-8l.c pr38016.c "

cd "$old_pwd"

for src in $TESTSUITE_PATH/compile/*.c ; do
  echo $NOOC -o $RUNTIME_DIR/tst.o -c $src
  $NOOC -o $RUNTIME_DIR/tst.o -c $src >> nooc.fail 2>&1
  if [ "$?" = "0" ] ; then
    result="PASS"
    nb_ok=$(( $nb_ok + 1 ))
  else
    base=`basename "$src"`
    skip_me="`echo  $skip_builtin $skip_ieee $skip_complex $skip_misc | grep -w $base`"

    if [ -n "$skip_me" ]
    then
      result="SKIP"
      nb_skipped=$(( $nb_skipped + 1 ))
    else
      result="FAIL"
      nb_failed=$(( $nb_failed + 1 ))
    fi
  fi
  echo "$result: $src"  >> nooc.sum
done

if [ -f "$RUNTIME_DIR/tst.o" ]
then
    rm -f "$RUNTIME_DIR/tst.o"
fi

for src in $TESTSUITE_PATH/execute/*.c  $TESTSUITE_PATH/execute/ieee/*.c ; do
  echo $NOOC $src -o $RUNTIME_DIR/tst -lm
  $NOOC $src -o $RUNTIME_DIR/tst -lm >> nooc.fail 2>&1
  if [ "$?" = "0" ] ; then
    result="PASS"
    if $RUNTIME_DIR/tst >> nooc.fail 2>&1
    then
      result="PASS"
      nb_ok=$(( $nb_ok + 1 ))
    else
      result="FAILEXE"
      nb_exe_failed=$(( $nb_exe_failed + 1 ))
    fi
  else
    base=`basename "$src"`
    skip_me="`echo $skip_builtin $skip_ieee $skip_complex $skip_misc | grep -w $base`"

    if [ -n "$skip_me" ]
    then
      result="SKIP"
      nb_skipped=$(( $nb_skipped + 1 ))
    else
      result="FAIL"
      nb_failed=$(( $nb_failed + 1 ))
    fi
  fi
  echo "$result: $src"  >> nooc.sum
done

if [ -f "$RUNTIME_DIR/tst.o" ]
then
    rm -f "$RUNTIME_DIR/tst.o"
fi
if [ -f "$RUNTIME_DIR/tst" ]
then
    rm -f "$RUNTIME_DIR/tst"
fi

echo "$nb_ok test(s) ok." >> nooc.sum
echo "$nb_ok test(s) ok."
echo "$nb_skipped test(s) skipped." >> nooc.sum
echo "$nb_skipped test(s) skipped."
echo "$nb_failed test(s) failed." >> nooc.sum
echo "$nb_failed test(s) failed."
echo "$nb_exe_failed test(s) exe failed." >> nooc.sum
echo "$nb_exe_failed test(s) exe failed."
