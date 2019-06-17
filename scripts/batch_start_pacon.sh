# 
# batch start pacon
#

CONFIG='/root/yubo/mdtest_pacon/batch_start_list'
SSH='ssh '

for node in $(cat $CONFIG)
do
	echo $node
	($SSH $node "cd /root/yubo/mdtest_pacon/pacon/scripts && bash ./start_pacon.sh") &
done

exit 0