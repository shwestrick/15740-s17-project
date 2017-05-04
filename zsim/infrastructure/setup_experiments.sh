#!/bin/bash

target_dir="/usr2/cmcguffe/test_experiment/"
system_name="first_system_symmetric"
run_file="/usr2/cmcguffe/zsim/infrastructure/execute_experiment.sh"
zsim_bin_root="/usr2/cmcguffe/zsim/build/opt/"
zsim_config_file="/usr2/cmcguffe/zsim/tests/spec_cfgs/sys1_l2lru_l3SRRIP.cfg"
sys_param_list_file="/usr2/cmcguffe/zsim/infrastructure/test_zsim_file.cfg"
test_bmks_file="/usr2/cmcguffe/zsim/infrastructure/test_bmks_file.cfg"
spec_root="/usr2/cmcguffe/spec/spec2006/benchspec/"
spec_config="amd64-m64-gcc42-nn"
spec_input="ref"

python ./setup_experiments.py $target_dir $system_name $run_file $zsim_bin_root $zsim_config_file $sys_param_list_file $test_bmks_file $spec_root $spec_config $spec_input
