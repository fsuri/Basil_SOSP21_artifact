# SOSP21 Artifact Evaluation #108
This is the repository for the Artifact Evaluation of SOSP'21 submission #108: "Basil: Breaking up BFT with ACID transactions".

For all questions about the artifact that do not require anonymity please e-mail (or message over google hangouts) "fs435@cornell.edu". For specific questions about 1) building the codebase or 2) running TxBFTSmart, aditionally please CC zw494@cornell.edu. For questions about 3) running TxHotstuff, please CC yz2327@cornell.edu, and 4) for questions about the experiment scripts or cloudlab, please  CC mlb452@cornell.edu.


# Table of Contents
1. [High Level Claims](#Claims)
2. [Artifact Organization](#artifact)
3. [Overview of steps to validate Claims](#validating)
4. [Installing Dependencies and Building Binaries](#installing)
5. [Setting up CloudLab](#cloudlab)
6. [Running Experiments](#experiments)

## Claims 

### General

This artifact contains, and allows to reproduce, experiments for all figures included in the paper #108: "Basil: Breaking up BFT with ACID transactions". 

It contains a prototype implemententation of Basil, a replicated Byzantine Fault Tolerant key-value store offering interactive transactions and sharding. The prototype uses cryptographically secure hash functions and signatures for all replicas, but does not sign client requests on any of the evaluated prototype systems, as we delegate this problem to the application layer. The Basil prototype can simulate Byzantine Clients failing via Stalling or Equivocation, and is robust to both. While the Basil prototype tolerates many obvious faults such as message corruptions and duplications, it does *not* exhaustively implement defences against arbitrary failures or data format corruptions, nor does it simulate all possible behaviors. For example, while the prototype implements fault tolerance (safety) to leader failures during recovery, it does not include code to simulate these, nor does it implement explicit exponential timeouts to enter new views that are necessary for theoretical liveness under partial synchrony.

> **[NOTE]** The Basil codebase is called, for historical reasons,  "*Indicus*". Throughout this document you will find references to Indicus, and many configuration files are named such. All these occurences refer to the Basil prototype.

Basils current codebase (Indicus) was modified beyond some of the results reported in the paper to include the fallback protocol used to defend against client failures. While takeaways remain consistent, individual performance results may differ slightly across the microbenchmarks (better performance in some cases) as other minor modifications to the codebase were necessary to support the fallback protocol implementation.

In addition to Basil, this artifact contains prototype implementations for three baselines: 1) An extension of the original codebase for Tapir, a Crash Failure replicated and sharded key-value store, as well as 2) TxHotstuff and 3) TxBFTSmart, two Byzantine Fault Tolerant replicated and sharded key-value stores built atop 3rd party implementations of consensus modules. 

### Concrete claims in the paper

- **Main claim 1**: Basil's throughput is within a small factor (within 4x on TPCC, 3x on Smallbank, and 2x on Retwis)  of that of Tapir, a state of the art Crash Fault Tolerant database. 

- **Main claim 2**: Basil achieves higher throughput and lower latency than both BFT baselines (>5x over TxHotstuff on TPCC, 4x on Smallbank, and close to 5x on Retwis; close to 4x over TxBFTSmart on TPCC, 3x on Smallbank, and 4x on Retwis).

   All comparisons for claims 1 and 2 are made in the absence of failures.

- **Main claim 3**: The throughput of correct clients in Basil is robust to simulated attack by Byzantine Clients. With 30% Byzantine clients, throughput experienced by correct clients drops by less than 25% in the worst-case.

- **Supplementary**: All other microbenchmarks reported realistically represent Basil.


## Artifact Organization <a name="artifact"></a>

The artifact spans across the following four branches. Please checkout the corresponding branch when validating claims for a given system.
1. Branch main: Contains the Readme, the paper, the exeriment scripts, experiment configurations to validate results, and sample validated outputs.
2. Branch Basil/Tapir: Contains the source code used for all Basil and Tapir evaluation.
3. Branch TxHotstuff: Contains the source code used for TxHotstuff evaluation.
4. Branch TxBFTSmart: Contains the source code used for TxBFTSmart evaluation.

For convenience, all branches include the experiment scripts and configurations necessary to reproduce our results. Do however, *make sure* to only run the configs for a specific system on the respective branch (i.e. only run configs for Basil from the Basil branch, Hotstuff from TxHotstuff, etc.).
Since we require edits to the configs (see Section "Running experiments"), we recommend copying the configs (and experiement scripts) to a separate location, outside the github folder, to avoid duplicating your changes for every branch that you check out.


## Validating the Claims - Overview <a name="validating"></a>

All our experiments were run using Cloudlab (https://www.cloudlab.us/), specifically the Cloudlab Utah cluster. To reproduce our results and validate our claims, you will need to 1) instantiate a matching Cloudlab experiment, 2) build the prototype binaries, and 3) run the provided experiment scripts with the (supplied) configs we used to generate our results. You may go about 2) and 3) in two ways: You can either build and control the experiments from a local machine (easier to parse/record results & troubleshoot, but more initial installs necessary); or, you can build and control the experiments from a dedicated Cloudlab control machine, using pre-supplied disk images (faster setup out of the box, but more overhead to parse/record results and troubleshoot). Both options are outlined in this ReadMe.

The ReadMe is organized into the following high level sections:

1. *Installing pre-requisites and building binaries*

   To build Basil and baseline source code in any of the branches several dependencies must be installed. Refer to section "Installing Dependencies" for detailed instructions on how to install dependencies and compile the code. You may skip this step if you choose to use a dedicated Cloudlab "control" machine using *our* supplied fully configured disk images. Note that, if you choose to use a control machine but not use our images, you will have to follow the Installation guide too, and additionally create your own disk images. More on disk images can be found in section "Setting up Cloudlab".
  

2. *Setting up experiments on Cloudlab* 

     To re-run our experiments, you will need to instantiate a distributed and replicated server (and client) configuration using Cloudlab. We have provided a public profile as well as public disk images that capture the configurations we used to produce our results. Section "Setting up Cloudlab" covers the necessary steps in detail. Alternatively, you may create a profile of your own and generate disk images from scratch (more work) - refer to section "Setting up Cloudlab" as well for more information. Note, that you will need to use the same Cluster (Utah) and machine types (m510) to reproduce our results.


3. *Running experiments*

     To reproduce our results you will need to checkout the respective branch, and run the supplied experiment scripts using the supplied experiment configurations. Section "Running Experiments" includes instructions for using the experiment scripts, modifying the configurations, and parsing the output. TxHotstuff and TxBFTSmart require additional configuration steps, also detailed in section "Running Experiments".
     

## Installing Dependencies (Skip if using Cloudlab control machine using supplied images) <a name="installing"></a>

The high-level requirements for compiling Basil and the baselines are: 
- Operating System: Ubuntu 18.04 LTS, Bionic 
   - We recommend running on Ubuntu 18.04 LTS, Bionic, as a) binaries were built and run on this operating system, and b) our supplied images use Ubuntu 18.04 LTS.    - If you cannot do this locally, consider using a CloudLab controller machine - see section "Setting up CloudLab".
   <!-- You may try to use Ubuntu 20.04.2 LTS instead of 18.04 LTS. However, we do not guarantee a fully documented install process, nor precise repicability of our results. Note, that using Ubuntu 20.04.2 LTS locally (or as control machine) to generate and upload binaries may *not* be compatible with running cloudlab machines using our cloud lab images (as they use 18.04 LTS(. In order to use Ubuntu 20.04.2 LTS you may have to manually create new disk images for CloudLab instead of using our supplied images for 18.04 LTS to guarantee library compatibility. -->
   <!-- You may try to run on Mac, which has worked for us in the past, but is not documented in the following ReadMe and may not easily be trouble-shooted by us. -->
  
- Requires python3 
- Requires C++ 17 
- Requires Java Version >= 1.8 for BFTSmart. We suggest you run the Open JDK java 11 version (install included below) as our Makefile is currently hard-coded for it.


### General installation pre-reqs

Before beginning the install process, update your distribution:
1. `sudo apt-get update`
2. `sudo apt-get upgrade`

Then, install the following tools:

3. `sudo apt install python3-pip`
4. `sudo -H pip3 install numpy`
5. `sudo apt-get install autoconf automake libtool curl make g++ unzip valgrind cmake gnuplot pkg-config ant`


### Development library dependencies

The prototype implementations depend the following development libraries:
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

In addition, you will need to install the following libraries from source (detailed instructions below):
- [googletest-1.10](https://github.com/google/googletest/releases/tag/release-1.10.0)
- [protobuf-3.5.1](https://github.com/protocolbuffers/protobuf/releases/tag/v3.5.1)
- [cryptopp-8.2](https://github.com/weidai11/cryptopp/releases/tag/CRYPTOPP_8_2_0) <!-- (htps://cryptopp.com/cryptopp820.zip)-->
- [bitcoin-core/secp256k1](https://github.com/bitcoin-core/secp256k1/)
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3)
- [ed25519-donna](https://github.com/floodyberry/ed25519-donna)
- [Intel TBB](https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit/get-the-toolkit.html). 
   - You will additionally need to [configure your CPU](https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-intel-oneapi-base-linux/top/before-you-begin.html) before being able to compile the prototypes.

Detailed install instructions:

We recommend organizing all installs in a dedicated folder:

1. `mkdir dependencies`
2. `cd dependencies`

#### Installing google test

Download the library:

1. `git clone https://github.com/google/googletest.git`
2. `cd googletest`
3. `git checkout release-1.10.0`

Alternatively, you may download and unzip from source: 

1. `get https://github.com/google/googletest/archive/release-1.10.0.zip`
2. `unzip release-1.10.0.zip`  

Next, build googletest:

4. `sudo cmake CMakeLists.txt`
5. `sudo make -j $(nproc)`
6. `sudo make install`
7. `sudo cp -r googletest /usr/src/gtest-1.10.0`
8. `sudo ldconfig`
9. `cd ..`


#### Installing protobuf

Download the library:

1. `git clone https://github.com/protocolbuffers/protobuf.git`
2. `cd protobuf`
3. `git checkout v3.5.1`

Alternatively, you may download and unzip from source: 

1.`wget https://github.com/protocolbuffers/protobuf/releases/download/v3.5.1/protobuf-all-3.5.1.zip`
2.`unzip protobuf-all-3.5.1.zip`

Next, build protobuf:

4. `./autogen.sh`
5. `./configure`
6. `sudo make -j $(nproc)`
7. `sudo make check -j $(nproc)`
8. `sudo make install`
9. `sudo ldconfig`
10. `cd ..`


#### Installing secp256k1

Download and build the library:

1. `git clone https://github.com/bitcoin-core/secp256k1.git`
2. `cd secp256k1`
3. `./autogen.sh`
4. `./configure`
5. `make -j $(nproc)`
6. `make check -j $(nproc)`
7. `sudo make install`
8. `sudo ldconfig`
9. `cd ..`


#### Installing cryptopp

Download and build the library:

1. `git clone https://github.com/weidai11/cryptopp.git`
2. `cd cryptopp`
3. `make -j $(nproc)`
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

#### Installing Intel TBB

Download and execute the installation script:

1. `wget https://registrationcenter-download.intel.com/akdlm/irc_nas/17977/l_BaseKit_p_2021.3.0.3219.sh`
2. `sudo bash l_BaseKit_p_2021.3.0.3219.sh`
(To run the installation script you may have to manually install `apt -y install ncurses-term` if you do not have it already).

Follow the installation instructions: 
- It will either open a GUI installation interface if availalbe, or otherwise show the same within the shell (e.g. on a control machine)
- Select custom installation 
- You need only "Intel oneAPI Threading Building Blocks". You may uncheck every other install -- In the shell use the space bar to uncheck all items marked with an X 
- Skip Eclipse IDE configuration
- You do not need to consent to data collection

Next, set up the intel TBB environment variables (Refer to https://software.intel.com/content/www/us/en/develop/documentation/get-started-with-intel-oneapi-base-linux/top/before-you-begin.html if necessary):

If you installed Intel TBB with root access, it should be installed under `/opt/intel/oneapi`. Run the following to initialize environment variables:

3. `source /opt/intel/oneapi/setvars.sh`

Note, that this must be done everytime you open a new terminal. You may add it to your .bashrc to automate it:

4. `echo source /opt/intel/oneapi/setvars.sh --force >> ~/.bashrc`
5. `source ~/.bashrc`

(When building on a cloudlab controller instead of locally, the setvars.sh must be sourced manually everytime since bashrc will not be persisted across images. All other experiment machines will be source via the experiment scripts, so no further action is necessary there.)


This completes all requires installs for branches Basil/Tapir and TxHotstuff. 

When building TxBFTSmart (on branch TxBFTSmart) the following additional steps are necessary:

#### Additional prereq for BFTSmart (only on TxBFTSmart branch)

First, install Java open jdk 1.11.0 in /usr/lib/jvm and export your LD_LIBRARY_Path:

1. `sudo apt-get install openjdk-11-jdk` Confirm that `java-11-openjdk-amd64` it is installed in /usr/lib/jvm  
2. `export LD_LIBRARY_PATH=/usr/lib/jvm/java-1.11.0-openjdk-amd64/lib/server:$LD_LIBRARY_PATH`
3. `sudo ldconfig`

If it is not installed in `/usr/lib/jvm` then source the `LD_LIBRARY_PATH` according to your install location and adjust the following lines in the Makefile with your path:

- `# Java and JNI`
- `JAVA_HOME := /usr/lib/jvm/java-11-openjdk-amd64`  (adjust this)
- `CFLAGS += -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux`
- `LDFLAGS += -L/usr/lib/jvm/java-1.11.0-openjdk-amd64/lib/server -ljvm`  (adjust this)


### Building binaries:
   
Finally, you can build the binaries (you will need to do this anew on each branch):
Navigate to `SOSP21_artifact_eval/src` and build:
- `make -j $(nproc)`



#### Troubleshooting:
   
##### Problems with locating libraries:
   
1. You may need to export your `LD_LIBRARY_PATH` if your installations are in non-standard locations:
   The default install locations are:

   - Secp256k1:  /usr/local/lib
   - CryptoPP: /usr/local/include  /usr/local/bin   /usr/local/share
   - Blake3: /usr/local/lib
   - Donna: /usr/local/lib
   - Googletest: /usr/local/lib /usr/local/include
   - Protobufs: /usr/local/lib
   - Intel TBB: /opt/intel/oneapi

 Run `export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/share:/usr/local/include:$LD_LIBRARY_PATH` (adjusted depending on where `make install` puts the libraries) followed by `sudo ldconfig`.
   
2. If you installed more Intel API tools besides "Intel oneAPI Threading Building Blocks", then the Intel oneAPI installation might have  installed a different protobuf binary. Since the application pre-pends the Intel install locations to `PATH`, you may need to manually pre-pend the original directories. Run: `export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH`

3. Building googletest differently:
   
   If you get an error: `make: *** No rule to make target '.obj/gtest/gtest-all.o', needed by '.obj/gtest/gtest_main.a'.  Stop.` try to install googletest directly into the `src` directory as follows:
   1. `git clone https://github.com/google/googletest.git`
   2. `cd googletest`
   3. `git checkout release-1.10.0`
   4. `rm -rf <Relative-Path>/SOSP21_artifact_eval/src/.obj/gtest`
   5. `mkdir <Relative-Path>/SOSP21_artifact_eval/src/.obj`
   6. `cp -r googletest <Relative-Path>/SOSP21_artifact_eval/src/.obj/gtest`
   7. `cd <Relative-Path>/SOSP21_artifact_eval/src/.obj/gtest`
   8. `cmake CMakeLists.txt`
   9. `make -j $(nproc)`
   10. `g++ -isystem ./include -I . -pthread -c ./src/gtest-all.cc`
   11. `g++ -isystem ./include -I . -pthread -c ./src/gtest_main.cc`

### Confirming that Basil binaries work locally (optional sanity check)
You may want to run a simple toy single server/single client experiment to validate that the binaries you built do not have an obvious error.

Navigate to `SOSP21_artifact_eval/src`. Run `./keygen.sh` to generate local priv/pub key-pairs. 

Run server:
   
`DEBUG=store/indicusstore/* store/server --config_path shard-r0.config --group_idx 0 --num_groups 1 --num_shards 1 --replica_idx 0 --protocol indicus --num_keys 1 --debug_stats --indicus_key_path keys &> server.out`

Run client:
   
`store/benchmark/async/benchmark --config_path shard-r0.config --num_groups 1 --num_shards 1 --protocol_mode indicus --num_keys 1 --benchmark rw --num_ops_txn 2 --exp_duration 10 --client_id 0 --warmup_secs 0 --cooldown_secs 0 --key_selector zipf --zipf_coefficient 0.0 --stats_file "stats-0.json" --indicus_key_path keys &> client-0.out`

The client should finish within 10 seconds and the output file `client-0.out` should include summary of the transactions committed at the end. Cancel the server manually using `ctrl C`. 


## Setting up Cloudlab <a name="cloudlab"></a>
   
To run experiments on [Cloudlab](https://www.cloudlab.us/) you will need to request an account with your academic email (if you do not already have one) and create a new project  To request an account click [here](https://cloudlab.us/signup.php). You can create a new project either directly while requesting an account, or by selecting "Start/Join project" in your account drop down menu.

We have included screenshots below for easy usebility. Follow the [cloudlab manual](http://docs.cloudlab.us/) if you need additional information for any of the outlined steps. 

If you face any issues with registering, please make a post at the [Cloudlab forum](https://groups.google.com/g/cloudlab-users?pli=1). Replies are usually very swift during workdays on US mountain time (MT). Alternatively -- but *not recommended* --, if you are unable to get access to create a new project, request to join project "morty" and wait to be accepted. Reach out to mlb452@cornell.edu if you are not accepted, or unsure how to join.

![image](https://user-images.githubusercontent.com/42611410/129490833-eb99f58c-8f0a-43d9-8b99-433af5dab559.png)

To start experiments that connect to remote Cloudlab machines, you will need to set up ssh and register your key with Cloudlab. This is necessary regardless of whether you are using your local machine or a Cloudlab control machine. 

Install ssh if you do not already have it: `sudo apt-get install ssh`. To create an ssh key and register it with your ssh agent follow these instructions: https://docs.github.com/en/github/authenticating-to-github/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent. Next, register your public key under your Cloudlab account user->Manage SSH Keys. Alternatively, you may add your keys driectly upon project creation.

Next, you are ready to start up an experiment:

To use a pre-declared profile supplied by us, start an experiment using the public profile ["SOSP108"](https://www.cloudlab.us/p/morty/SOSP108). If you face any issues using this profile (or the disk images specified below) please make a post at the [Cloudlab forum](https://groups.google.com/g/cloudlab-users?pli=1) or contact `fs435@cornell.edu` and `mlb452@cornell.edu`.
![image](https://user-images.githubusercontent.com/42611410/129490911-8c97d826-caa7-4f04-95a7-8a2c8f3874f7.png)

This profile by default starts with 18 server machines and 18 client machines, all of which use m510 hardware on the Utah cluster. This profile includes two disk images "SOSP108.server" (`urn:publicid:IDN+utah.cloudlab.us+image+morty-PG0:SOSP108.server`) and "SOSP108.client" (`urn:publicid:IDN+utah.cloudlab.us+image+morty-PG0:SOSP108.client`) that already include all dependencies and additional setup necessary to run experiments. Check the box "Use Control Machine" if you want to build binaries and run all experiments from one of the Cloudlab machines.
![image](https://user-images.githubusercontent.com/42611410/129490922-a99a1287-6ecc-4d50-b05d-dfe7bd0496d9.png)
Click "Next" and name your experiment (e.g. "sosp108"). In the example below, our experiment name is "indicus", and the project name is "morty". All our pre-supplied experiment configurations use these names as default, and you will need to change them accordingly to your chosen names (see section "Running Experiments").
![image](https://user-images.githubusercontent.com/42611410/129490940-6c527b08-5def-4158-afd2-bc544e4758ab.png)
Finally, set a duration and start your experiment. Starting all machines may take a decent amount of time as the server disk images contain large datasets that need to be loaded. Wait for it to be "ready":
![image](https://user-images.githubusercontent.com/42611410/129490974-f2b26280-d5e9-42ca-a9fe-82b80b8e2349.png)
You may ssh into the machines to test your connection using the ssh commands shown under "List View" or by using `ssh <cloudlab-username>@<node-name>.<experiment-name>.<project-name>-pg0.<cluster-domain-name>`. In the example below it would be: `ssh fs435@us-east-1-0.indicus.morty-pg0.utah.cloudlab.us`.
![image](https://user-images.githubusercontent.com/42611410/129490991-035a1865-43c3-4238-a264-e0d43dd0095f.png)


Since experiments require a fairly large number of machines, you may have to create a reservation in order to have enough resources. Go to the "Make reservation tab" and make a reservation for 36 m510 machines on the Utah cluster (37 if you plan to use a control machine). 
![image](https://user-images.githubusercontent.com/42611410/129491361-b13ef31b-707b-4e02-9c0f-800e6d9b4def.png)

All experiments work using an experiment profile with 18 servers (36 total machines), but if you cannot get access to enough machines, it suffices to use 9 server machines for Tapir (remove the trailing 9 server names from the profile, i.e. `['us-east-1-0', 'us-east-1-1', 'us-east-1-2', 'eu-west-1-0', 'eu-west-1-1', 'eu-west-1-2', 'ap-northeast-1-0', 'ap-northeast-1-1', 'ap-northeast-1-2']`); or 12 server machines when running TxHotstuff and TxBFTSmart (remove the trailing 6 server names from the profile, i.e. `['us-east-1-0', 'us-east-1-1', 'us-east-1-2', 'eu-west-1-0', 'eu-west-1-1', 'eu-west-1-2', 'ap-northeast-1-0', 'ap-northeast-1-1', 'ap-northeast-1-2', 'us-west-1-0', 'us-west-1-1', 'us-west-1-2']`). 

### Using a control machine (skip if using local machine)
When using a control machine (and not your local machine) to start experiments, you will need to source setvars.sh and may need to export the LD_LIBRARY_PATH for the Java dependencies (see section "Install Dependencies") before building. You will need to do this everytime you start a new control machine because those may not be persisted across images.

Connect to your control machine via ssh: `ssh <cloudlab-user>@control.<experiment-name>.<project-name>.utah.cloudlab.us.`  You may need to add `-pg0` to your project name. (i.e. if your project is called "sosp108", it may need to be "sosp108-pg0" in order to connect. Find out by Trial and Error.).

### Using a custom profile (skip if using pre-supplied profile)

If you decide to instead [create your own profile](https://www.cloudlab.us/manage_profile.php), use the following parameters (be careful to follow the same naming conventions of our profile for the servers or the experiment scripts/configuration provided will not work). You will need to buid your own disk image from scratch, as the public image is tied to the public profile. (You can try if the above images work, but likely they will not).

- Number of Replicas: `['us-east-1-0', 'us-east-1-1', 'us-east-1-2', 'eu-west-1-0', 'eu-west-1-1', 'eu-west-1-2', 'ap-northeast-1-0', 'ap-northeast-1-1', 'ap-northeast-1-2', 'us-west-1-0', 'us-west-1-1', 'us-west-1-2', 'eu-central-1-0', 'eu-central-1-1', 'eu-central-1-2', 'ap-southeast-2-0', 'ap-southeast-2-1', 'ap-southeast-2-2']`
- Number of sites (DCs): 6
- Replica Hardware Type: `m510`
- Replica storage: `64GB`
- Replica disk image: Your own (server) image
- Client Hardware Type: `'m510'` (add the '')
- Client storage: `16GB`
- Client disk image: Your own (client) image
- Number of clients per replica: `1`
- Total number of clients: `0` (this will still create 18 clients)
- Use control machine?:  Check this if you plan to use a control machine
- Control Hardware Type: `m510`

### Building and configuring disk images from scratch (skip if using pre-supplied images)
If you want to build an image from scratch, follow the instructions below:

Start by choosing to load a default Ubuntu 18.04 LTS image as "Replica disk image" and "Client disk image": `urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU18-64-STD)` - for Ubuntu 20.04 LTS use: `urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU20-64-STD`. 

Next, follow the above manual installation guide (section "Installing Dependencies" to install all dependencies (you can skip adding tbb setvars.sh to .bashrc). 

Additionally, you will have to install the following requisites:
1. **NTP**:  https://vitux.com/how-to-install-ntp-server-and-client-on-ubuntu/ 
   
   Confirm that it is running: sudo service ntp status (check for status Active)

2. **Data Sets**: Build TPCC/Smallbank data sets and move them to /usr/local/etc/ 
   
      **Store TPCC data:**
   - Navigate to`SOSP21_artifact_eval/src/store/benchmark/async/tpcc` 
   - Run `./tpcc_generator --num_warehouses=<N> > tpcc-<N>-warehouse`
   - We used 20 warehouses, so replace `<N>` with `20`
   - Move output file to `/usr/local/etc/tpcc-<N>-warehouse`
   - You can skip this on client machines and create a separate disk image for cients without. This will considerably reduce image size and speed up experiment startup. 
 
      **Store Smallbank data:**
   - Navigate to `SOSP21_artifact_eval/src/store/benchmark/async/smallbank/`
   - Run `./smallbank_generator_main --num_customers=<N>`
   - We used 1 million customers, so replace `<N>` with `1000000`
   - The script will generate two files, smallbank_names, and smallbank_data. Move them to /usr/local/etc/
   - The server needs both, the client needs only smallbank_names (not storing smallbank_data saves space for the image)

   
3. **Public Keys**: Generate Pub/Priv key-pairs, move them to /usr/local/etc/donna/

    - Navigate to `SOSP21_artifact_eval/src` and run `keygen.sh`
    - By default keygen.sh uses type 4 = Ed25519 (this is what we evaluated unde); it can be modifed secp256k1 (type 3), but this requires editing the config files as well. (do not do this, to re-produce our experiments)
    - Move the key-pairs in the `/keys` folder to `/usr/local/etc/indicus-keys/donna/` (or to `/usr/local/etc/indicus-keys/secp256k1/` depending on what type used)

4. **Helper scripts**: 

    (On branch main) Navigate to SOSP21_artifact_eval/helper-scripts. Copy both these scripts (with the exact name) and place them in `/usr/local/etc` on the cloudlab machine. Add execution permissions: `chmod +x disable_HT.sh; chmod +x turn_off_turbo.sh` The scripts are used at runtime by the experiments to disable hyperthreading and turbo respectively.

   
Once complete, create a new disk image (separate ones for server and client if you want to save space/time). Then, start the profile by choosing the newly created disk image.
To create a disk image, select "Create Disk Image" and name it accordingly.
![image](https://user-images.githubusercontent.com/42611410/129491499-eb7d0618-5dc4-4942-a25a-3b4a955c5077.png)

   
  
   

## Running experiments <a name="experiments"></a>

Hurray! You have completed the tedious process of installing the binaries and setting up Cloudlab. Next, we will cover how to run experiments. This is a straightfoward, but time-consuming process, and importantly requires good network connectivity to upload binaries to the remote machines and download experiment results. Uploading binaries on high speed (e.g university) connections takes a few minutes and needs to be done only once per branch -- however, if your uplink speed is low it may take (as I have painstakenly experienced in preparing this documentation for you) several hours. Downloading experiment outputs requires a moderate amount of download bandwidth and is usually quite fast. This section is split into 4 subsections: 1) Pre-configurations for Hotstuff and BFTSmart, 2) Using the experiment scripts, 3) Parsing outputs, and finally 4) reproducing experiment claims 1-by-1.

Before you proceed, please confirm that the following credentials are accurate:
1. Cloudlab-username `<cloudlab-user>`: e.g. "fs435"
2. Cloudlab experiment name `<experiment-name>`: e.g. "indicus"
3. Cloudlab project name `<project-name`>: e.g. "morty-pg0"  (May need the "-pg0" extension)

Confirm these by attempting to ssh into a machine you started (on the Utah cluster): `ssh <cloudlab-user>@us-east-1-0.<experiment-name>.<project-name>.utah.cloudlab.us`

### 1) Pre-configurations for Hotstuff and BFTSmart

On branches TxHotstuff and TxBFTSmart you will need to complete the following pre-configuration steps before running an experiment script:

1. **TxHotstuff**
   1. Navigate to `SOSP21_artifact_eval/src/scripts`
   2. Run `./batch_size <batch_size>` to configure the internal batch size used by the Hotstuff Consensus module. See sub-section "1-by-1 experiment guide" for what settings to use
   3. Open file `config_remote.sh` and edit the following lines to match your Cloudlab credentials:
      - Line 3: `TARGET_DIR="/users/<cloudlab-user>/config/"`
      - Line 14: `rsync -rtuv config <cloudlab-user>@${machine}.<experiment-name>.<project-name>.utah.cloudlab.us:/users/<cloudlab-user>/`
   4. Finally, run `./config_remote.sh` 
   5. This will upload the necessary configurations for the Hotstuff Consensus module to the Cloudlab machines.

3. **TxBFTSmart**
   1. Navigate to `SOSP21_artifact_eval/src/scripts`
   2. Run `./one_step_config.sh <Local SOSP21_artifact_eval directory> <cloudlab-user> <experiment-name> <project-name> <cluster-domain-name>`
   3. For example: `./one_step_config.sh /home/florian/Indicus/SOSP21_artifact_eval fs435 indicus morty-pg0 utah.cloudlab.us`
   4. This will upload the necessary configurations for the BFTSmart Conesnsus module to the Cloudlab machines.
      - Troubleshooting: Make sure files `server-hosts` and `client-hosts` in `/src/scripts/` do not contain empty lines at the end

### 2) Using the experiment scripts

To run an experiment you simply need to run: `python3 SOSP21_artifact_eval/experiment-scripts/run_multiple_experiments.py <CONFIG>` using a specified configuration JSON file (see below). The script will load all binaries and configurations onto the remote cloudlab machines, and collect experiment data upon completion. We have provided experiment configurations for all experiments claimed by the paper that you can find under `SOSP21_artifact_eva./experiment-configs`. In order for you to use them yourself you will need to make the following modifications to each file (Ctrl F and Replace in all the configs to save time):

#### Required Modifications:
1. `"project_name": "morty-pg0"`
   - change the value field to the name of your Cloudlab project `<project-name>`. On cloudlab.us (utah cluster) you will generally need to add "-pg0" to your project_name in order to ssh into the machines. To confirm which is the case for you, try to ssh into a machine directly using `ssh <cloudlab-user>@us-east-1-0.<experiment-name>.<project-name>.utah.cloudlab.us`.  
2. `"experiment_name": "indicus"`
   - change the value field to the name of your Cloudlab experiment `<experiment-name>`.
3. `"base_local_exp_directory": “home/florian/Indicus/output”`
   - Set the value field to be the local path (on your machine or the control machine) where experiment output files will be downloaded to and aggregated. 
4. `"base_remote_bin_directory_nfs": “users/<cloudlab-user>/indicus”` 
   - Set the field `<cloudlab-user>`. This is the directory on the Cloudlab machines where the binaries will be uploaded
5. `"src_directory" : “/home/florian/Indicus/SOSP21_artifact_eval/src”` 
   - Set the value field to your local path (on your machine or the control machine) to the source directory 
6. `"emulab_user": "<cloudlab-username>"`
   - Set the field `<cloudlab-user>`. 

#### **Optional** Modifications 
1. Experiment duration:
   - The provided configs are by default, for time convenience, set to run for 30 seconds total, using a warmup and cooldown period of 5 seconds respectively. 
      - "client_experiment_length": 30,
      - "client_ramp_down": 5,
      - "client_ramp_up": 5,
   - All experiment results in the paper were run for longer: 90 seconds total, with a warmup and cooldown period of 30 seconds respectively. If you want to run the experiments as long, replace the above settings with respective durations. For cross-validation purposes shorter experiments will suffice and save you time (and memory, since output files will be smaller)
   
2. Number of experiments:
   - The provided config files by default run the configured experiment once. Experiment results from the paper for 1-Workloads and 2-Failures were instead run several times (four times) and report the mean throughput/latency as well as standard deviations across the runs. For cross-validation purposes, this is not necessary. If you do however want to run the experiment multiple times, you can modify the config entry `num_experiment_runs: 1` to a repitition of your choice, which will automatically run the experiment the specified amount of times, and aggregate the joint statistics.
3. Number of clients:
   - The provided config files by default run an experiment for a single client setting that corresponds to the rough "peak" for throughput. Client settings are defined by the following JSON entries:
      - "client_total": [[71]],
         - "client_total" specifies the upper limit for total client *processes* used
      - "client_processes_per_client_node": [[8]],
         - "client_proccesses_per_client_node" specifies the number of client processes run on each server machine. 
      - "client_threads_per_process": [[2]],
         - "client_threads_per_process" specifies the number of client threads run by each client process.  
   - The *absolute total number* of clients used by an experiment is: **Total clients** *= max(client_total, num_servers x client_processes_per_client_node) *x client_threads_per_process*. For Tapir "num_servers" = 9, for Basil "num_servers" = 18, and for TxHotstuff/TxBFTSmart "num_servers" = 12.
   - To determine the peak **Total clients** settings we ran a *series* of client settings for each experiment. For simple cross-validation purposes this is not necessary - If you do however want to, you can run multiple settings automatically by specifying a list of client settings. For example:
      - "client_total": [[71, 54, 63, 71, 54, 63]],
      - "client_processes_per_client_node": [[8, 6, 7, 8, 6, 7]],
      - "client_threads_per_process": [[1, 2, 2, 2, 3, 3]]
   - For convenience, we have included such series (in comments) in all configuration files. To use them, uncomment them (by removing the underscore `_`) and comment out the pre-specified single settings (by adding an underscore `_`).
   - 
#### Starting an experiment:
You are ready to start an experiment. Use any of the provided JSON configs under `SOSP21_artifact_eval/experiment-configs/<PATH>/<config>.json`. **Make sure** to use the binaries from a respective branch when running configs for Basil/Tapir, TxHotstuff, and TxBFTSmart respectively. All microbenchmark configs are Basil exclusive.

Run: `python3 <PATH>/SOSP21_artifact_eval/experiment-scripts/run_multiple_experiments.py <PATH>SOSP21_artifact_eval/experiment-configs/<PATH>/<config>.json` and wait!

Optional: To monitor experiment progress you can ssh into a server machine (us-east-1-0) and run htop. During the experiment run-time the cpus will be loaded (to different degrees depending on contention and client count).
  
   
### 3) Parsing outputs
After the experiment is complete, the scripts will generate an output folder at your specified `base_local_exp_directory`. Each folder is timestamped. 

To parse experiment results you have 2 options:
1. (Recommended) Looking at the `stats.json` file:
   1. Navigate into the timestamped folder, and keep following the timestamped folders until you enter folder `/out`. Open the file `stats.json`. When running multiple client settings, each setting will generate its own internal timestamped folder, with its own `stats.json` file. Multiple runs of the same experiment setting instead will directly be aggregated in a single `stats.json` file.
   2. In the `stats.json` file search for the Json field: `run_stats: ` 
   3. Then, search for the JSON field: `combined:`
   4. Finally, find Throughput measurments under `tput`, Latency measurements under `mean`, and Throughput per Correct client under `tput_s_honest` (**this will exist only for failure experiments**).
2. Looking at generated png plots:
   Alternatively, on your local machine you can navigate to `<time_stamped_folder>/plots/tput-clients.png` and `<time_stamped_folder>/plots/lat-tput.png` to look at the data points directly. Currently however, it shows as "Number of Clients" the number of total client **processes** (i.e. `client_total`) and not the number of **Total clients** specified above. Keep this in mind when viewing output that was generated for experiments with a list of client settings.
   
 Find below, some example screenshots from looking at a provided experiment output from `SOSP21_artifact_eval/sample-output/Validated Results`:

   Experiment output folder:

   ![image](https://user-images.githubusercontent.com/42611410/129566751-a179de6e-8b22-49bc-96f5-bfb517e8eb9e.png)

   Subfolder that contains `stats.json`. Note: To save memory, we have removed all the server/client folders in /sample-output that you will see yourself.

   ![image](https://user-images.githubusercontent.com/42611410/129566648-808ea2d7-a2c0-48b4-b2e8-57221b040f13.png) 

   JSON fields `run_stats` and `combined`. Note: `combined` might not be the first entry within `run_stats` in every config, so double check to get the right data.

   ![image](https://user-images.githubusercontent.com/42611410/129566877-87000119-c43b-4fa2-973a-2a9e571d9351.png)

   Throughput: 

   ![image](https://user-images.githubusercontent.com/42611410/129566950-f0126263-7bd4-4978-8270-9051ad403a37.png)

   Latency: 

   ![image](https://user-images.githubusercontent.com/42611410/129566988-5fc99464-a6c2-4e7a-8108-320c55e5b82e.png)

   Correct Client Throughput: 

   ![image](https://user-images.githubusercontent.com/42611410/129567041-4f002dca-5c6f-4617-bab5-87d7f4bd1af0.png)

   Alternatively Plots (Throughput):

   ![image](https://user-images.githubusercontent.com/42611410/129566828-694cf8e2-2c25-4e5b-941e-9a745340ea74.png)


Next, we will go over each included experiment individually to provide some pointers.

### 4) Reproducing experiment claims 1-by-1
   
We have included recently re-validated experiment outputs (for most of the experiments) for easy cross-validation of the claimed througput (and latency) numbers under `/sample-output/ValidatedResults`. To directly compare against the numbers reported in our paper please refer to the figures there -- we include rough numbers below as well. Some of Basil' reported microbenchmark performances have changed slightly (documented below) as the codebase has since matured, but all takeaways remain consistent.

#### **1 - Workloads**:
We report evaluation results for 3 workloads (TPCC, Smallbank, Retwis) and 4 systems: Tapir (Crash Failure baseline), Basil (our system), TxHotstuff (BFT baseline), and TxBFTSmart (BFT baseline). All systems were evaluated using 3 shards each, but use different replication factors.

   1. **Tapir**: 
   
   > :warning: Make sure to run on branch `Basil/Tapir`. Build the binaries before running (see instructions above)
   
   Reproducing our claimed results is straightforward and requires no additional setup besides running the included configs under `/experiment-configs/1-Workloads/1.Tapir`.  Reported peak results were roughly:
   
      - TPCC: Throughput: ~20k tx/s, Latency: ~7 ms
      - Smallbank: Throughput: ~ 61,5k tx/s, Latency: ~2.3 ms
      - Retwis: Throughput: ~45k tx/s, Latency: 2 ms
      All Tapir experiments were run using 24 shards to allow for even use of resources across systems, since unlike the BFT systems (all use 3 shards) that require multiple cores to handle cryptography, Tapir's servers are single threaded. \


   2. **Basil**: 
   > :warning: Make sure to run on branch `Basil/Tapir`. Build the binaries before running (see instructions above)
   
   Use the configurations under `/experiment-configs/1-Workloads/2.Basil`. Reported peak results were roughly:
   
      - TPCC: Throughput: ~4.8k tx/s, Latency: ~30 ms
      - Smallbank: Throughput: ~23k tx/s Latency: ~12 ms
      - Retwis: Throughput: ~24 k tx/s, Latency: ~10 ms
        
   > **[NOTE]** On both Smallbank and Retwis throughput has decreased (and Latency has increased) ever so slightly since the reported paper results, as the system now additionally implements failure handling, which is optimistically triggered even under absence of failures. To disable this option set the JSON value `"no_fallback" : "true"`. We note, that none of the baseline systems (implement and) run with failure handling.\
         
   3. **TxHotstuff:** 
   > :warning: Make sure to run on branch `TxHotstuff`. Build the binaries before running (see instructions above)
   
   Use the configurations under `/experiment-configs/1-Workloads/3.TxHotstuff`. Before running these configs, you must configure Hotstuff using the instructions from section "1) Pre-configurations for Hotstuff and BFTSmart" (see above). Use a batch size of 4 when running TPCC, and 16 for Smallbank and Retwis for optimal results. Note, that you must re-run `src/scripts/remote_config.sh` **after** updating the batch size and **before** starting an experiment. 
   
     Reported peak results were roughly:
      - TPCC: Throughput: ~920 tx/s, Latency: ~73 ms
      - Smallbank: Throughput: ~6.4k tx/s Latency: ~42 ms
      - Retwis: Throughput: ~5.2k tx/s, Latency: ~48 ms
      
   > :warning: **[WARNING]**: Hotstuffs performance is quite volatile with respect to total number of clients and the batch size specified. Since the Hotstuff protocol uses a pipelined consensus mechanism, it requires at least `batch_size x 4` active client requests per shard at any given time for progress. Using too few clients, and too large of a batch size will get Hotstuff stuck. In turn, using too many total clients will result in contention that is too high, causing exponential backoffs which leads to few active clients, hence slowing down the remaining active clients. These slow downs in turn lead to more contention and aborts, resulting in no throughput. The configs provided by us roughly capture the window of balance that allows for peak throughput. \  
      
   4. **TxBFTSmart**: 
   > :warning: Make sure to run on branch `TxBFTSmart`. Build the binaries before running (see instructions above)
   
   Use the configurations under `/experiment-configs/1-Workloads/4.TxBFTSmart`. Before running these configs, you must configure Hotstuff using the instructions from section "1) Pre-configurations for Hotstuff and BFTSmart" (see above). You can, but do not need to manually set the batch size for BFTSmart (see optional instruction below). Note, that you must re-run `src/scripts/one_step_config.sh` **after** updating the batch size and **before** starting an experiment. 
      
      Reported peak results were roughly:
      - TPCC: Throughput: ~1.3k tx/s, Latency: ~60 ms
      - Smallbank: Throughput: ~8.7k tx/s Latency: ~19 ms
      - Retwis: Throughput: ~6.3k tx/s, Latency: ~23 ms

   > **[OPTIONAL NOTE]** **If you read, read fully**: To change batch size in BFTSmart navigate to  `src/store/bftsmartstore/library/java-config/system.config` and change line `system.totalordermulticast.maxbatchsize = <batch_size>`. Use 16 for TPCC and 64 for Smallbank/Retwis for optimal results. However, explicitly setting this batch size is not necessary, as long as the currently configured `<batch_size>` is `>=` the desired one. This is because BFTSmart performs optimally with a batch timeout of 0, and hence the batch size set *only* dictates an upper bound for consensus batches. Using a larger batch size has no effect. Hence, our reported optimal batch sizes of 16 and 64 respectively correspond to the upper bound after which no further improvements are seen. By default our configurations are set to `<batch_size> = 64`, so no further edits are necessary. \
   > **[Troubleshooting]**: If you run into any issues (specifically the error: “SSLHandShakeException: No Appropriate Protocol” ) with running BFT-Smart please comment out the following in your `java-11-openjdk-amd64/conf/security/java.security` file: `jdk.tls.disabledAlgorithms=SSLv3, TLSv1, RC4, DES, MD5withRSA, DH keySize < 1024 EC keySize < 224, 3DES_EDE_CBC, anon, NULL`


#### **2-Client Failures**:
   We evaluated 4 types of client failures: 1) forced simulated equivocation (equiv-forced), 2) realistic equivocation (equiv-real), 3) early client stalling (stall-early), and 4) late client stalling (stall-late). We evaluated all failures across 2 simple YCSB workloads: a) a uniform workload (RW-U), and b) a heavily skewed/contended workload (RW-Z). We remark once again that "equiv-forced" is **not** the realistic worst-case, as it simulates an artificial execution; Instead, it is "stall-early" that corresponds to the worst-case statistics claimed by us.
   
   The metric used is Throughput/Correct-Clients: It can be found under `stats.json` -> `run_stats` -> `combined` -> `tput_s_honest` (see subsection 3) Parsing outputs) /
   
   To reproduce the full lines/slope in the paper you need to re-run several configs per failure type, per workload. To only validate our claims, it may instead suffice to re-run data points near the maximum evaluated fault threshold (e.g. config Indicus-90-2). Configs are named after the total number of faulty clients, and the frequency at which they issue failures: E.g. Indicus-90-2.json (in a given failure type folder) simulates 90% of clients injecting a failure every 2nd transaction. To read the total percentage of faulty transaction (relative to total transactions injected) search for `"tx_attempted_failure_percentage"`.
   
   > ⚠️ **[WARNING]** Do NOT change the client settings in any of the configurations. To facilitate comparisons between different failure levels all must use the same number of clients. The client numbers used correspond to those reported in the paper.
   > **[NOTE]** Experiment stats are only collected on correct clients. Due to the high number of failure fractions evaluated, only few clients are used to collect stats which can lead to higher variance in the results; Hence all our results reported in the paper are averages over 4 runs and include standard deviation. You *may* want to do the same, but it is not necessary. We recommend using Indicus-90-2.json instead of 98-2.json for higher stability (if you only run one config).


   1. **Evaluating RW-U**: Use configs from `/experiment-configs/2-Client-Failures/RW-U
      1. No Failures baseline:
         - First, run the baseline `Indicus-NoFailures.json` to establish the throughput per correct client when no failures are occuring. 
         - Reported Tput/CorrectClients: ~107
      2. equiv-forced:
         - Navigate to folder `EquivForced` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 40% fautly/total tx. Tput/Corectclients: ~76
         
      3. equiv-real
         - Navigate to folder `EquivReal` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 45% faulty/total tx. Tput/Corectclients: ~107
         
      4. stall-early
         - Navigate to folder `StallEarly` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 46% faulty/total tx. Tput/CorrectClients: ~90
         
      5. stall-late
         - Navigate to folder `StallLate` and run a config file.
         - Reported Tput for Indicus-90-2: corresponds to 45% of faulty/total tx. Tput/CorrectClient: ~96
         
      All reported data points. Data format: [% faulty/total, Tput/CorrectClients]
      
            |  Failure Type | Indicus-10-3 | Indicus-15-2 | Indicus-30-3 | Indicus-60-3 | Indicus-60-2 | Indicus-80-2 | Indicus-90-2 | Indicus-98-2
            |---------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|-------------
            | equiv-forced  |  [3%, 104]   |  [7%, 100]   |  [10%, 97]   |  [19%, 39]   |  [28%, 83]   |  [36%, 79]   |  [40%, 76]   |  [43%, 74]
            | equiv-real    |      -       |       -      |  [10%, 107]  |  [20%, 106]  |  [30%, 106]  |  [40%, 107]  |  [45%, 107]  |  [49%, 107]
            | stall-early   |      -       |       -      |  [11%, 101]  |  [21%, 101]  |  [32%, 97]   |  [41%, 93]   |  [46%, 90]   |  [49%, 90]
            | stall-late    |      -       |       -      |  [10%, 106]  |  [20%, 104]  |  [30%, 102]  |  [40%, 100]  |  [45%, 90]   |  [49%, 95]
      
   2. **Evaluating RW-Z**: Use configs from `/experiment-configs/2-Client-Failures/RW-U
      1. No Failures baseline:
         - First, run the baseline `Indicus-NoFailures.json` to establish the throughput per correct client when no failures are occuring. 
         - Reported Tput/CorrectClients: ~67
      2. equiv-forced:
         - Navigate to folder `EquivForced` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 27% fautly/total tx. Tput/Corectclients: ~24
         
      3. equiv-real
         - Navigate to folder `EquivReal` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 36% faulty/total tx. Tput/Corectclients: ~66
         
      4. stall-early
         - Navigate to folder `StallEarly` and run a config file.
         - Reported Tput for Indicus-90-2: Corresponds to 40% faulty/total tx. Tput/CorrectClients: ~46
         
      5. stall-late
         - Navigate to folder `StallLate` and run a config file.
         - Reported Tput for Indicus-90-2: corresponds to 40% of faulty/total tx. Tput/CorrectClient: ~56
         
      All reported data points. Data format: [faulty transactions / total transactions as %, Tput/CorrectClients as tx/s/#correct_clients]
      
            |  Failure Type | Indicus-10-3 | Indicus-15-2 | Indicus-30-3 | Indicus-60-3 | Indicus-60-2 | Indicus-80-2 | Indicus-90-2 | Indicus-98-2
            |---------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|-------------
            | equiv-forced  |   [2%, 53]   |   [5%, 48]   |   [7%, 43]   |  [13%, 35]   |  [19%, 30]   |  [24%, 26]   |  [27%, 24]   |  [29%, 22]
            | equiv-real    |      -       |       -      |   [8%, 67]   |  [16%, 67]   |  [24%, 67]   |  [32%, 66]   |  [36%, 66]   |  [39%, 67]
            | stall-early   |      -       |       -      |   [10%, 65]  |  [18%, 60]   |  [29%, 55]   |  [36%, 50]   |  [40%, 46]   |  [42%, 44]
            | stall-late    |      -       |       -      |   [10%, 65]  |  [18%, 62]   |  [25%, 61]   |  [36%, 59]   |  [40%, 56]   |  [42%, 54]
     
 > **[NOTE]** Why does %faulty/total end at different points?/
     The explanation is not unavailability, or a failure to run the experiment: rather, it is an artefact of how we count transactions. In particular, (faulty_clients x failure_frequency) % (e.g. 45% for Indicus-90-2) of the transactions newly submitted to Basil are faulty. However, contention (and dependencies on equivocating transactions) can require some of the correct transactions to abort and re-execute (faulty transactions instead do not care to retry), which decreases the percentage of faulty transactions that Basil processes (since, again, some correct transactions end up being prepared multiple times). Thus, when measuring the throughout, the percentage of faulty transactions we report is the fraction of faulty transactions among all processed (as opposed to admitted) transactions--the latter is set at (faulty_clients x failure_frequency) %, while the former depends on the number of re-executions of correct transactions.
 

#### **Microbenchmarks**:
Finally, we review the reported Microbenchmarks.

> :warning: Make sure to run on branch `Basil/Tapir`. Build the binaries before running (see instructions above)

#### **3-Crypto Overheads**:
To reproduce the reported evaluation of the impact of proofs and cryptography on the system navigate to `experiment-configs/3-Micro:Crypto`. The evaluation covers the RW-U workload as well as RW-Z, and for each includes a config with Crypto/Proofs disabled and enabled respectively. Since signatures induce a high amount of overhead the full Basil system is multithreaded and uses several worker threads to handle cryptography - Since this overhead falls to the wayside, the Non-Crypto/Proofs version instead runs single-threaded (no crypto worker threads) and instead uses the available cores to run more shards. In total, the Non-Crypto/Proofs version uses 24 shards, vs normal Basil using 3.

> **[NOTE]** Since running this microbenchmark the codebase has changed significantly to include client failure handling. The No-Crypto/Proofs option is no longer supported on the full version, and hence must run with the fallback protocol disabled (since failures are not simulated it will not regularly be triggered anyways, but it may be occasionally, leading to segmentation faults). The provided config has the fallback protocol disabled by default `"no_fallback": true"` - Make sure to not change this. We remark that the No-crypto/Proofs version is **not** a safe BFT implementation, it is purely an option for microbenchmarking overheads. The throughput for the No-Crypto/Proofs version has changed ever so slightly given the updates.

1. **RW-U**
   - Navigate to the `RW-U` folder and run `Indicus.json` and `Indicus-NoCrypto.json` respectively.
   - The reported results are ~38k tput for Crypto enabled (Indicus.json) and ~143k for Crypto/Proofs disabled.
2. **RW-Z**
   - Navigate to the `RW-Z` folder and run `Indicus.json` and `Indicus-NoCrypto.json` respectively.
   - The reported results are ~4.8k tput for Crypto enabled (Indicus.json) and ~22k for Crypto/Proofs disabled.

#### **4-Reads**:
To reproduce the reported evaluation of the impact of different Read Quorum sizes on the system navigate to `experiment-configs/4-Micro:Reads`. The evaluation uses a read only workload and compares Read Quorums consisting of 1) a single read, 2) f+1 reads from different replicas, and 3) 2f+1 reads from different replicas. All configurations use an "eager-reads" optimization (which is used by all baseline systems too) in which read messages are optimistically only sent to the Read Quorum itself (instead of pessimistically sending to f additional replicas).

> ⚠️**[Warning]** Do **not** run the single read configuration on a non-read-only workload (i.e. a workload with writes as well) as the prototype is hard coded to only read uncommitted values from f+1 replicas (which is necessary for Byzantine Independence). Running with a single read is **not** BFT tolerant and is purely an option for microbenchmarking purposes.

The provided configs only run an experiment for the rough peak points reported in the paper which is sufficient to compare the overheads of larger Quorums. If you want to reproduce the full figure reported, you may run `combined.json`, however we advise against it, since it takes a *considerable* amount of time. You may instead run each configuration for a few neighboring client configurations (already included as comments in the configs). 

1. **Single read*
   - Run configuration `1.json`.
   - The reported peak throughput is ~17k tx/s.
2. **f+1 reads**
   - Run configuration `f+1.json`.
   - The reported peak throughput is ~13.5k tx/s
3. **2f+1 reads**
   - Run configuration `2f+1.json`.
   - The reported peak throughput is ~17k tx/s
  
#### **5-Sharding**:
To reproduce the reported evaluation of the impact of proofs and cryptography on sharding navigate to `experiment-configs/5-Micro:Sharding`. The evaluation covers the RW-U workload and includes configurations for different number of shards with Crypto/Proofs disabled and enabled respectively. Since the Non-Crypto/Proofs version is single threaded (see subsection 3-Crypto above) we run 8 times more shards than in the full Basil system, since the latter uses 8 threads, over 8 cores (since m510 machines have 8 cores).

> **[NOTE]** Like mentioned under **3-Crypto** above, th No-Crypto/Proofs option is no longer supported on the full Basil prototype  and hence must run with the fallback protocol disabled The provided config has the fallback protocol disabled by default `"no_fallback": true"` - Make sure to not change this. 

1. **Crypto/Proofs enabled** (normal Basil): 
   - Navigate to folder `/Crypto`.
   1. Scale Factor 1 (1 shard): 
      - Run config `1-Indicus-RW-U.json`
      - Reported throughput: ~20k
   2. Scale Factor 2 (2 shards): 
      - Run config `2-Indicus-RW-U.json`
      - Reported throughput: ~23k
   3. Scale Factor 3 (3 shards): 
      - Run config `3-Indicus-RW-U.json`
      - Reported throughput: ~27k

1. **Crypto/Proofs disabled**: 
   - Navigate to folder `/Non-Crypto`.
   1. Scale Factor 1 (8 shards): 
      - Run config `8-RW-U-cryptoOFF.json`
      - Reported throughput: ~45k
   2. Scale Factor 2 (16 shards): 
      - Run config `16-RW-U-cryptoOFF.json`
      - Reported throughput: ~61k
   3. Scale Factor 3 (24 shards): 
      - Run config `24-RW-U-cryptoOFF.json`
      - Reported throughput: ~86k

   > Throughput may be a little better on the current version - which only emphasizes the overhead that Crypto and Quroum Proofs impose.


#### **6-FastPath**:
To reproduce the reported evaluation of the utility of the Fast Path navigate to `experiment-configs/6-Micro:FastPath`. The evaluation covers both the RW-U and RW-Z workload and includes configurations to run the normal Basil prototype, and the Basil prototype with the Fast Path explicitly disabled.

1. **RW-U**
   - Navigate to the `RW-U` folder and run `Indicus_16_FP_ON.json` and `Indicus_16_FP_OFF.json` to run Basil with Fast Path enabled and disabled respectively.
   - The reported results are ~38k tput for Fast Path enabled, and ~32k for Fast Path disabled.
2. **RW-Z**
   - Navigate to the `RW-Z` folder and run `Indicus_4_FP_ON.json` and `Indicus_4_FP_OFF.json` to run Basil with Fast Path enabled and disabled respectively.
   - The reported results are ~4.8k tput for Fast Path enabled and ~2.4k for Fast Path disabled.
   > The RW-Z results for Fast-Path disabled may be slightly higher (which is a good thing) than the reported results since we modified the codebase since.

   > [Note] The evaluation with no Fast Path is a lower bound on the impact of a replica failure during the Prepare phase. While Basil cannot use the Commit Fast Path in presence of a misbhehaving replica, it might still be able to use the Abort Fast Path, which reduces contention by removing tentative transactions faster, and allows clients submitting aborting transactions to retry sooner. This microbenchmark instead fully disables all Fast Paths.
     
#### **7-Batching**:
To reproduce the reported evaluation of different batch sizes in Basil navigate to `experiment-configs/7-Micro:Batching`. The evaluation covers both the RW-U and RW-Z workload and includes configurations to run Basil with several different batch sizes. 

1. **RW-U**
   - Navigate to the `RW-U` folder and run different batch sizes by running `Indicus_<batch_size>.json`.
   - The reported peak results are for batch size 16/31 at ~38k at which point there are no further improvements
2. **RW-Z**
   - Navigate to the `RW-Z` folder and run different batch sizes by running `Indicus_<batch_size>.json`.
   - The reported peak results are for batch size 2/4 at ~4.8k at which point further increasing the batch size is detrimental.
   - The RW-Z results for batch sizes 1 and 8 may be slightly higher than the reported results since we modified the codebase since, but the peak remains the same.
       
All reported data points. 
     
            | Workload | Batch Size: 1 | Batch Size: 2 | Batch Size: 4 | Batch Size: 8 | Batch size: 16 | Batch size: 32 |
            |----------|---------------|---------------|---------------|---------------|----------------|----------------|
            |   RW-U   |   9600 tx/s   |   14400 tx/s  |   21700 tx/s  |   30300 tx/s  |   38200 tx/s   |  38200 tx/s    |
            |   RW-Z   |   3300 tx/s   |    4600 tx/s  |    4800 tx/s  |    2900 tx/s  |        -       |        -       |

 
