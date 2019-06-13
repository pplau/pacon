# 
# stop pacon
#

CONFIG='/root/yubo/mdtest_pacon/config'
SSH='ssh '

for node in $(cat $CONFIG)
do
	$SSH $node "ps -ef | grep pacon_server | grep -v grep | awk '{print $2}' | xargs kill -9"
	$SSH $node "ps -ef | grep pacon_server | grep -v grep | awk '{print $2}' | xargs kill -9"
	$SSH $node "ps -ef | grep memcached | grep -v grep | awk '{print $2}' | xargs kill -9"
done

exit 0