# SOSP21 Artifact Evaluation #108
This is the repository for the Artifact Evaluation of SOSP'21 submission #108: "Basil: Breaking up BFT with ACID transactions" 


## Claims

We made 4 claims which can be found in Figure 3 of our submission. We copy-and-paste them here.

- **claim1**: Basil comes within competitive performance (both throughput and latency) compared to Tapir, a Crash Fault Tolerant database. Basils current code-base was modified since the Basil results reported in the paper (for microbenchmarks too) to include failure handling, so while results should be largely consistent, they may differ slightly.

- **claim2**: Basil achieves both higher throughput and lower latency than both BFT baselines (TxHotstuff, TxBFTSmart)

- **claim3**: Basil maintains robust throughput for correct clients under attack by byzantine Clients

- **claim4**: All other microbenchmarks are correct...


# Artifacts
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
(You may try to run on Mac, which has worked for us in the past, but is not documented 
And cannot be easily aided by us. We recommend running on Ubuntu 18.  If you cannot do this locally, we recommend using a CloudLab controller machine - see section "setting up CloudLab")
(To re-install from scratch use a clean image:       urn:publicid:IDN+emulab.net+image+emulab-ops:UBUNTU18-64-STD.
Or use an existing image: tbb, or East for bftsmart - TODO: make public)
- Requires python3 for scripts
- C++ 17 for Main code (gcc version > 5)
- Java 11 for BFT Smart code (install included below)

### General installation pre-reqs
Before beginning the install process, update your distribution:
1. `sudo apt-get update`
2. `sudo apt-get upgrade`
Then, install the following tools:
3. `sudo apt-get install autoconf automake libtool curl make g++ unzip valgrind cmake`


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
# Install java open jdk 1.11.0 in /usr/lib/jvm
First, install Java 11 and export your LD_LIBRARY_Path:
1. `sudo apt-get install openjdk-11-jdk`
2. `export LD_LIBRARY_PATH=/usr/lib/jvm/java-1.11.0-openjdk-amd64/lib/server:$LD_LIBRARY_PATH`
Next, modify all references of "fs435" or "zw494" in the source code and replace them with your cloudlab id  (TODO: please write script that automates this)
After that, compile BFT-SMART (TODO: integrate this into Makefile)
3. `cd /src/store/bftsmartstore/library`
4. `mkdir bin`
5. `ant`
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

