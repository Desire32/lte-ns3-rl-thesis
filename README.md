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

# Architecture

## Acknowledgements
Expressing my sincere gratitude to my mentor, Christoforos Christoforou, for the guidance and materials provided throughout this work, and to the Huawei team for sharing their vision and insights on 6G technologies. 

Special thanks are extended to Ian Goodfellow, whose educational materials have been an invaluable resource in my learning journey. 
