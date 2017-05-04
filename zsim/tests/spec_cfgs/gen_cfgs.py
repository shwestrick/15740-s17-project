#!/usr/bin/python

import libconf
import io
import sys
import appDicts 


def main():
    spec_bench_root = "/usr2/cmcguffe/spec/spec2006/benchspec/"
    spec_config = "amd64-m64-gcc42-nn"
    zsim_config = sys.argv[1]
    input_type = sys.argv[2]
    name_dict = appDicts.SPEC2006NamesDict()
    curr_proc = 0
    curr_arg = 3
    processes = list()
    while curr_arg < len(sys.argv):
        proc = {'bmk_name': name_dict[sys.argv[curr_arg]], 'input_type': input_type}
        processes.append(proc)
        curr_proc += 1
        curr_arg += 1
    add_commands_to_config(zsim_config, spec_bench_root, spec_config, processes)
    

def add_commands_to_config(input_cfg_file, benchspec_root, spec_cfg, processes):
    #load original config file
    f_name_start = input_cfg_file.find("/")
    f_name_start = max(f_name_start, 0)
    ext_start = input_cfg_file.find(".", f_name_start)
    if ext_start == -1:
        output_cfg_file = input_cfg_file
    else:
        output_cfg_file = input_cfg_file[0:ext_start]
    output_cfg_file += "_" + str(len(processes))
    print "output file name initalized as: " + output_cfg_file
     
    with io.open(input_cfg_file) as in_file:
        cfg = libconf.load(in_file)
    
        envs = {'BENCH_DIR': benchspec_root, 'SPEC_CFG': spec_cfg}
    
        num_procs = len(processes)
    
        app_dictionary = appDicts.SPECAppDict(envs)
    
        curr_proc = 0
    
        for process in processes:
            benchmark = process["bmk_name"]
            arg_type = process["input_type"]
    
            curr_app = app_dictionary.get(benchmark)
            if curr_app != None:
                if not (arg_type == "test" or arg_type == "train" or arg_type == "ref"):
                    print "Process " + curr_proc + " had an invalid arg type"
                curr_command = curr_app.getCommand(arg_type)
                curr_args = curr_app.getArguments(arg_type)
                cfg["process" + str(curr_proc)] = {'command': curr_command + " " + curr_args}
    #            cfg["process" + str(curr_proc)][command] = curr_command + " " + curr_args 
                output_cfg_file = output_cfg_file + "_" + str(curr_proc) + "_" + benchmark + arg_type
            else:
                print "Process " + curr_proc + " had an invalid name"
                sys.exit(0)
    
            curr_proc += 1
    
    #    print libconf.dumps(cfg)
        output_cfg_file += ".cfg"
        out_file = io.open(output_cfg_file, 'w')
        libconf.dump(cfg, out_file)

if __name__ == "__main__":
    main(); 


