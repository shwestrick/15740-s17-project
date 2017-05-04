#!/usr/bin/python

import sys
import io
import appDicts
import os
import libconf
import gen_cfgs
import shutil
import itertools
from datetime import datetime
import stat


def main():
    target_root = sys.argv[1]
    system_name = sys.argv[2]
    binary_file = sys.argv[3]
    zsim_bin_root = sys.argv[4]
    zsim_config_file = sys.argv[5]
    sys_param_file = sys.argv[6]
    test_bmks_file = sys.argv[7]
    spec_root = sys.argv[8]
    spec_config = sys.argv[9]
    spec_input = sys.argv[10]
    with io.open(sys_param_file) as in_file:
        sys_param_list = libconf.load(in_file)
    with io.open(test_bmks_file) as in_file:
        test_bmks_list = libconf.load(in_file)
    total_system_setup(target_root, system_name, binary_file, zsim_bin_root, zsim_config_file, sys_param_list, test_bmks_list, spec_root, spec_config, spec_input)
        

def total_system_setup(tgt_root, sys_name, run_bin, zsim_bin_root, input_zsim_config, sys_param_list, test_bmks_list, bmk_root, bmk_config, bmk_input):

    #parse zsim parameters
    params_to_set = sys_param_list

    #setup directory for the zsim system
    intermediate_dir, intermediate_file = system_directory_setup(tgt_root, sys_name, input_zsim_config, params_to_set)
    #copy the zsim binary files
    if zsim_bin_root[-1] != "/":
        zsim_bin_root += "/"
    shutil.copyfile(zsim_bin_root + "zsim", intermediate_dir + "zsim")

    #setup the spec information
    bmk_envs = {'BENCH_DIR': bmk_root, 'SPEC_CFG': bmk_config}
    bmk_dict = appDicts.SPECAppDict(bmk_envs)

    #parse list of process sets to test
    name_dict = appDicts.SPEC2006NamesDict()
    bmks_to_test = list()
    for bmk_set in test_bmks_list.values():
        if bmk_set["names"] == "all":
            bmk_names = list(set(name_dict.values()))
        else:
            bmk_names = list()
            for bmk in bmk_set["names"]:
                bmk_names.append(name_dict[bmk])
        if bmk_set["type"] == "list":
            bmks_to_test.append(bmk_names)
        elif bmk_set["type"] == "pickset":
            combo_length = bmk_set["num_procs"]
            combos = itertools.combinations(bmk_names, combo_length)
            for combo in combos:
                bmks_to_test.append(combo)

    #setup the benchmark directories
    benchmark_directory_setup(run_bin, intermediate_dir + intermediate_file, bmks_to_test, bmk_input, bmk_dict, True)

def params_lru():
    return {'type': "LRU"}

def params_SRRIP(M):
    return {'type': "SRRIP", 'M': M}

def params_ASRRIP(M, r_start, w_start, c_prom, d_prom):
    return {'type': "ASRRIP", 'M': M, 'r_start': r_start, 'w_start': w_start, 'c_prom': c_prom, 'd_prom': d_prom}

def system_directory_setup(direct_root, sys_name, input_config_file, params_to_set):
    if direct_root[-1] != "/":
        direc_root += "/"
    output_file_name = sys_name
    output_dir_name = direct_root + sys_name + "/"
    with io.open(input_config_file) as input_file:
        config = libconf.load(input_file)
        for param, value in params_to_set.iteritems():
            if param == "l2_repl":
                config.sys.caches.l2.repl = value
                key_str = "l2repl"
                value_str = value["type"]
                for repl_par, repl_val in value.iteritems():
                    if repl_par != "type":
                        value_str += "," + repl_par + repl_val
            elif param == "l3_repl":
                config.sys.caches.l3.repl = value
                key_str = "l3repl"
                value_str = value["type"]
                for repl_par, repl_val in value.iteritems():
                    if repl_par != "type":
                        value_str += "," + repl_par + repl_val
#TODO implement memory and other parameters
            else:
                print "Unknown parameter: " + param
                key_str = "unkown"
            output_file_name += "-" + key_str + "_" + value_str
            output_dir_name += key_str + "_" + value_str + "/"
            if not os.path.exists(output_dir_name):
                os.makedirs(output_dir_name)
        output_file_name += ".cfg"
        output_path = output_dir_name + output_file_name
        out_file = io.open(output_path, 'w')
        libconf.dump(config, out_file)
        return [output_dir_name, output_file_name]

def benchmark_directory_setup(exec_file, zsim_config_file, target_bmks, bmk_input, bmk_dict, log):
    #find the name of the exec_file
    file_start = exec_file.rfind("/")
    exec_file_short = exec_file[file_start+1:]
    #setup the root directory as the directory containing the input file
    file_start = zsim_config_file.rfind("/")
    if file_start == -1:
        target_dir = "./"
    else:
        target_dir = zsim_config_file[0:(file_start + 1)]
    #create timestamp mark to separate similar runs
    timestamp = str(datetime.now())
    timestamp = timestamp.replace(" ", "_")
    timestamp = timestamp.replace(":", "-")
    #create log file if requested
    if log:
        log_file_name = "log_" + timestamp + ".txt"
        log_file = io.open(log_file_name, 'w')
    #format the benchmarks by number of processes
    formatted_bmks = list()
    for bmk_set in target_bmks:
        num_procs = len(bmk_set)
        while num_procs >= len(formatted_bmks):
            formatted_bmks.append(list())
        formatted_bmks[num_procs].append(bmk_set)
    #process the reformatted benchmark list
    num_procs = 0
    while num_procs < len(formatted_bmks):
        curr_bmk_superset = formatted_bmks[num_procs]
        proc_dir_name = target_dir + str(num_procs) + "procs"
        if len(curr_bmk_superset) > 0:
            #make a dir for this number of procs
            if not os.path.exists(proc_dir_name):
                os.makedirs(proc_dir_name)
            for bmk_set in curr_bmk_superset:
                #generate the directory for this process set
                exp_dir_name = proc_dir_name + "/"
                proc_list = list()
                for bmk in bmk_set:
                    exp_dir_name += bmk + "_"
                    proc = {'bmk_name': bmk, 'input_type': bmk_input}
                    proc_list.append(proc)
#                exp_dir_name[-1] = "/"
                exp_dir_name = exp_dir_name[0:-1] + "/"
                if not os.path.exists(exp_dir_name):
                    os.makedirs(exp_dir_name)
                exp_dir_name += timestamp + "/"
                if not os.path.exists(exp_dir_name):
                    os.makedirs(exp_dir_name)
                if log:
                    log_file.write(exp_dir_name + "\n")
                #generate the new config file for this process set and copy run command
                gen_cfgs.add_commands_to_config(zsim_config_file, exp_dir_name, proc_list, bmk_dict)
                shutil.copyfile(exec_file, exp_dir_name + exec_file_short)
                os.chmod(exp_dir_name + exec_file_short, 0o755)
                #generate symbolic links to run code 
                proc_num = 0
                for bmk in bmk_set:
                    try:
                        bmk_run_dir = bmk_dict[bmk].getRunDir(bmk_input)
                        proc_dir = exp_dir_name + "P" + str(proc_num)
                        os.symlink(bmk_run_dir, proc_dir)
                    except:
                        print "Unable to make symlink at " + proc_dir
                    proc_num += 1
        num_procs += 1

if __name__ == "__main__":
    main()
