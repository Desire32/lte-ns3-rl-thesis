## Requirements

- Ubuntu 22.04
- Python 3.12
- ns-3.46.1 with contrib/defiance and contrib/ai
  
```
Boost C++ libraries
Ubuntu: sudo apt install libboost-all-dev
macOS: brew install boost
Protocol buffers
Ubuntu: sudo apt install libprotobuf-dev protobuf-compiler
macOS: brew install protobuf
pybind11
Ubuntu: sudo apt install pybind11-dev
macOS: brew install pybind11
A Python virtual environment dedicated for ns3-ai (highly recommended)
For example, to use conda to create an environment named ns3ai_env with python version 3.11: conda create -n ns3ai_env python=3.11.
```

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
REMEMBER, ALL THE STEPS STARTING FROM STEP 4 MUST BE DONE BEING IN VIRTUAL ENVIRONMENT (.venv)
5. Export the environment
```
IMPORTANT: export NS3_HOME=$(pwd)/ns-3.XX, without NS3_HOME path won't be found and code won't work
OPTIONAL: export WANDB_API_KEY"YOUR_KEY", if you wish to track with wandb
```

6. Install the python dependency of defiance with pip:
``` 
pip install torch --index-url https://download.pytorch.org/whl/cpu
pip install -r requirements.txt
pip install cppyy

pip install pybind11 protobuf

pip install -e contrib/ai/python_utils
pip install -e contrib/ai/model/gym-interface/py
```
7. Compile everything:
```
./ns3 configure --enable-examples --enable-tests -- -DPython_EXECUTABLE=$(which python) -DPython3_EXECUTABLE=$(which python)
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
