# 
# stop pacon
#

CONFIG='/root/yubo/mdtest_pacon/config'
SSH='ssh '

for node in $(cat $CONFIG)
do
	echo $node
	$SSH $node "ps -ef | grep pacon_server | grep -v grep | awk '{print $2}' | xargs kill -9"
	$SSH $node "ps -ef | grep pacon_server | grep -v grep | awk '{print $2}' | xargs kill -9"
	$SSH $node "ps -ef | grep memcached | grep -v grep | awk '{print $2}' | xargs kill -9"
done

rm -rf /mnt/beegfs/pacon_fsync_log/*

exit 0