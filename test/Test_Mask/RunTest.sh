#!/bin/bash

. ../MasterTest.sh

# Clean
CleanFiles mask.in mask.out mask.pdb.1 mask.mol2.1 M.dat \
  Last.dat Last.pdb.1 First.dat First.pdb.1

TESTNAME='Mask command tests'
Requires netcdf

INPUT="-i mask.in"
# Test 1
UNITNAME='Mask single frame test'
CheckFor maxthreads 1
if [ $? -eq 0 ] ; then
  cat > mask.in <<EOF
noprogress
parm ../tz2.ortho.parm7
trajin ../tz2.ortho.nc 1 1
mask "(:5 <:3.0) & :WAT" maskout mask.out maskpdb mask.pdb
mask "(:5 <:3.0) & :WAT" maskmol2 mask.mol2
EOF
  RunCpptraj "$UNITNAME"
  DoTest mask.out.save mask.out
  DoTest mask.pdb.1.save mask.pdb.1
  DoTest mask.mol2.1.save mask.mol2.1
fi

cat > mask.in <<EOF
noprogress
parm ../tz2.ortho.parm7
trajin ../tz2.ortho.nc
mask "(:8@NZ <:3.0) & :WAT@O" name M out M.dat noxcol
EOF
RunCpptraj "Mask longer command test."
DoTest M.dat.save M.dat

Test0() {
cat > mask.in <<EOF
parm ../tz2.parm7
reference ../tz2.nc 1 [FIRST]
reference ../tz2.nc lastframe [LAST]
activeref ref [LAST]
loadcrd ../tz2.nc 1 1
crdaction tz2.nc mask :2>@5.0 maskout Last.dat maskpdb Last.pdb
activeref ref [FIRST]
crdaction tz2.nc mask :2>@5.0 maskout First.dat maskpdb First.pdb
EOF
RunCpptraj "Mask with active reference test"
}

cat > mask.in <<EOF
parm ../tz2.parm7
reference ../tz2.nc 1 [FIRST]
trajin ../tz2.nc 1 1
mask :2>@5.0 maskout First.dat maskpdb First.pdb
EOF
RunCpptraj "Test"

EndTest

exit 0
