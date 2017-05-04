#!/usr/bin/python

import libconf
import io
import sys

input_cfg_file = sys.argv[1]
spec_root = sys.argv[2]
num_procs = sys.argv[3]

curr_proc = 0
curr_arg = 4
while curr_proc < num_procs:
    benchmark = sys.argv[curr_arg]
    args = sys.argv[curr_arg+1]
    curr_arg += 2

    #FIXME: perlbench has severla runs per set; only doing the first one
    if benchmark in "400.perlbench":
        name = "400.perlbench"
        test_args = "-I. -I./lib attrs.pl"
        train_args = "-I./lib diffmail.pl 2 550 15 24 23 100"
        ref_args = "-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1"


    elif benchmark in "401.bzip2":
        name = "401.bzip2"
        test_args = "dryer.jpg 2"
        train_args = "byoudoin.jpg 5"
        ref_args = "input.source 64" #let's be a little easier on footprint #"input.source 280") #many more cases

    elif benchmark in "403.gcc":
        name = "403.gcc"
        test_args = "-C cccp.i -o out.o"
        train_args = "-C integrate.i -o out.o"
        ref_args = "-C scilab.i -o out.o" #FIXME: gcc ref's run is actually a sequence of runs, each with a different input file. This is just the largest file.

    elif benchmark in "410.bwaves":
        name = "410.bwaves"
        test_args = ""
        train_args = ""
        ref_args = ""
        

    elif benchmark in self["416.gamess"] = SPECCPU2006Application(envs,
        name = "416.gamess",
        test_args = "<exam29.config",
        train_args = "<h2ocu2+.energy.config",
        ref_args = "<cytosine.2.config") #FIXME: As in gcc, only one of 3 possible runs

    elif benchmark in self["429.mcf"] = SPECCPU2006Application(envs,
        name = "429.mcf",
        test_args = "inp.in",
        train_args = "inp.in",
        ref_args = "inp.in")

    elif benchmark in self["433.milc"] = SPECCPU2006Application(envs,
        name = "433.milc",
        test_args = "<su3imp.in",
        train_args = "<su3imp.in",
        ref_args = "<su3imp.in")

    elif benchmark in self["434.zeusmp"] = SPECCPU2006Application(envs, "434.zeusmp")

    elif benchmark in self["435.gromacs"] = SPECCPU2006Application(envs,
        name = "435.gromacs",
        test_args = "-silent -deffnm gromacs.tpr -nice 0",
        train_args = "-silent -deffnm gromacs.tpr -nice 0",
        ref_args = "-silent -deffnm gromacs.tpr -nice 0")

    elif benchmark in self["436.cactusADM"] = SPECCPU2006Application(envs,
        name = "436.cactusADM",
        test_args = "benchADM.par",
        train_args = "benchADM.par",
        ref_args = "benchADM.par")

    elif benchmark in self["437.leslie3d"] = SPECCPU2006Application(envs,
        name = "437.leslie3d",
        test_args = "<leslie3d.in",
        train_args = "<leslie3d.in",
        ref_args = "<leslie3d.in")

    elif benchmark in self["444.namd"] = SPECCPU2006Application(envs,
        name = "444.namd",
        test_args = "--input namd.input --iterations 1 --output namd.out",
        train_args = "--input namd.input --iterations 1 --output namd.out", #NOTE: train == test
        ref_args = "--input namd.input --iterations 38 --output namd.out")

    #FIXME: There are a few more inputs for each of these
    elif benchmark in self["445.gobmk"] = SPECCPU2006Application(envs,
        name = "445.gobmk",
        test_args = "--quiet --mode gtp <capture.tst",
        train_args = "--quiet --mode gtp <arb.tst",
        ref_args = "--quiet --mode gtp <13x13.tst")

    elif benchmark in self["447.dealII"] = SPECCPU2006Application(envs,
        name = "447.dealII",
        test_args = "8",
        train_args = "10",
        ref_args = "23")

    #FIXME: Missing A LOT of stuff; essentially, each run is 2 invocations and the secons seems to depend on the first. Only doing the first here.
    # Actually, this is the second one. If the run fails, do the first one (pds*.mps) instead
    elif benchmark in self["450.soplex"] = SPECCPU2006Application(envs,
        name = "450.soplex",
        test_args = "-m10000 test.mps",
        train_args = "-m1200 train.mps",
        ref_args = "-m3500 ref.mps")

    elif benchmark in self["453.povray"] = SPECCPU2006Application(envs,
        name = "453.povray",
        test_args = "SPEC-benchmark-test.ini",
        train_args = "SPEC-benchmark-train.ini",
        ref_args = "SPEC-benchmark-ref.ini")

    elif benchmark in self["454.calculix"] = SPECCPU2006Application(envs,
        name = "454.calculix",
        test_args = "-i beampic",
        train_args = "-i stairs",
        ref_args = "-i hyperviscoplastic")

    elif benchmark in self["456.hmmer"] = SPECCPU2006Application(envs,
        name = "456.hmmer",
        test_args = "--fixed 0 --mean 325 --num 45000 --sd 200 --seed 0 bombesin.hmm",
        train_args = "--fixed 0 --mean 425 --num 85000 --sd 300 --seed 0 leng100.hmm",
        ref_args = "nph3.hmm swiss41") #some more ref ones

    elif benchmark in self["458.sjeng"] = SPECCPU2006Application(envs,
        name = "458.sjeng",
        test_args = "test.txt",
        train_args = "train.txt",
        ref_args = "ref.txt")

    elif benchmark in self["459.GemsFDTD"] = SPECCPU2006Application(envs, "459.GemsFDTD")

    elif benchmark in self["462.libquantum"] = SPECCPU2006Application(envs,
        name = "462.libquantum",
        test_args = "33 5",
        train_args = "143 25",
        ref_args = "1397 8")

    elif benchmark in self["464.h264ref"] = SPECCPU2006Application(envs,
        name = "464.h264ref",
        test_args = "-d foreman_test_encoder_baseline.cfg",
        train_args = "-d foreman_train_encoder_baseline.cfg",
        ref_args = "-d foreman_ref_encoder_baseline.cfg")

    elif benchmark in self["465.tonto"] = SPECCPU2006Application(envs, "465.tonto")

    elif benchmark in self["470.lbm"] = SPECCPU2006Application(envs,
        name = "470.lbm",
        test_args = "20 reference.dat 0 1 100_100_130_cf_a.of",
        train_args = "300 reference.dat 0 1 100_100_130_cf_b.of",
        ref_args = "3000 reference.dat 0 0 100_100_130_ldc.of")

    elif benchmark in self["471.omnetpp"] = SPECCPU2006Application(envs,
        name = "471.omnetpp",
        test_args = "omnetpp.ini",
        train_args = "omnetpp.ini",
        ref_args = "omnetpp.ini")

    elif benchmark in self["473.astar"] = SPECCPU2006Application(envs,
        name = "473.astar",
        test_args = "lake.cfg",
        train_args = "BigLakes1024.cfg",
        ref_args = "BigLakes2048.cfg") #ref and train have additional runs

    # NOTE: The all/ input files are endianness and 32/64-bit dependent. Currently using le/64
    elif benchmark in self["481.wrf"] = SPECCPU2006Application(envs, "481.wrf")

    # NOTE: Generating the inputs was a bit involved and this depends on the architecture; please see the script to generate the ctlfiles
    elif benchmark in self["482.sphinx3"] = SPECCPU2006Application(envs,
        name = "482.sphinx3",
        test_args = "ctlfile.le . args.an4",
        train_args = "ctlfile.le . args.an4",
        ref_args = "ctlfile.le . args.an4")

    elif benchmark in self["483.xalancbmk"] = SPECCPU2006Application(envs,
        name = "483.xalancbmk",
        test_args = "-v test.xml xalanc.xsl",
        train_args = "-v allbooks.xml xalanc.xsl",
        ref_args = "-v t5.xml xalanc.xsl")


