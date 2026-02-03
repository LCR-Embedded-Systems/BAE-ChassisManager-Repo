# LCR-ChassisManager-Repo

# Using RESTful API

Run this command to Log In:

curl -c cjar -b cjar -k -H "Content-Type: application/json" -X POST -d '{"data": ["root", "0penBmc"]}' https://192.168.0.104/login



Run commands like this to see data and schemas:

curl -b cjar -k https://192.168.0.104/redfish/v1/Managers/bmc

# Using RMCP

ipmitool -I lanplus -H 192.168.0.104 -U root -P 0penBmc -C 17 mc info
