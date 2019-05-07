# prepare evn and install indexfs

yum install -y gcc g++ make flex bison
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

