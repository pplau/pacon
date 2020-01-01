#
# This script prepares the env and intalls pacon
# All libs will be downloaded on the directory that contaions pacon src code
#

cd /root/yubo

yum install -y libevent-devel 

# inatll memc3
#git clone https://github.com/efficient/memc3.git
#cd ./memc3
#autoreconf -fis
#./configure
#make
#cd ..

# install memcached
wget http://www.memcached.org/files/memcached-1.5.14.tar.gz
tar -xvf ./memcached-1.5.14.tar.gz
cd ./memcached-1.5.14
./configure
make && make install
cd ..

#install libmemcached
wget https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
tar -xvf libmemcached-1.0.18.tar.gz
cd ./libmemcached-1.0.18
./configure
make && make install
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

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
ldconfig

# configure ld.so.conf (only for TIANHE-II)
echo "/opt/intel/composer_xe_2013_sp1/lib/intel64/" >> /etc/ld.so.conf
ldconfig


# install libcuckoo
#git clone https://github.com/efficient/libcuckoo.git
#cd ./libcuckoo
# cmake version >= 3
#/usr/local/bin/cmake -DCMAKE_INSTALL_PREFIX=../install -DBUILD_EXAMPLES=1 -DBUILD_TESTS=1 ..
#make all
#make install
