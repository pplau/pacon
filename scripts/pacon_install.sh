#
# This script prepares the env and intalls pacon
#

cd ..
cd ./lib

yum install -y libevent-devel 

# inatll memc3
git clone https://github.com/efficient/memc3.git
cd ./memc3
autoreconf -fis
./configure
make
cd ..

# isntall zeromq
yum install -y uuid*
yum install -y libuuid-devel
yum install -y e2fsprogs*
wget https://github.com/zeromq/libzmq/archive/v4.3.1.tar.gz
tar -xvf v4.3.1.tar.gz
rm -f ./v4.3.1.tar.gz
cd ./libzmq-4.3.1
./autogen.sh && ./configure && make && make install
cd ..

# install libcuckoo
git clone https://github.com/efficient/libcuckoo.git
cd ./libcuckoo
# cmake version >= 3
/usr/local/bin/cmake -DCMAKE_INSTALL_PREFIX=../install -DBUILD_EXAMPLES=1 -DBUILD_TESTS=1 ..
make all
make install
