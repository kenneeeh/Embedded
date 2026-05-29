#### Structure:

The virtual prototype consists of three directories:

- modules: This directory contains the several modules that are contained in the SOC. Add your own modules in this directory. Most modules also contain a ```doc``` directory with documentation.
- programms: This directory contains the "hello world" template that can be used as basis for your own programms.
- systems: This directory contains all the required files for the "top level" of the SOC.

#### Hardware configuration files:

To be able to build the Virtual Prototype hardware, there are several files:

- systems/singleCore/scripts/gecko5_or1420.lpf: This file contains the pin-mapping of the top level to the FPGA-pins. 
- systems/singleCore/scripts/yosysOr1420.script: This file contains all the verilog files required to build your system, note that "read -sv " needs to be in each line
- systems/singleCore/scripts/synthesizeOr1420.sh: This is a script that will perform the synthesys, P&R and will upload the bit-file to your GECKO5.

#### Building the hardware:

This step is done with the OSS-cad-suite (Yosys, nextpnr, ...).
It is completely automated, just do (in a terminal):
cd systems/singleCore/sandbox
../scripts/synthesizeOr1420.sh

If you want to permenantly store the bit-file in Flash (for example for a demo), you can use (in the sandbox directory):
openFPGALoader -f or1420SingleCore.bit

#### Building the software:

The software is based on a makefile system. To build a program follow following steps (with as example the hello world program):

- Goto the directory ```programs/helloWorld```
- Execute ```make clean mem1420```
- If no error occurred, you will find in the directory ```programms/helloWorld/build-release/``` the files ```hello.elf```, ```hello.cmem```, and ```hello.mem```. The file that you need to upload to your board is the ```hello.cmem```-file.
- Upload the ```hello.cmem```-file with your favorite terminal program to your virtual prototype.

IMPORTANT: As the or1420 does not contain a hardware-divide unit you have to compile your programm with the compile option ```-msoft-div```!
