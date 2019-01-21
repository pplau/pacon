mkdir ./redis
cd ./redis
wget http://download.redis.io/releases/redis-5.0.3.tar.gz
tar -xvf ./redis-5.0.3.tar.gz
cd ./redis-5.0.3
make
mkdir ./cluster/
mkdir ./cluster/6379
scp root@10.182.171.2:/root/redis/redis-5.0.3/cluster/6379/redis.conf ./cluster/6379
yum -y install ruby rubygems