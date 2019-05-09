#
# This script prepares the env and intalls pacon
#

cd ..

yum install -y libevent-devel 

# inatll memc3
git clone https://github.com/efficient/memc3.git
cd ./memc3
autoreconf -fis
./configure
make
