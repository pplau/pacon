# 
# config pacon cluster accroding to the node's config file
# 

CONFIG='/root/yubo/mdtest_pacon/config_tmp'
SSH='ssh '
SCP='scp '

for node in $(cat $CONFIG)
do
	$SSH $node "rm /root/yubo/mdtest_pacon/config"
	$SSH $node "rm /root/yubo/mdtest_pacon/local_config"
	$SSH $node "rm /root/yubo/mdtest_pacon/pacon/config"
	$SSH $node "rm /root/yubo/mdtest_pacon/pacon/local_config"
	$SCP /root/yubo/mdtest_pacon/config_tmp $node":/root/yubo/mdtest_pacon/config"
	$SCP /root/yubo/mdtest_pacon/config_tmp $node":/root/yubo/mdtest_pacon/pacon/config"
	$SSH $node "touch /root/yubo/mdtest_pacon/local_config"
	$SSH $node "echo $node >> /root/yubo/mdtest_pacon/local_config"
	$SSH $node "touch /root/yubo/mdtest_pacon/pacon/local_config"
	$SSH $node "echo $node >> /root/yubo/mdtest_pacon/pacon/local_config"
done

exit 0

