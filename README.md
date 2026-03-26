# lte-ns3-rl-thesis

Was tested and implemented on Linux, partially implemented in Mac.
ADDITIONAL INSTRUCTION ABOUT DEFIANCE PROJECT CAN BE FOUND HERE:
https://github.com/DEFIANCE-project/ns3-DEFIANCE

1. Install ns-3.46.1 https://www.nsnam.org/releases/ns-3.46.1.tar.bz2
2. Install AI/Defiance framerworks using the next command:
   
git clone https://github.com/DEFIANCE-project/ns3-ai contrib/ai
git clone https://github.com/DEFIANCE-project/ns3-defiance contrib/defiance

3. Make a clone of this repository into contrib/defiance/examples section:

rm -rf contrib/defiance/examples
git clone https://github.com/Desire32/lte-ns3-rl-thesis.git contrib/defiance/examples/lte-ns3-rl-thesis

4. Create the python venv:
python3.12 -m venv .venv

5. Activate the environment:
source .venv/bin/activate

Export the environment
export NS3_HOME=$(pwd)/ns-3.XX
OPTIONAL: export WANDB_API_KEY"YOUR_KEY", if you wish to track with wandb

Install the python dependency of defiance with poetry: 
poetry -C contrib/defiance install --without local
Running ./ns3 configure --enable-python --enable-examples --enable-tests should succeed.
Then, compile ns3-ai to generate the message types with protobuf: ./ns3 build ai
Install the python packages of ns3-ai with poetry -C contrib/defiance install --with local
Compile everything with ./ns3 build
