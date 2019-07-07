# Pacon

# Install:  
`cd /pacon ` 
`bash ./scripts/pacon_install.sh`    
`make   // build test tool`      
`cd ./pacon/server`  
`make` 

# Start Pacon  
`echo #NODE_IP > /pacon/config`  
`modify the file path in start_pacon.sh`  
`bash ./scripts/start_pacon.sh`  
`pacon   // run test`  

# Stop Pacon  
`bash ./scripts/stop_pacon.sh`

# Pacon Interfaces

# Batch Permission Setting
`If application does not call the pacon_set_permission() function, Pacon use the default setting`   
`If application want to define the permission, it must call pacon_set_permission() in each client and make sure they use the same permission setting`   

# Joint CRegions
`add all addresses of remote cregion into ./cr_joint_config`  
`add the workspace of remote cregion into ./crj_info`  
`call cregion_joint() func in the app  // only need to be called by one client in the app`   