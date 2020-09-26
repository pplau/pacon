# Pacon: Improving Scalability and Efficiency of Metadata Service through Partial Consistency
A library that allows existing distributed file system to use partial consistency. Partial consistency provides the application with strong consistency guarantee for only its workspace.

Find out more about Pacon in our IPDPS'20 paper:

Yubo Liu, Yutong Lu, Zhiguang Chen, and Ming Zhao. Pacon: Improving Scalability and Efficiency of Metadata Service through Partial Consistency. In Proceedings of International Parallel and Distributed Processing Symposium (IPDPS), 2020, pp. 986–996.

# Install:  
```bash
$ cd /pacon
$ bash ./scripts/pacon_install.sh   
$ make   // build test tool      
$ cd ./pacon/server
$ make 
```

# Start Pacon  
```bash
$ echo $NODE_IP > /pacon/config`  
$ echo $NODE_IP:$CLIENT_NUM > /pacon/local_config //CLIENT_NUM is the number of current clients in this node`  
$ modify the file path in start_pacon_new.sh   
$ bash ./scripts/start_pacon_new.sh`  
$ ./pacon   // run test`  
```

# Stop Pacon  
```bash
$ bash ./scripts/stop_pacon.sh`
```

# Pacon Interfaces  
Most interefaces in Pacon are similar to POSIX, except rmdir, readdir  


# Batch Permission Setting
If application does not call the pacon_set_permission() function, Pacon use the default setting     
If application want to define the permission, it must call pacon_set_permission() in each client and make sure they use the same permission setting     

# Joint Consistent Regions
1. add all addresses of remote cregion into ./cr_joint_config    
2. add the workspace of remote cregion into ./crj_info    
3. call cregion_joint() func in the app  // only need to be called by one client in the app   