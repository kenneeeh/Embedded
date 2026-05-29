#!/bin/bash
rm or1420SingleCore.*
yosys -D GECKO5Education -s ../scripts/yosysOr1420.script 
nextpnr-ecp5 --timing-allow-fail --85k --package CABGA381 --json or1420SingleCore.json --lpf ../scripts/gecko5_or1420.lpf --textcfg or1420SingleCore.config
ecppack --compress --freq 62.0 --input or1420SingleCore.config --bit or1420SingleCore.bit
openFPGALoader or1420SingleCore.bit
