# Was tested and implemented on Linux, partially implemented in Mac.
# ADDITIONAL INSTRUCTION ABOUT DEFIANCE PROJECT CAN BE FOUND HERE: [https://github.com/DEFIANCE-project/ns3-DEFIANCE]

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

6. Install the python dependency of defiance with poetry:
``` 
poetry -C contrib/defiance install --without local
./ns3 configure --enable-python --enable-examples --enable-tests (running should succeed).
./ns3 build ai (compile ns3-ai to generate the message types with protobuf)
poetry -C contrib/defiance install --with local (Install the python packages of ns3-ai)
```
7. Compile everything:
```
./ns3 build
```
