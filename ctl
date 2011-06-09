#!/bin/sh

VERSION="v3"

BIN="bin"
PORT=12345

YACC_FLAGS="-DYYERROR_VERBOSE" # for bison
WARN="-W -Wall -Wextra -Wredundant-decls -Wno-unused"
LINK=""
CC="gcc -g -std=c99 $WARN $YACC_FLAGS"
YACC="yacc -d"
LEX="flex -I"

LIBS="array% expression% head% http% index% memory% pack% relation%"
LIBS="$LIBS string% summary% tuple% transaction% value% version% volume%"
LIBS="$LIBS test/common% lex.yy% y.tab%"
STRUCT_TESTS="test/array% test/expression% test/head% test/http% test/index%"
STRUCT_TESTS="$STRUCT_TESTS test/language% test/memory% test/multiproc%"
STRUCT_TESTS="$STRUCT_TESTS test/network% test/number% test/pack%"
STRUCT_TESTS="$STRUCT_TESTS test/relation% test/string% test/summary%"
STRUCT_TESTS="$STRUCT_TESTS test/system% test/tuple% test/transaction%"
STRUCT_TESTS="$STRUCT_TESTS test/value%"
PERF_TESTS="test/perf/expression% test/perf/index% test/perf/multiproc%"
PERF_TESTS="$PERF_TESTS test/perf/number% test/perf/relation%"
PERF_TESTS="$PERF_TESTS test/perf/system% test/perf/tuple%"
PROGS="bandicoot%"

if [ ! "`uname | grep -i CYGWIN`" = "" ]
then
    LIBS="$LIBS system_win32%"
    LINK="-lws2_32"
else
    LIBS="$LIBS system_posix%"
    if [ `uname` = "SunOS" ]
    then
        LINK="-lnsl -lsocket"
    elif [ `uname` = "Linux" ]
    then
        CC="$CC -pthread"
    fi
fi

ALL_VARS=`ls test/data`

create_out_dirs()
{
    mkdir -p "$BIN/test/perf"
    mkdir -p "$BIN/volume"
    mkdir -p "$BIN/test/lsdir/one_dir"
}

check_lp64()
{
    lp64=`mktemp $BIN/lp64-XXXX`
    echo 'int main() { return sizeof(long); }' \
        | $CC $DIST_CFLAGS -o $lp64 -x c -

    ./$lp64
    if [ "8" = "$?" ]
    then
        echo '[=] LP64'
        CC="$CC -DLP64"
    else
        echo '[=] ILP32'
    fi
}

prepare_lang()
{
    $YACC language.y
    $LEX language.l
}

prepare_version()
{
    cat > version.c << EOF
const char *VERSION =
    "[bandicoot $VERSION, http://bandilab.org, built on `date -u`]\n";
EOF
}

compile()
{
    create_out_dirs
    prepare_lang
    prepare_version
    check_lp64
    echo

    for i in `echo $* | sed 's/%//g'`
    do
        echo "[C] $i.o"
        $CC $DIST_CFLAGS -c -o $i.o $i.c
        if [ "$?" != "0" ]
        then
            echo "Compilation error. Abort."
            exit 1
        fi
    done
}

link()
{
    libs=`echo $LIBS | sed 's/%/.o/g'`
    for obj in `echo $* | sed 's/%/.o/g'`
    do
        e=`echo $BIN/$obj | sed 's/\.o//g'`
        echo "[L] $e"
        $CC $DIST_CFLAGS -o $e $libs $obj $LINK
        if [ "$?" != "0" ]
        then
            echo "Linking error. Abort."
            exit 1
        fi
    done
}

run()
{
    for e in `echo $* | sed 's/%//g'`
    do
        echo "[T] $e"
        $BIN/$e
        if [ "$?" != "0" ]
        then
            echo "    failed"
        fi
    done
}

clean()
{
    find . -name '*.o' -exec rm {} \;
    find . -name '*.log' -exec rm {} \;
    rm -rf version.c y.tab.[ch] lex.yy.c $BIN
    rm -rf "bandicoot-$VERSION"
}

dist()
{
    DIST_CFLAGS=$1

    clean
    compile $LIBS $PROGS
    echo
    link $PROGS
    echo

    d="bandicoot-$VERSION"
    mkdir -p $d

    cp LICENSE NOTICE README $BIN/bandicoot* $d
    if [ -d "$2" ]
    then
        echo "including examples directory $2"
        cp -r $2 $d
    fi

    a="$BIN/$d.tar.gz"
    echo "[A] $a"
    tar cfz $a $d
}

cmd=$1
case $cmd in
    clean)
        clean
        ;;
    pack)
        clean
        compile $LIBS $PROGS $STRUCT_TESTS $PERF_TESTS
        echo
        link $PROGS $STRUCT_TESTS $PERF_TESTS
        echo
        echo "[P] ..."
        $BIN/bandicoot start -c "test/test_defs.b" \
                             -d $BIN/volume \
                             -s $BIN/state \
                             -p $PORT &
        pid=$!

        # we need to finish the initialization
        # before we start storing the variables
        sleep 1

        for v in $ALL_VARS
        do
            curl -s http://127.0.0.1:$PORT/load_$v > /dev/null
            curl -s --data-binary @test/data/$v http://127.0.0.1:$PORT/store_$v
        done
        kill $pid
        ;;
    perf)
        run $PERF_TESTS 
        ;;
    stress_read)
        while [ true ]
        do
            for v in $ALL_VARS
            do
                curl -s http://127.0.0.1:$PORT/load_$v -o /dev/null
                if [ "$?" != "0" ]
                then
                    exit
                fi
            done
        done
        ;;
    stress_write)
        while [ true ]
        do
            for v in $ALL_VARS
            do
                curl -s --data-binary @test/data/$v \
                    http://127.0.0.1:$PORT/store_$v -o /dev/null
                if [ "$?" != "0" ]
                then
                    exit
                fi
            done
        done
        ;;
    test)
        run $STRUCT_TESTS
        cat test/progs/*.log | sort > bin/lang_test.out
        cat test/progs/all.out | sort > bin/all_sorted.out
        diff -u bin/all_sorted.out bin/lang_test.out
        if [ "$?" != "0" ]
        then
            echo "---"
            echo "language test failed (inspect the differences above)"
        fi
        ;;
    todos)
        find . -regex '.*\.[chly]' -exec egrep -n -H -i 'fixme|todo|think' {} \;
        ;;
    dist)
        if [ "$2" != "-m32" ] && [ "$2" != "-m64" ]
        then
            echo "---"
            echo "expecting -m32|-m64 as the first parameter"
            exit 1
        fi

        dist "-Os $2" $3
        ;;
    *)
        echo "unknown command '$cmd', usage: ctl <command>"
        echo "    dist -m32|-m64 [examles/] - build a package for distribution"
        echo "    pack - compile and prepare for running tests"
        echo "    test - execute structured tests (prereq: pack)"
        echo "    perf - execute performance tests (prereq: pack)"
        echo "    clean - remove object files etc."
esac
