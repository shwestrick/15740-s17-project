
import libconf
import io
import sys
import appDicts 


def main():
    spec_bench_root = "/usr2/cmcguffe/spec/spec2006/benchspec/"
    spec_config = "amd64-m64-gcc42-nn"
    zsim_config = sys.argv[1]
    output_direct = "./"
    input_type = sys.argv[2]
    name_dict = appDicts.SPEC2006NamesDict()
    curr_proc = 0
    curr_arg = 3
    #setup the process list
    processes = list()
    while curr_arg < len(sys.argv):
        proc = {'bmk_name': name_dict[sys.argv[curr_arg]], 'input_type': input_type}
        processes.append(proc)
        curr_proc += 1
        curr_arg += 1
    envs = {'BENCH_DIR': spec_bench_root, 'SPEC_CFG': spec_config}
    app_dict = appDicts.SPECAppDict(envs)
    #call the function to generate the new config file
    add_commands_to_config(zsim_config, output_direct, processes, app_dict)
    

def add_commands_to_config(input_cfg_file, output_directory, processes, app_dictionary):
    #generate inital output file name; this will be updated with the associated processes
    #reduce the file path to the file name
    f_name_start = input_cfg_file.rfind("/")
    f_name_start = max(f_name_start, 0)
    #remove the file extension
    ext_start = input_cfg_file.rfind(".", f_name_start)
    if ext_start == -1:
        output_cfg_file = input_cfg_file[f_name_start:]
    else:
        output_cfg_file = input_cfg_file[f_name_start:ext_start]
    output_cfg_file += "_" + str(len(processes))
#    print "output file name initalized as: " + output_cfg_file
     
    #load original config file
    with io.open(input_cfg_file) as in_file:
        cfg = libconf.load(in_file)

        if not cfg.has_key("sim"):
            cfg.setdefault("sim", dict())
        cfg.sim.update({'perProcessDir': True, 'maxTotalInstrs': 2000000000})
    
#        envs = {'BENCH_DIR': benchspec_root, 'SPEC_CFG': spec_cfg}
    
        num_procs = len(processes)
    
#        app_dictionary = appDicts.SPECAppDict(envs)
    
        curr_proc = 0
    
        for process in processes:
            benchmark = process["bmk_name"]
            arg_type = process["input_type"]
    
            curr_app = app_dictionary.get(benchmark)
            if curr_app != None:
                if not (arg_type == "test" or arg_type == "train" or arg_type == "ref"):
                    print "Process " + curr_proc + " had an invalid arg type: " + arg_type
                curr_command = curr_app.getCommand(arg_type)
                curr_args = curr_app.getArguments(arg_type)
                command_dict = {'command': curr_command + " " + curr_args, 'ffiPoints': [1000000000, 10000000000], 'startFastForwarded': True}
                cfg["process" + str(curr_proc)] = command_dict #update file name with process info
                output_cfg_file = output_cfg_file + "_" + str(curr_proc) + "-" + benchmark + "-" + arg_type
            else:
                print "Process " + str(curr_proc) + " had an invalid name: " + benchmark
                return "error"
    
            curr_proc += 1
    
    #    print libconf.dumps(cfg)
        #generate target path using output directory and file name
        output_cfg_file += ".cfg"
        if output_directory[-1] != "/":
            output_directory += "/"
#        print "Creating output_path"
#        print "output_directory: " + str(output_directory)
#        print "output_cfg_file: " + str(output_cfg_file)
        output_path = output_directory + output_cfg_file
        out_file = io.open(output_path, 'w')
        libconf.dump(cfg, out_file)
        return output_path

if __name__ == "__main__":
    main(); 


