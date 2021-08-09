# SOSP21 Artifact Evaluation #108
This is the repository for the Artifact Evaluation of SOSP'21 submission #108: "Basil: Breaking up BFT with ACID transactions" 


## Claims

We made 4 claims which can be found in Figure 3 of our submission. We copy-and-paste them here.

- **claim1**: Basil comes within competitive performance (both throughput and latency) compared to Tapir, a Crash Fault Tolerant database.

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
1. Installing Prerequisites (for all branches)  
     --> Point to installation guide

2. Setting up experiments on Cloudlab 
     --> Point to Cloudlab guide: a) how to start a profile and what images to use, b) how to generate these images (all installs + extras)
     
3. Running experiments:
      1) Go to respective branch, 
      2) follow additional installs & remote configs for Hotstuff and BFTsmart
      3) build
      4) experiments.
           --> Show how to run basic dummy test locally, just to confirm that binaries work.
           --> Point to experiment scripts and configs guide. to run remotely.

