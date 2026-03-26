#!/bin/bash
run-agent random --single -n defiance-lte-ns3-rl-thesis --iterations 2 2>&1 | grep -v "ninja\|Re-checking\|globbed\|no work"
