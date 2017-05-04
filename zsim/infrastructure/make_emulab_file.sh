#!/bin/bash

binary_to_run="execute_experiment.sh"
log_to_convert="/usr2/cmcguffe/zsim/infrastructure/log_2017-03-26_22-25-30.496056.txt"

python ./make_emulab_file.py $binary_to_run $log_to_convert
