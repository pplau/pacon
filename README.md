# Pacon: Improving Scalability and Efficiency of Metadata Service through Partial Consistency (IPDPS'20)
A library that allows existing distributed file system to use partial consistency. Partial consistency provides the application with strong consistency guarantee for only its workspace. Pacon improves the scalability of metadata service by optimizing the consistency model of client side metadata cache. 

Find out more about Pacon in our [paper](https://ieeexplore.ieee.org/abstract/document/9139884):

***Pacon: Improving Scalability and Efficiency of Metadata Service through Partial Consistency. Yubo Liu, Yutong Lu, Zhiguang Chen, and Ming Zhao. In Proceedings of International Parallel and Distributed Processing Symposium (IPDPS), 2020, pp. 986–996.***

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
$ echo $NODE_IP > /pacon/config 
$ echo $NODE_IP:$CLIENT_NUM > /pacon/local_config # CLIENT_NUM is the number of current clients in this node  
# modify the file path in start_pacon_new.sh   
$ bash ./scripts/start_pacon_new.sh
$ ./pacon   # run test  
```

# Stop Pacon  
```bash
$ bash ./scripts/stop_pacon.sh
```

# Interfaces  
Most interefaces in Pacon are similar to POSIX, except rmdir, readdir  


# Batch Permission Setting
* Pacon uses the default setting (all clients ) if a application does not customize the permission information by the *pacon_set_permission()* function.

* If a application defines the permission information, it must call pacon_set_permission() in each client and make sure they use the same permission setting.  
 

# Joint Consistent Regions
1. add all addresses of remote cregion into ./cr_joint_config    
2. add the workspace of remote cregion into ./crj_info    
3. call cregion_joint() func in the app  // only need to be called by one client in the app   