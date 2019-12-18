# prepare evn and install indexfs
cd ..
cd ..
yum install -y cmake
yum install -y gcc g++ make flex bison
yum install -y gcc-c++
yum install -y autoconf automake libtool
yum install -y pkgconfig
yum install -y zlib-deve
yum install -y snappy-devel
yum install -y boost-devel
yum install -y libevent-devel
yum install -y openssl-devel
yum install -y fuse-devel fuse-libs
yum install -y mpich pdsh
wget https://github.com/gflags/gflags/archive/v2.1.2.tar.gz
tar -xvf ./v2.1.2.tar.gz
cd ./gflags-2.1.2
mkdir ./build
cd ./build
ccmake .. 
#c c g
cmake .. -DBUILD_SHARED_LIBS=ON
make & make install
cd ..
cd ..
git clone https://github.com/google/glog.git
cd ./glog
./autogen.sh && ./configure && make && make install
cd ..
yum install -y thrift-devel
git clone https://github.com/zhengqmark/indexfs_old.git
cd ./indexfs_old
export LANG=en_US.UTF-8
export LC_ALL=en_US
./bootstrap.sh

touch /tmp/giga_conf
touch /tmp/idxfs_conf

echo '/usr/local/lib' >> /etc/ld.so.conf
ldconfig

########## How to run IndexFS: ###########
#
#	1. add servers info in giga_conf (also in /etc/indexfs-distributed/server_list) :
#		10.182.171.2:10086
#		10.182.171.3:10086
#
#	2. add file path in idxfs_conf (also in /etc/indexfs-distributed/indexfs_conf) :
#		leveldb_dir=/mnt/beegfs/indexfs/leveldb
#		split_dir=/mnt/beegfs/indexfs/split
#		file_dir=/mnt/beegfs/indexfs/files
#
#	3. /PATH/TO/INDEXFS/sbin/start-all.sh
#
#	4. add /usr/local/lib to /etc/ld.so.conf  and then ldconfig
#
#	5. /PATH/TO/INDEXFS/build/md_test/
#		e.g. mpirun -machinefile ./host  -np 24 ./mdtest_nobk  -n 4166  -z 1 -L -R -D -C -T -d 
#
#	6. /PATH/TO/INDEXFS/sbin/stop-all.sh
#
#	7. rm /mnt/beegfs/indexfs/* -rf  && rm /tmp/indexfs/* -rf
#
############################################