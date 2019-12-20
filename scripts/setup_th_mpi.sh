# #############################################################
#  setup MPI on TIANHE-II
#
#  Before run this script, you should:
#  1. Copy mpi.tgz, intel.tar.gz and licenses.tgz to /usr/local of the target node 
#  2. Copy /opt/intel to /opt of the target node
###############################################################

cd /usr/local
tar -zxvf mpi.tgz
tar -zxvf intel.tar.gz
tar -zxvf licenses.tgz

export PATH=$PATH:/usr/local/mpi3-dynamic/bin/

export LD_LIBRARY_PATH=/opt/intel/composer_xe_2013_sp1.2.144/compiler/lib/intel64:/opt/intel/mic/coi/host-linux-release/lib:/opt/intel/mic/myo/lib:/opt/intel/composer_xe_2013_sp1.2.144/mpirt/lib/intel64:/opt/intel/composer_xe_2013_sp1.2.144/ipp/../compiler/lib/intel64:/opt/intel/composer_xe_2013_sp1.2.144/ipp/lib/intel64:/opt/intel/mic/coi/host-linux-release/lib:/opt/intel/mic/myo/lib:/opt/intel/composer_xe_2013_sp1.2.144/compiler/lib/intel64:/opt/intel/composer_xe_2013_sp1.2.144/mkl/lib/intel64:/opt/intel/composer_xe_2013_sp1.2.144/tbb/lib/intel64/gcc4.4

export INTEL_LICENSE_FILE=$INTEL_LICENSE_FILE:/usr/local/licenses

PATH=/usr/local/mpi3-dynamic/bin:/opt/intel/composer_xe_2013_sp1.2.144/bin/intel64:/opt/intel/composer_xe_2013_sp1.2.144/mpirt/bin/intel64:/opt/intel/composer_xe_2013_sp1.2.144/debugger/gdb/intel64_mic/py27/bin:/opt/intel/composer_xe_2013_sp1.2.144/debugger/gdb/intel64/py27/bin:/opt/intel/composer_xe_2013_sp1.2.144/bin/intel64:/opt/intel/composer_xe_2013_sp1.2.144/bin/intel64_mic:/opt/intel/composer_xe_2013_sp1.2.144/debugger/gui/intel64:/usr/lib64/qt-3.3/bin:/usr/kerberos/sbin:/usr/kerberos/bin:/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:/root/bin:/root/bin

source /etc/profile