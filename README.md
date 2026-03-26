ADDITIONAL INSTRUCTION ABOUT DEFIANCE PROJECT AND AI MODULE CAN BE FOUND HERE: 

[https://github.com/DEFIANCE-project/ns3-DEFIANCE] 

[https://github.com/DEFIANCE-project/ns3-ai/blob/c18b8a3ea67978ea1ebb2fe0263b10d25f220ee8/docs/install.md]


# LTE NS-3 RL Thesis

Reinforcement learning agent for LTE downlink power control using ns-3 and Ray RLlib.

## How it works

The simulation runs in ns-3 (C++) and communicates with a Python RL agent via the ns3-ai interface.
On each step the agent receives SINR values for every (BS, UE) pair and decides the TX power for each base station.
```
Python RL Agent (Ray/RLlib)
        ↕  ns3-ai socket
ns-3 LTE Simulation (C++)
  └── PowerControlMobComEnv
        ├── Observation: SINR matrix [nBS × nUE]
        ├── Action:      TX power per BS [0..46 dBm]
        └── Reward:      coverage + efficiency
```

## Requirements

- Ubuntu 22.04
- Python 3.12
- ns-3.46.1 with contrib/defiance and contrib/ai


1. Install ns-3.46.1 https://www.nsnam.org/releases/ns-3.46.1.tar.bz2
2. Locate into ns-3.4.1/ and Install AI/Defiance frameworks using the next command:
   
```   
git clone https://github.com/DEFIANCE-project/ns3-ai contrib/ai
git clone https://github.com/DEFIANCE-project/ns3-defiance contrib/defiance
```

3. Make a clone of this repository into contrib/defiance/examples section:

```
rm -rf contrib/defiance/examples
git clone https://github.com/Desire32/lte-ns3-rl-thesis.git contrib/defiance/examples/lte-ns3-rl-thesis
```

3.1 Replace old ray.py with the new one:
```
cp contrib/defiance/examples/lte-ns3-rl-thesis/ray.py contrib/defiance/model/agents/single/ray.py
```

4. Create the python venv and activate it:
```
python3.12 -m venv .venv
source .venv/bin/activate
```

5. Export the environment
```
IMPORTANT: export NS3_HOME=$(pwd)/ns-3.XX, without NS3_HOME path won't be found and code won't work
OPTIONAL: export WANDB_API_KEY"YOUR_KEY", if you wish to track with wandb
```

6. Install the python dependency of defiance with pip:
``` 
pip install torch --index-url https://download.pytorch.org/whl/cpu
pip install -r requirements.txt

cd contrib/ai/model/gym-interface/py
pip install .
```
7. Compile everything:
```
./ns3 build
```

8. Usage:
In case these don't work, write commands manually in the console
```bash
cd ssh/
chmod +x *.sh

./test.sh    # run sanity check first
./main.sh    # start training
./wandb.sh   # start training with wandb logging
```
