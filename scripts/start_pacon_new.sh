# 
# start pacon
#

CONFIG='/root/yubo/mdtest_pacon/config'
SSH='ssh '

for node in $(cat $CONFIG)
do
	$SSH $node "/root/yubo/memcached-1.5.14/memcached -u root -l $node:11211 -t 16 -m 8192 -d"
done

for node in $(cat $CONFIG)
do
	($SSH $node "cd /root/yubo/mdtest_pacon/pacon/server && (./pacon_server &)") &
	$SSH $node "ldconfig"

exit 0