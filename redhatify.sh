#! /bin/bash

function die()
{
    echo $*
    exit 1
}

function patchopt()
{
    grep 'metacity-' $1 >/dev/null 2>&1 && echo "-p1" || echo "-p0"
}

RHDIR=~/rh-cvs/rpms/metacity
JHDIR=~/jhbuild-checkouts/metacity-2-2

PATCHLINES=`egrep '^Patch[0-9]+:' $RHDIR/metacity.spec`

PATCHES=""
for L in $PATCHLINES ; do
    if test -e $RHDIR/$L ; then
        ## button-down is already applied to cvs
        if ! echo $L | grep 'button-down' ; then
            PATCHES="$PATCHES $L"
        else
            echo "skipping $L"
        fi
    fi
done

echo "Patches are: $PATCHES"

cd $JHDIR

if test x"$1" = x"undo" ; then
    echo "Reverting patches"
    REVPATCHES=
    for P in $PATCHES ; do
        REVPATCHES="$P $REVPATCHES"
    done
    for P in $REVPATCHES ; do
        patch -R -N `patchopt $RHDIR/$P` < $RHDIR/$P || true
    done
else
    for P in $PATCHES ; do
        patch `patchopt $RHDIR/$P` < $RHDIR/$P || die "Failed to apply $P"
    done
fi

