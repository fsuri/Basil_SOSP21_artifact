#!/bin/bash

##use this if I want to upload things onto the machines..
##scp  -r file fs435@${machine}.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/
## ssh ... sudo mv .. ...

## declare an array variable
declare -a arr_servers=("us-east-1-0" "us-east-1-1" "us-east-1-2"
						"eu-west-1-0" "eu-west-1-1" "eu-west-1-2"
						"ap-northeast-1-0" "ap-northeast-1-1" "ap-northeast-1-2"
						"us-west-1-0" "us-west-1-1" "us-west-1-2"
						"eu-central-1-0" "eu-central-1-1" "eu-central-1-2"
						"ap-southeast-2-0" "ap-southeast-2-1" "ap-southeast-2-2"
			  		   )

declare -a arr_clients=("client-0-0" "client-1-0" "client-2-0"
						"client-3-0" "client-4-0" "client-5-0"
						"client-6-0" "client-7-0" "client-8-0"
						"client-9-0" "client-10-0" "client-11-0"
						"client-12-0" "client-13-0" "client-14-0"
						"client-15-0" "client-16-0" "client-17-0"
			  		   )

## now loop through the above array
for host in "${arr_servers[@]}"
do
   echo "uploading binaries to $host"
   #ssh fs435@$host.indicus.morty-pg0.utah.cloudlab.us "sudo rm -rf /mnt/extra/experiments/*"
   rsync -v -r -e ssh /home/florian/Indicus/Basil_SOSP21_artifact/src/bin fs435@$host.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/indicus
done

for host in "${arr_clients[@]}"
do
   echo "uploading binaries to $host"
   #ssh fs435@$host.indicus.morty-pg0.utah.cloudlab.us "sudo rm -rf /mnt/extra/experiments/*"
   rsync -v -r -e ssh /home/florian/Indicus/Basil_SOSP21_artifact/src/bin fs435@$host.indicus.morty-pg0.utah.cloudlab.us:/users/fs435/indicus
done


