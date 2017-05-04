import io
import sys

def main():
    bin_to_run = sys.argv[1]
    log_file = sys.argv[2]
    tgt_list = convert_log_to_dir_list(log_file)
    log_loc = log_file.rfind("log_")
    ext_loc = log_file.rfind(".")
    emu_file_name = "exp_" + log_file[log_loc+4:ext_loc] + ".emu"
    convert_dir_list_to_emulab(bin_to_run, tgt_list, emu_file_name)


def write_line(target, data):
    target.write(data)
    target.write("\n")

def convert_log_to_dir_list(log_file_name):
    log_file = io.open(log_file_name)
    log_transcript = log_file.read()
    lines = log_transcript.split("\n")
    return lines

def convert_dir_list_to_emulab(bin_to_run, dir_list, emulab_f_name):
    em_file = io.open(emulab_f_name, 'wb')
    write_line(em_file, "set ns [new Simulator]")
    write_line(em_file, "source tb_compat.tcl")
    write_line(em_file, "")
    node_num = 1
    for tgt_dir in dir_list:
        node_num_str = str(node_num)
        node_name = "node" + node_num_str
        write_line(em_file, "#" + node_name)
        write_line(em_file, "set " + node_name + " [$ns node]")
        write_line(em_file, node_name + " add-desire rr 1.0")
        write_line(em_file, "tb-set-node-os " + node_name + " UBUNTO14-64-PROBE")
        write_line(em_file, "tb-set-node-startcmd " + node_name + " " + tgt_dir + bin_to_run)
        write_line(em_file, "")
        node_num += 1
    write_line(em_file, "$ns rtproto Static")
    write_line(em_file, "$ns run")

if __name__ == "__main__":
    main()
