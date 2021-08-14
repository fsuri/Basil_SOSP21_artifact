# SOSP21 Artifact Evaluation #108
This is the repository for the Artifact Evaluation of SOSP'21 submission #108: "Basil: Breaking up BFT with ACID transactions" 


## Claims TODO

our claims are everything we evaluate in our figures..
this prototype implements Basil, and offers replication, sharding and interactive transactions. The prototype uses cryptographically secure hash functions and signatures for all replicas, and implements byzantine clients failing via Stalling or Equivocation, and is robust to both. The artifact contains, and allows to reproduce, experiments for all figures included in the paper. Client requests are unsigned on all prototype systems, as we delegate this problem to the application layer. 
While the prototype uses tolerates many obvious faults such as message corruptions, duplications, it does *not* exhaustively defend against arbitrary byzantine failures or corruptions, nor does it simulate all possible behaviors. For example, the prototype implements fault tolerance (safety) to leader failures during recovery, it does not include code to simulate these, nor does it implement explicit exponential timeouts to enter new views that are necessary for theoretical liveness under partial synchrony.

- **claim1**: Basil comes within competitive performance (both throughput and latency) compared to Tapir, a Crash Fault Tolerant database. Basils current code-base was modified since the Basil results reported in the paper (for microbenchmarks too) to include failure handling, so while results should be largely consistent, they may differ slightly.

- **claim2**: Basil achieves both higher throughput and lower latency than both BFT baselines (TxHotstuff, TxBFTSmart)

- **claim3**: Basil maintains robust throughput for correct clients under attack by byzantine Clients

- **claim4**: All other microbenchmarks are correct...


## Artifacts
The artifact is spread across the following three branches. Please checkout a given branch when validating claims for a respective system.
0. Branch main: Contains the paper, the exeriment scripts, and all experiment configurations used.
1. Branch Basil/Tapir: Contains the source code used for all Basil and Tapir evaluation
3. Branch TxHotstuff: Contains the source code used for TxHotstuff evaluation
4. Branch TxBFTSmart: Contains the source code used for TxBFTSmart evaluation
For convenience, all branches have the necessary experiment scripts as well. Do however, make sure to only run the configs for a specific system on the respective branch.
We recommend making a separate copy of the configs (and experiment scripts) and checking out the other branches.

Alternatively, I could put all the source code in a different repo?



## Validating the Claims
1. Building binaries (for all branches)  
   In order to build Basil and baseline source code in any of the branches several dependencies must be installed. 
   Alternatively, you may use a dedicated Cloudlab "control" machine that is pre-loaded with a fully configured disk image.
   In this case, you may skip step 1, and move straight to setting up a CloudLab experiment. Otherwise, please follow the
   instructions in section "Installing Dependencies" in order to compile the code.

2. Setting up experiments on Cloudlab 
     --> Point to Cloudlab guide: a) how to start a profile and what images to use, b) how to generate these images (all installs + extras)
     
3. Running experiments:
      1) Go to respective branch, 
      2) follow additional installs & remote configs for Hotstuff and BFTsmart
      3) build
      4) experiments.
           --> Show how to run basic dummy test locally, just to confirm that binaries work.
           --> Point to experiment scripts and configs guide. to run remotely.


## Installing Dependencies (NOT NECESSARY IF USING PROVIDED CLOUDLAB IMAGE AND USING CONTROLLER MACHINE TO START SCRIPTS)
Compiling Basil requires the following high level requirements: 
- Operating System: Ubuntu 18.04 LTS, Bionic  
   - You may try to run on Mac, which has worked for us in the past, but is not documented 
   And cannot be easily aided by us. We recommend running on Ubuntu 18.  If you cannot do this locally, we recommend using a CloudLab controller machine - see       section "setting up CloudLab")
   - You may try to use Ubuntu 20.04.2 LTS instead of 18.04 LTS. However, we do not guarantee a fully documented install process, nor precise repicability of our results. In order to use Ubuntu 20.04.2 LTS you will have to manually create a disk image instead of using our supplied images for 18.04 LTS. Note, that using Ubuntu 20.04.2 LTS locally as control machine to generate and upload binaries may *not* be compatible with our cloud lab images running on 18.04 LTS.

(To re-install from scratch use a clean image:   18.04 LTS:     urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU18-64-STD.
20.04 LTS urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU20-64-STD
Or use a image pre-configured by us: urn:publicid:IDN+utah.cloudlab.us+image+morty-PG0:SOSP108.server and urn:publicid:IDN+utah.cloudlab.us+image+morty-PG0:SOSP108.client under public Profile: "SOSP108" https://www.cloudlab.us/p/morty/SOSP108)
- Requires python3 and numpy for scripts
 
- C++ 17 for Main code (gcc version > 5)
- Java Version >= 1.8 for BFT Smart code. We suggest you run the Open JDK java 11 version. (install included below) as our Makefile is hard-coded for it.


### General installation pre-reqs
Before beginning the install process, update your distribution:
1. `sudo apt-get update`
2. `sudo apt-get upgrade`
Then, install the following tools:
3. `sudo apt install python3-pip`
4. `pip3 install numpy` or `python3 -m pip install numpy`
5. `sudo apt-get install autoconf automake libtool curl make g++ unzip valgrind cmake gnuplot pkg-config`


### Development library dependencies
The artifact depends the following development libraries:
- libevent-openssl
- libevent-pthreads
- libevent-dev
- libssl-dev
- libgflags-dev
- libsodium-dev
- libbost-all-dev
- libuv1-dev
You may install them directly using:
- `sudo apt install libsodium-dev libgflags-dev libssl-dev libevent-dev libevent-openssl-2.1-6 libevent-pthreads-2.1-6 libboost-all-dev libuv1-dev`
- If using Ubuntu 20, use `sudo apt install libevent-openssl-2.1-7 libevent-pthreads-2.1-7` instead for openssl and pthreads.

In addition, you need to install the following libraries from source:
- [googletest-1.10](https://github.com/google/googletest/releases/tag/release-1.10.0)
- [protobuf-3.5.1](https://github.com/protocolbuffers/protobuf/releases/tag/v3.5.1)
- [cryptopp-8.2](htps://cryptopp.com/cryptopp820.zip)
- [bitcoin-core/secp256k1](https://github.com/bitcoin-core/secp256k1/)
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3)
- [ed25519-donna] (https://github.com/floodyberry/ed25519-donna)
- [Intel TBB] (https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit/get-the-toolkit.html). In order to compile, will need to configure CPU: https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-intel-oneapi-base-linux/top/before-you-begin.html

Detailed instructions are included below:

We recommend organizing all installs in a dedicated folder:
1. `mkdir dependencies`
2. `cd dependencies`

#### Installing google test
Download the library:
1. `git clone https://github.com/google/googletest.git`
2. `cd googletest`
3. `git checkout release-1.10.0`
Next, build googletest:
4. `sudo cmake CMakeLists.txt`
5. `sudo make -j #cores`
6. `sudo make install`
7. `sudo cp -r googletest /usr/src/gtest-1.10.0`
8. `sudo ldconfig`
9. `cd ..`
Alternatively, you may download and unzip from source: 
1. `get https://github.com/google/googletest/archive/release-1.10.0.zip`
2. `unzip release-1.10.0.zip`  
3. Proceed install as above  


#### Installing protobuf
Download the library:
1. `git clone https://github.com/protocolbuffers/protobuf.git`
2. `cd protobuf`
3. `git checkout v3.5.1`
Next, build protobuf:
4. `./autogen.sh`
5. `./configure`
6. `sudo make -j #cores`
7. `sudo make check -j #cores`
8. `sudo make install`
9. `sudo ldconfig`
10. `cd ..`
Alternatively, you may download and unzip from source: 
1.`wget https://github.com/protocolbuffers/protobuf/releases/download/v3.5.1/protobuf-all-3.5.1.zip`
2.`unzip protobuf-all-3.5.1.zip`
3. Proceed install as above

#### Installing secp256k1
Download and build the library:
1. `git clone https://github.com/bitcoin-core/secp256k1.git`
2. `cd secp256k1`
3. `./autogen.sh`
4. `./configure`
5. `make -j #num_cores`
6. `make check -j`
7. `sudo make install`
8. `sudo ldconfig`
9. `cd ..`


#### Installing cryptopp
Download and build the library:
1. `git clone https://github.com/weidai11/cryptopp.git`
2. `cd cryptopp`
3. `make -j`
4. `sudo make install`
5. `sudo ldconfig`
6. `cd ..`

#### Installing BLAKE3
Download the library:
1. `git clone https://github.com/BLAKE3-team/BLAKE3`
2. `cd BLAKE3/c`
Create a shared libary:
3. `gcc -fPIC -shared -O3 -o libblake3.so blake3.c blake3_dispatch.c blake3_portable.c blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S`
Move the shared libary:
4. `sudo cp libblake3.so /usr/local/lib/`
5. `sudo ldconfig`
6. `cd ../../`

#### Installing ed25519-donna
Download the library:
1. `git clone https://github.com/floodyberry/ed25519-donna`
2. `cd ed25519-donna`
Create a shared library:
3. `gcc -fPIC -shared -O3 -m64 -o libed25519_donna.so ed25519.c -lssl -lcrypto`
Move the shared libary:
4. `sudo cp libed25519_donna.so /usr/local/lib`
5. `sudo ldconfig`
6. `cd ..`

#### Innstalling Intel TBB
Download and execute the installation script:
1. `wget https://registrationcenter-download.intel.com/akdlm/irc_nas/17977/l_BaseKit_p_2021.3.0.3219.sh`
2. `sudo bash l_BaseKit_p_2021.3.0.3219.sh`
(To run the installation script you may have to manually install `apt -y install ncurses-term` if you do not have it already).
Follow the installation instructions: Doing a custom installation saves space as the only dependency is "Intel oneAPI Threading Building Blocks" 
(Use space bar to unmark X other items. You do not need to consent to data collection)

Next, set up the intel TBB environment variables (Reference: https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-intel-oneapi-base-linux/top/before-you-begin.html):
If you installed Intel TBB with root access, it should be installed under /opt/intel/oneapi. Run the following to initialize environment variables:
3. `source /opt/intel/oneapi/setvars.sh`
Note, that this must be done everytime you open a new terminal. You may add it to your .bashrc to automate it:
3. `echo source /opt/intel/oneapi/setvars.sh --force >> ~/.bashrc`
4. `source ~/.bashrc`
(When building on a cloudlab controller instead of locally, the setvars.sh must be sourced manually everytime since bashrc will not be persisted across images. All experiment machines will be source via the experiment scripts, so no further action is necessary there.)


This completes all requires installs for branches Basil/Tapir and TxHotstuff:
### Building:
Go to /src and build:
- `make -j #num-cores`

When building on branch TxBFTSmart the following additional steps are necessary:
#### Additional prereq for BFTSmart (only on TxBFTSmart branch)
First, install Java open jdk 1.11.0 in /usr/lib/jvm and export your LD_LIBRARY_Path:
1. `sudo apt-get install openjdk-11-jdk` Confirm that `java-11-openjdk-amd64` it is installed in /usr/lib/jvm  
2. `export LD_LIBRARY_PATH=/usr/lib/jvm/java-1.11.0-openjdk-amd64/lib/server:$LD_LIBRARY_PATH`
If it is not installed in /usr/lib/jvm source the LD_LIBRARY_PATH accordingly and adjust the following lines in the Makefile accordingly, and source the LD_LIBRARY_PATH accordingly:
`# Java and JNI`
`JAVA_HOME := /usr/lib/jvm/java-11-openjdk-amd64`
`CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux`
`LDFLAGS += -L/usr/lib/jvm/java-1.11.0-openjdk-amd64/lib/server -ljvm`


Next, modify all references of "fs435" or "zw494" in the source code and replace them with your cloudlab id  (TODO: please write script that automates this)
After that, compile BFT-SMART (TODO: integrate this into Makefile)
3. `cd /src/store/bftsmartstore/library`
4. `mkdir bin`
5. `ant`
6. Mkdir jars; cp bin/BftSmart.jar jars/; cp lib/* jars
Finally, return to /src/ and build using `make -j #num-cores`


#### Troubleshooting:
##### Problems with locating libraries:
1. You may need to export your path if your installations are in non-standard locations:
Include: `export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH`
Additionally, you may want to add `/usr/local/lib:/usr/local/share:/usr/local/include` depending on where `make install` puts the libraries.
The default install locations are:

Secp256k1:  /usr/local/lib
CryptoPP: /usr/local/include  /usr/local/bin   /usr/local/share
Blake3: /usr/local/lib
Donna: /usr/local/lib
Googletest: /usr/local/lib /usr/local/include
Protobufs: /usr/local/lib
Intel TBB: /opt/intel/oneapi

2. Building googletest differently:
If you get this error: `make: *** No rule to make target '.obj/gtest/gtest-all.o', needed by '.obj/gtest/gtest_main.a'.  Stop.`
try to install googletest directly into src as follows:
1. `git clone https://github.com/google/googletest.git`
2. `cd googletest`
3. `git checkout release-1.10.0`
4. `rm -rf <PATH>/src/.obj/gtest`
5. `mkdir <PATH>/src/.obj`
6. `cp -r googletest <PATH>/src/.obj/gtest`
7. `cd <PATH>/src/.obj/gtest`
8. `cmake CMakeLists.txt`
9. `make -j`
10. `g++ -isystem ./include -I . -pthread -c ./src/gtest-all.cc`
11. `g++ -isystem ./include -I . -pthread -c ./src/gtest_main.cc`

## Confirming that binaries work locally
Simple single server/single client experiment

Run all this from /src/
To do so need to first generate keys: go to /src and run ./keygen.sh 

Run server:
`DEBUG=store/indicusstore/* store/server --config_path shard-r0.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 0 --protocol indicus --num_keys 1 --debug_stats --indicus_key_path keys &> server.out`

Run client 
`store/benchmark/async/benchmark --config_path shard-r0.config --num_groups 1 --num_shards 1 --protocol_mode indicus --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --client_id 0 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &> client-0.out`

Client should finish within 10 seconds, output file should have summary at the end.
If it doesnt work.. contact us I guess...

## Setting up Cloudlab
In order to run experiments on Cloudlab you will have to register an account with your academic email and create a new project.
Alternatively, if you are unable to get access to create a new project, request to join project "morty" and wait to be accepted.
If you use your local machine to start experiments, then you need to set up and register ssh in order to connect to the cloudlab servers. 
If you are instead using a cloudlab control machine (see next steps) you can skip this step.
To create an ssh key and register it with your ssh agent follow these instructions: https://docs.github.com/en/github/authenticating-to-github/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent (Install ssh if you have not already.)
Next, register your public key under your Cloudlab account user->Manage SSH Keys

You are ready to start up an experiment:
To use a pre-declared profile, use the following public profile "SOSP108" https://www.cloudlab.us/p/morty/SOSP108
The profile by default starts with 18 server machines (follow the naming convention) and 18 client machines, all of which use m510 hardware on the Utah cluster.
When running expeirments for Tapir, you may instead use only 9 server machines (remove the trailing 9 server names from the profile); When running TxHotstuff and TxBFTSmart,
you may use 12 server machines (remove the trailing 6 server names from the profile). Since experiments require a fairly large number of machines, you may have to create a reservation in order to have enough resources. go to the "Make reservation tab" and make a reservation for 36 m510 machines on the Utah cluster (37 if you plan to use a control machine).

This profile includes two disk images "SOSP108.server" and "SOSP108.client" that already include all dependencies and additional machinery necessary to run experiments. If you instead want to build an image from scratch, start by loading a default Ubuntu 18.04 LTS image (urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU18-64-STD). Then, follow the above manual installation guide to install all dependencies (you may skip adding tbb setvars.sh to .bashrc). Additionally, you will have to install the following requisites:
1. NTP:  https://vitux.com/how-to-install-ntp-server-and-client-on-ubuntu/ 
         Confirm that it is running: sudo service ntp status (check for status Active)

2. Data Sets: Build TPCC/Smallbank , move them to /usr/local/etc/  (can skip this on client machines for tpcc)
   Store TPCC data:
- Run tpcc_generator bin from /src/store/benchmark/async/tpcc/
- `./tpcc_generator --num_warehouses=<N> > tpcc-<N>-warehouse`
- Move output file to /usr/local/etc/tpcc-<N>-warehouse
- We used 20 warehouses, so do N=20
 
   Store Smallbank data:
- Run smallbank_generator_main bin from /src/store/benchmark/async/smallbank/
- `./smallbank_generator_main --num_customers=<N>`
- It will generate two files, smallbank_names, and smallbank_data. Move them to /usr/local/etc/
- The server needs both, the client needs only names (not storing smallbank_data saves space for the image)
- We used 1 million customers

   
3. Public Keys: Build crypto keys, move them to /usr/local/etc/donna/
 - Go to /src and use keygen.sh
- Run ./keygen.sh → creates a lot of keys using ./create_key <key_type>  
- By default keygen.sh uses type 4 = Donna, but you can modify it to 3 for secp256k1
- Move the key-pairs in the /keys folder to /usr/local/etc/indicus-keys/donna/ or to /usr/local/etc/indicus-keys/secp256k1/ depending on what type used

4. Create the following two scripts (and enable execution permissions) and place them in /usr/local/etc/
   The scripts are used at runtime by the experiments to disable hyperthreading and turbo respectively.
- Hyperthreading script:
-   name it: `disable_HT.sh`
-   `#!/bin/bash`
-   `for i in {8..15}; do`
-   `   echo "Disabling logical HT core $i."`
-   `   echo 0 > /sys/devices/system/cpu/cpu${i}/online;`
-   `Done`

- Turbo script:
- name it: turn_off_turbo.sh
- `#!/bin/bash`
- `echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo`

Once complete, create a new disk image. Then, start the profile with the disk image specified.
   

## Using a control machine:
When using a control machine (and not your local machine) to start experiments, you will need to source setvars.sh and export the LD path for java before building (everytime you start a new control machine) because those will not be persisted across images.
You dont need to setup ssh(?)

## Running experiments:
Scripts: run: `python3 <PATH>/experiment-scripts/run_multiple_experiments.py <CONFIG>`
The script will load all binaries and configurations onto the remote cloudlab machines, and collect experiment data upon completion.
To use the provided config files, you will need to make the following modifications to each file:
1. "project_name": "morty-pg0" --> change to the name of your project. On cloudlab.us (utah cluster) you will generally need to add "-pg0" to your project_name in order to ssh into the machines. To confirm which is the case for you, try to ssh into a machine directly using
   `ssh <cloudlab-user>@us-east-1-0.<experiment-name>.<project-name>.utah.cloudlab.us`
   
2. "experiment_name": "indicus", --> change to the name of your experiment.
3. src_commit_hash: “branch_name” (i.e. Basil/Tapir, or a specific commit hash)
IMPORTANT: In new scripts dont use this param, it will detach git. Instead just leave it blank (i.e. remove the flag) and the script will automatically use the current branch you are on
4. base_local_exp_directory: “media/floriansuri/experiments” (set the local path where output files will be generated)
5. base_remote_bin_directory_nfs: “users/<cloudlab-user>/indicus” (set the directory on the cloudlab machines for uploading compiled files)
6. src_directory : “/home/floriansuri/Indicus/BFT-DB/src” (Set your local source directory)
7. emulab_user: <cloudlab-username>
8. run_locally: false (set to false to run remote experiments on distributed hardware (cloud lab), set to true to run locally)
   
After the expeirment is complete, the scripts will generate an output folder at your specified base_local_exp_directory. Each folder is timestamped: Go deeper into the folders (total of 3 timestamped folders) until you enter /out. Look for the stats.json file. Throughput measurements will be under: aggregate/combined/tput (or run-stats/combined /tput if you run multiple experiments for error bars) Latency will be under: aggregate/combined /mean
   Plots: go to /plots/tput-clients.png and /plots/lat-tput.png to look at the data points directly. On the control machine looking at stats is necessary
   
### Extra Pre-Configurations necessary for TxHotstuff and TxBFTSmart
   --> see the branch... Extra coudlab configuration is necessary before running (some even necessary before building)
   
Now you are ready to start a experiment:
- Use any of the provided configs under /experiments/<Figures>. Make sure to use the binary code from branches TXHotstuff/TxBFTSmart if you are running any of those configs
- To confirm that we indeed report the max throughput you can modify the num_clients fields on the baseline configs.. One can put multiple [a,b,c] which will run multiple experiments and output the results in a plot.
   

