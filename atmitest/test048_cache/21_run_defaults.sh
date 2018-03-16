#!/bin/bash
## 
## @(#) See README. Test some defaults
##
## @file 21_run_defaults.sh
## 
## -----------------------------------------------------------------------------
## Enduro/X Middleware Platform for Distributed Transaction Processing
## Copyright (C) 2015, Mavimax, Ltd. All Rights Reserved.
## This software is released under one of the following licenses:
## GPL or Mavimax's license for commercial use.
## -----------------------------------------------------------------------------
## GPL license:
## 
## This program is free software; you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free Software
## Foundation; either version 2 of the License, or (at your option) any later
## version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
## PARTICULAR PURPOSE. See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along with
## this program; if not, write to the Free Software Foundation, Inc., 59 Temple
## Place, Suite 330, Boston, MA 02111-1310 USA
##
## -----------------------------------------------------------------------------
## A commercial use license is available from Mavimax, Ltd
## contact@mavimax.com
## -----------------------------------------------------------------------------
##

export TESTNAME="test048_cache"

PWD=`pwd`
if [ `echo $PWD | grep $TESTNAME ` ]; then
    # Do nothing 
    echo > /dev/null
else
    # started from parent folder
    pushd .
    echo "Doing cd"
    cd $TESTNAME
fi;

export NDRX_CCONFIG=`pwd`
. ../testenv.sh

export TESTDIR="$NDRX_APPHOME/atmitest/$TESTNAME"
export PATH=$PATH:$TESTDIR
export NDRX_TOUT=10
export NDRX_ULOG=$TESTDIR

source ./test-func-include.sh

export TESTDIR_DB=$TESTDIR
export TESTDIR_SHM=$TESTDIR
#
# Domain 1 - here client will live
#
set_dom1() {
    echo "Setting domain 1"
    . ../dom1.sh
    export NDRX_CONFIG=$TESTDIR/ndrxconfig-dom1.xml
    export NDRX_DMNLOG=$TESTDIR/ndrxd-dom1.log
    export NDRX_LOG=$TESTDIR/ndrx-dom1.log
    export NDRX_CCTAG=dom1
}

#
# Generic exit function
#
function go_out {
    echo "Test exiting with: $1"
    
    set_dom1;
    xadmin stop -y
    xadmin down -y



    # If some alive stuff left...
    xadmin killall atmiclt48

    popd 2>/dev/null
    exit $1
}

rm *.log
# Any bridges that are live must be killed!
xadmin killall tpbridge

set_dom1;
xadmin down -y
xadmin start -y || go_out 1

RET=0

set_dom1;
xadmin psc
xadmin ppm
xadmin pc


echo "Running off client"

echo "Testing OK case, cache"
(time ./testtool48 -sTESTSV21OK -b '{"T_STRING_FLD":"KEY1","T_STRING_2_FLD":"OK"}' \
    -m '{"T_STRING_FLD":"KEY1","T_STRING_2_FLD":"OK"}' \
    -cY -n100 -fY 2>&1) > ./21_testtool48.log

if [ $? -ne 0 ]; then
    echo "testtool48 failed (1)"
    go_out 1
fi

echo "Testing fail case - no cache"
(time ./testtool48 -sTESTSV21FAIL -b '{"T_STRING_FLD":"KEY2","T_STRING_2_FLD":"FAIL"}' \
    -m '{"T_STRING_FLD":"KEY2","T_STRING_2_FLD":"FAIL"}' \
    -cN -n10 -fN -e11 2>&1) >> ./21_testtool48.log

if [ $? -ne 0 ]; then
    echo "testtool48 failed (2)"
    go_out 1
fi

xadmin cs db21

echo "There must be 1 keys"
ensure_keys db21 1

ensure_field db21 SV21OKKEY1 T_STRING_2_FLD OK 1
ensure_field db21 SV21FAILKEY2 T_STRING_2_FLD FAIL 0

go_out $RET
