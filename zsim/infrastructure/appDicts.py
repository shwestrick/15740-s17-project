# Applications and application dictionaries. Specifies how to run applications easily. See example python script to 
# Global environment variables: BENCH_DIR, INPUT_DIR, THREADS for multi-threaded apps 

import os

class Application:
    def __init__(self, envs):
        self.envs = envs
        self.name = "dummyApp"
        self.min_threads = 1
        self.max_threads = 1

    def getCommand(self):
        return "ls"

    def getSetupCommand(self, input=""):
        return ""

    def getTeardownCommand(self, input=""):
        return ""

    def getInputRedirect(self, input=""):
        return ""

    def getArguments(self, input=""):
        return ""

    def getEnv(self, input=""): # env vars string, as passed to env or export (e.g., "A=3 B=4"), or empty
        return ""

    def getInputSets(self):
        return []

    def _envSubst(self, strg):
        res = strg
        for key in self.envs:
            res = res.replace("$" + key, str(self.envs[key]))
        return res

class SPECApplication(Application):
    def __init__(self, envs, name, test_args="", train_args="", ref_args=""):
        Application.__init__(self, envs)
        self.name = name

        dot_loc = name.find(".")
        if dot_loc != -1:
            self.s_name = name[dot_loc+1:]
            self.number = name[0:dot_loc]
        else:
            self.s_name = name
            self.bumber = name
        
        self.args = {}
        self.inputRedir = {}

        # Parse input redirect from args string
        for (iname, args) in [("test", test_args), ("train", train_args), ("ref", ref_args)]:
            if args.find("<") == -1:
                self.args[iname] = args
                self.inputRedir[iname] = ""
            else:
                aLst = args.split("<")
                assert(len(aLst) == 2) #otherwise, something's terribly wrong...
                self.args[iname] = aLst[0]
                self.inputRedir[iname] = aLst[1]

        self.suite = "suitename"
        self.setupAction = "link" # copy, link or none

    def getCommand(self):
        return getCommand(self, "test")

    def getCommand(self, input_type):
        return self.s_name + "_base." + self.envs["SPEC_CFG"]

#inserted by CJM
    def getRunDir(self, input_type):
        return self.envs["BENCH_DIR"] + self.suite + "/" + self.name + "/run/run_base_" + input_type + "_" + self.envs["SPEC_CFG"] + ".0000"

    def getSetupCommand(self, input):
        if self.setupAction == "none":
            return ""

        benchInputDir = self.envs["INPUTS_DIR"] + "/" + self.suite + "/" + self.name + "/"
        
        allDir = benchInputDir + "all/"
        inputDir = benchInputDir + input + "/"

        cmds = []
        iCmd = "ln -s "
        if self.setupAction == "copy":
            iCmd = "cp -r "
            
        if os.path.exists(allDir):
            #cmds.append("cp -r " + allDir + "* .")
            for fd in os.listdir(allDir):
                cmds.append(iCmd + allDir + "/" + fd + "  " + fd)
            

        if os.path.exists(inputDir):
            for fd in os.listdir(inputDir):
                cmds.append(iCmd + inputDir + "/" + fd + "  " + fd)
            #cmds.append("cp -r " + inputDir + "* .")

        if self.setupAction == "copy":
            cmds.append("chmod -R u+w .")

        #if len(cmds) == 0: return ""
        #if len(cmds) == 1: return cmds[0]
        #if len(cmds) == 2: return " && ".join(cmds)
        return "\n".join(cmds)
        assert(False)

    def getTeardownCommand(self, input):
        return "" #TODO, not vital for now

    def getInputRedirect(self, input):
        return self._envSubst(self.inputRedir[input])

    def getArguments(self, input):
        return self._envSubst(self.args[input])

    def getInputSets(self):
        return ["test", "train", "ref"]

class SPECCPU2006Application(SPECApplication):
    def __init__(self, envs, name, test_args="", train_args="", ref_args=""):
        SPECApplication.__init__(self, envs, name, test_args, train_args, ref_args)
        self.suite = "CPU2006"
        if name == "435.gromacs":
            self.setupAction = "copy"


class SPECOMP2001Application(SPECApplication):
    def __init__(self, envs, name, test_args="", train_args="", ref_args=""):
        SPECApplication.__init__(self, envs, name, test_args, train_args, ref_args)
        self.suite = "specomp2001"
        self.max_threads = 0

    def getSetupCommand(self, input):
        assert "THREADS" in self.envs.keys()
        res = SPECApplication.getSetupCommand(self, input)
        res += "\nexport OMP_NUM_THREADS=" + str(self.envs["THREADS"]) + "\n" #TODO: Remove once getEnv is always used
        return res

    def getEnv(self, input):
        return "OMP_NUM_THREADS=" + str(self.envs["THREADS"])

class SPECOMP2012Application(SPECOMP2001Application):
    def __init__(self, envs, name, args="", test_args="", train_args="", ref_args=""):
        # Use either args or test/train/ref_args; args is a convenience for benchmarks with repeated arguments
        if test_args == "" and train_args == "" and ref_args == "":
            test_args = train_args = ref_args = args
        else:
            assert args == ""
        SPECOMP2001Application.__init__(self, envs, name, test_args, train_args, ref_args)
        self.suite = "specomp2012"


class SPECAppDict(dict):
    def __init__(self, envs):

        #FIXME: perlbench has severla runs per set; only doing the first one
        self["400.perlbench"] = SPECCPU2006Application(envs,
                name = "400.perlbench",
                test_args = "-I. -I./lib attrs.pl",
                train_args = "-I./lib diffmail.pl 2 550 15 24 23 100",
                ref_args = "-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1")


        self["401.bzip2"] = SPECCPU2006Application(envs,
                name = "401.bzip2",
                test_args = "dryer.jpg 2",
                train_args = "byoudoin.jpg 5",
                ref_args = "input.source 64") #let's be a little easier on footprint #"input.source 280") #many more cases

        self["403.gcc"] = SPECCPU2006Application(envs,
                name = "403.gcc",
                test_args = "-C cccp.i -o out.o",
                train_args = "-C integrate.i -o out.o",
                ref_args = "-C scilab.i -o out.o") #FIXME: gcc ref's run is actually a sequence of runs, each with a different input file. This is just the largest file.

        self["410.bwaves"] = SPECCPU2006Application(envs, "410.bwaves")

        self["416.gamess"] = SPECCPU2006Application(envs,
                name = "416.gamess",
                test_args = "<exam29.config",
                train_args = "<h2ocu2+.energy.config",
                ref_args = "<cytosine.2.config") #FIXME: As in gcc, only one of 3 possible runs

        self["429.mcf"] = SPECCPU2006Application(envs,
                name = "429.mcf",
                test_args = "inp.in",
                train_args = "inp.in",
                ref_args = "inp.in")

        self["433.milc"] = SPECCPU2006Application(envs,
                name = "433.milc",
                test_args = "<su3imp.in",
                train_args = "<su3imp.in",
                ref_args = "<su3imp.in")

        self["434.zeusmp"] = SPECCPU2006Application(envs, "434.zeusmp")

        self["435.gromacs"] = SPECCPU2006Application(envs,
                name = "435.gromacs",
                test_args = "-silent -deffnm gromacs.tpr -nice 0",
                train_args = "-silent -deffnm gromacs.tpr -nice 0",
                ref_args = "-silent -deffnm gromacs.tpr -nice 0")

        self["436.cactusADM"] = SPECCPU2006Application(envs,
                name = "436.cactusADM",
                test_args = "benchADM.par",
                train_args = "benchADM.par",
                ref_args = "benchADM.par")

        self["437.leslie3d"] = SPECCPU2006Application(envs,
                name = "437.leslie3d",
                test_args = "<leslie3d.in",
                train_args = "<leslie3d.in",
                ref_args = "<leslie3d.in")

        self["444.namd"] = SPECCPU2006Application(envs,
                name = "444.namd",
                test_args = "--input namd.input --iterations 1 --output namd.out",
                train_args = "--input namd.input --iterations 1 --output namd.out", #NOTE: train == test
                ref_args = "--input namd.input --iterations 38 --output namd.out")

        #FIXME: There are a few more inputs for each of these
        self["445.gobmk"] = SPECCPU2006Application(envs,
                name = "445.gobmk",
                test_args = "--quiet --mode gtp <capture.tst",
                train_args = "--quiet --mode gtp <arb.tst",
                ref_args = "--quiet --mode gtp <13x13.tst")

        self["447.dealII"] = SPECCPU2006Application(envs,
                name = "447.dealII",
                test_args = "8",
                train_args = "10",
                ref_args = "23")

        #FIXME: Missing A LOT of stuff; essentially, each run is 2 invocations and the secons seems to depend on the first. Only doing the first here.
        # Actually, this is the second one. If the run fails, do the first one (pds*.mps) instead
        self["450.soplex"] = SPECCPU2006Application(envs,
                name = "450.soplex",
                test_args = "-m10000 test.mps",
                train_args = "-m1200 train.mps",
                ref_args = "-m3500 ref.mps")

        self["453.povray"] = SPECCPU2006Application(envs,
                name = "453.povray",
                test_args = "SPEC-benchmark-test.ini",
                train_args = "SPEC-benchmark-train.ini",
                ref_args = "SPEC-benchmark-ref.ini")

        self["454.calculix"] = SPECCPU2006Application(envs,
                name = "454.calculix",
                test_args = "-i beampic",
                train_args = "-i stairs",
                ref_args = "-i hyperviscoplastic")

        self["456.hmmer"] = SPECCPU2006Application(envs,
                name = "456.hmmer",
                test_args = "--fixed 0 --mean 325 --num 45000 --sd 200 --seed 0 bombesin.hmm",
                train_args = "--fixed 0 --mean 425 --num 85000 --sd 300 --seed 0 leng100.hmm",
                ref_args = "nph3.hmm swiss41") #some more ref ones

        self["458.sjeng"] = SPECCPU2006Application(envs,
                name = "458.sjeng",
                test_args = "test.txt",
                train_args = "train.txt",
                ref_args = "ref.txt")

        self["459.GemsFDTD"] = SPECCPU2006Application(envs, "459.GemsFDTD")

        self["462.libquantum"] = SPECCPU2006Application(envs,
                name = "462.libquantum",
                test_args = "33 5",
                train_args = "143 25",
                ref_args = "1397 8")

        self["464.h264ref"] = SPECCPU2006Application(envs,
                name = "464.h264ref",
                test_args = "-d foreman_test_encoder_baseline.cfg",
                train_args = "-d foreman_train_encoder_baseline.cfg",
                ref_args = "-d foreman_ref_encoder_baseline.cfg")

        self["465.tonto"] = SPECCPU2006Application(envs, "465.tonto")

        self["470.lbm"] = SPECCPU2006Application(envs,
                name = "470.lbm",
                test_args = "20 reference.dat 0 1 100_100_130_cf_a.of",
                train_args = "300 reference.dat 0 1 100_100_130_cf_b.of",
                ref_args = "3000 reference.dat 0 0 100_100_130_ldc.of")

        self["471.omnetpp"] = SPECCPU2006Application(envs,
                name = "471.omnetpp",
                test_args = "omnetpp.ini",
                train_args = "omnetpp.ini",
                ref_args = "omnetpp.ini")

        self["473.astar"] = SPECCPU2006Application(envs,
                name = "473.astar",
                test_args = "lake.cfg",
                train_args = "BigLakes1024.cfg",
                ref_args = "BigLakes2048.cfg") #ref and train have additional runs

        # NOTE: The all/ input files are endianness and 32/64-bit dependent. Currently using le/64
        self["481.wrf"] = SPECCPU2006Application(envs, "481.wrf")

        # NOTE: Generating the inputs was a bit involved and this depends on the architecture; please see the script to generate the ctlfiles
        self["482.sphinx3"] = SPECCPU2006Application(envs,
                name = "482.sphinx3",
                test_args = "ctlfile.le . args.an4",
                train_args = "ctlfile.le . args.an4",
                ref_args = "ctlfile.le . args.an4")

        self["483.xalancbmk"] = SPECCPU2006Application(envs,
                name = "483.xalancbmk",
                test_args = "-v test.xml xalanc.xsl",
                train_args = "-v allbooks.xml xalanc.xsl",
                ref_args = "-v t5.xml xalanc.xsl")

        self["wupwise_m"] = SPECOMP2001Application(envs, "wupwise_m")

        self["swim_m"] = SPECOMP2001Application(envs,
                name = "swim_m",
                test_args = "<swim.in",
                train_args = "<swim.in",
                ref_args = "<swim.in")

        self["mgrid_m"] = SPECOMP2001Application(envs,
                name = "mgrid_m",
                test_args = "<mgrid.in",
                train_args = "<mgrid.in",
                ref_args = "<mgrid.in")

        self["applu_m"] = SPECOMP2001Application(envs,
                name = "applu_m",
                test_args = "<applu.in",
                train_args = "<applu.in",
                ref_args = "<applu.in")

        self["equake_m"] = SPECOMP2001Application(envs,
                name = "equake_m",
                test_args = "<inp.in",
                train_args = "<inp.in",
                ref_args = "<inp.in")

        self["apsi_m"] = SPECOMP2001Application(envs, "apsi_m")

        self["gafort_m"] = SPECOMP2001Application(envs, "gafort_m")

        self["fma3d_m"] = SPECOMP2001Application(envs, "fma3d_m")

        self["art_m"] = SPECOMP2001Application(envs,
                name = "art_m",
                test_args = "-scanfile c756hel.in -trainfile1 a10.img -trainfile2 hc.img -stride 2 -startx 130 -starty 220 -endx 150 -endy 230 -objects 1",
                train_args = "-scanfile c756hel.in -trainfile1 a10.img -trainfile2 hc.img -stride 2 -startx 130 -starty 220 -endx 150 -endy 230 -objects 10", #2 train runs
                ref_args = "-scanfile c756hel.in -trainfile1 a10.img -trainfile2 hc.img -stride 1 -startx 110 -starty 220 -endx 172 -endy 260 -objects 1000")

        self["ammp_m"] = SPECOMP2001Application(envs,
                name = "ammp_m",
                test_args = "<ammp.in",
                train_args = "<ammp.in",
                ref_args = "<ammp.in")

        # OMP2012

        # Benchmarks without args
        for benchName in ["350.md", "351.bwaves", "357.bt331", "360.ilbdc", "362.fma3d", "370.mgrid331", "371.applu331"]:
            self[benchName] = SPECOMP2012Application(envs, benchName)
        
        self["352.nab"] = SPECOMP2012Application(envs, 
                name = "352.nab",
                test_args = "hkrdenq 1930344093",
                train_args = "aminos 391519156",
                ref_args = "1ea0 281910391")
        
        self["358.botsalgn"] = SPECOMP2012Application(envs, "358.botsalgn", args="-f botsalgn")
        
        self["359.botsspar"] = SPECOMP2012Application(envs,
                name = "359.botsspar",
                test_args = "-n 50 -m 25",
                train_args = "-n 100 -m 25",
                ref_args = "-n 120 -m 501")
        
        self["363.swim"] = SPECOMP2012Application(envs, "363.swim", args = "<swim.in")

        # Some imagick runs have more commands; these are the first ones
        self["367.imagick"] = SPECOMP2012Application(envs,
                name = "367.imagick",
                test_args = "-shear 25 -resize 640x480 -negate -alpha Off input.tga output.tga",
                train_args = "-shear 31 -resize 1280x960 -negate -edge 14 -implode 1.2 -flop -convolve 1,2,1,4,3,4,1,2,1 -edge 100 input1.tga output1.tga",
                ref_args = "input2.tga -shear 31 -resize 12000x9000 -negate -edge 14 -implode 1.2 -flop -convolve 1,2,1,4,3,4,1,2,1 -edge 100 -resize 800x600 output2.tga")

        self["372.smithwa"] = SPECOMP2012Application(envs,
                name = "372.smithwa",
                test_args = "30",
                train_args = "32",
                ref_args = "41")

        self["376.kdtree"] = SPECOMP2012Application(envs,
                name = "376.kdtree",
                test_args = "100000 10 2",
                train_args = "400000 10 2",
                ref_args = "1400000 4 2")

# Parsec apps

class ConfigDictApplication(Application):
    def __init__(self, envs, name, configDict):
        Application.__init__(self, envs)
        self.name = name

        self.inputSets = configDict.keys()
        self.configDict = configDict

        self.suite = "generic" # Child must define
        self.setupAction = "link" # copy, link or none

    def getCommand(self):
        return self.envs["BENCH_DIR"] + "/" + self.suite + "/" + self.name + "/" + self.name

    def getSetupCommand(self, input):
        if self.setupAction == "none":
            return ""

        iDirName = str(self.configDict[input][0]) #can be None


        benchInputDir = self.envs["INPUTS_DIR"] + "/" + self.suite + "/" + self.name + "/"
        
        allDir = benchInputDir + "all/"
        inputDir = benchInputDir + iDirName + "/"

        cmds = []
        iCmd = "ln -s "
        if self.setupAction == "copy":
            iCmd = "cp -r "
            
        if os.path.exists(allDir):
            for fd in os.listdir(allDir):
                cmds.append(iCmd + allDir + "/" + fd + "  " + fd)

        if os.path.exists(inputDir):
            for fd in os.listdir(inputDir):
                cmds.append(iCmd + inputDir + "/" + fd + "  " + fd)

        if self.setupAction == "copy":
            cmds.append("chmod -R u+w .")

        res = "\n".join(cmds)
        return res

    def getTeardownCommand(self, input):
        return "" #TODO, not vital for now

    def getInputRedirect(self, input):
        return ""

    def getArguments(self, input):
        return self._envSubst(self.configDict[input][1])

    def getInputSets(self):
        return self.inputSets


class ParsecApplication(ConfigDictApplication):
    def __init__(self, envs, name, configDict):
        ConfigDictApplication.__init__(self, envs, name, configDict)
        self.suite = "parsec"
        if name == "freqmine":
            self.setupAction = "copy"

    def getSetupCommand(self, input):
        res = ConfigDictApplication.getSetupCommand(self, input)
        if self.name == "freqmine": #TODO: Remove once getEnv is always used
            res += "\nexport OMP_NUM_THREADS=" + str(self.envs["THREADS"]) + "\n"
        return res

    def getEnv(self, input):
        if self.name == "freqmine":
            return "OMP_NUM_THREADS=" + str(self.envs["THREADS"])
        else:
            return ""


class ParsecAppDict(dict):
    def __init__(self, envs):
        self["canneal"] = ParsecApplication(envs, "canneal", {
                "native" : ("native", "$THREADS 16384 2000 2500000.bnets 10000"), # in theory this is without the number of steps, BUT lack of steps creates early termination because algorithm seems fucked up. So fixed huge number of steps.
                "native100" : ("native", "$THREADS 16384 2000 2500000.bnets 100"),
                "simlarge" : ("simlarge", "$THREADS 16384 2000 400000.bnets 10000"),
                "simsmall" : ("simsmall", "$THREADS 16384 2000 100000.bnets 10000"),
                "simmedium" : ("simmedium", "$THREADS 16384 2000 200000.bnets 10000"),

                "huge100" : ("native", "$THREADS  262144 2000 2500000.bnets 100") # like native100, but phase length multiplied by 32, 256K moves/temp
            })

        self["blackscholes"] = ParsecApplication(envs, "blackscholes", {
                "native" : (None, "$THREADS 10000000"),
                "simlarge" : (None, "$THREADS 65536"),
                "250k" : (None, "$THREADS 250000")
            })

        self["fluidanimate"] = ParsecApplication(envs, "fluidanimate", {
                "native" : ("native", "$THREADS 500 in_500K.fluid out.fluid"),
                "native10" : ("native", "$THREADS 10 in_500K.fluid out.fluid"),
                "native20" : ("native", "$THREADS 20 in_500K.fluid out.fluid"),
                "simlarge" : ("simlarge", "$THREADS 5 in_300K.fluid out.fluid")
            })

        self["swaptions"] = ParsecApplication(envs, "swaptions", {
                "native" : (None, "-ns 128 -sm 1000000 -nt $THREADS"),
                "simlarge" : (None, "-ns 64 -sm 20000 -nt $THREADS"),
                "16K20K" : (None, "-ns 16384 -sm 20000 -nt $THREADS")
            })

        self["freqmine"] = ParsecApplication(envs, "freqmine", {
                "native" : ("native", "webdocs_250k.dat 11000"),
                "simlarge" : ("simlarge", "kosarak_990k.dat 790")
            })

        self["streamcluster"] = ParsecApplication(envs, "streamcluster", {
                "native" : (None, "10 20 128 1000000 200000 5000 none output.txt $THREADS"),
                "simlarge" : (None, "10 20 128 16384 16384 1000 none output.txt $THREADS")
            })

        self["facesim"] = ParsecApplication(envs, "facesim", {
                "native" : ("native", "-timing -threads $THREADS -lastframe 100"),
                "simlarge" : ("simlarge", "-timing -threads $THREADS")
            })

        self["bodytrack"] = ParsecApplication(envs, "bodytrack", {
                "native" : ("native", "sequenceB_261 4 261 4000 5 0 $THREADS"),
                "simlarge" : ("simlarge", "sequenceB_4 4 4 4000 5 0 $THREADS")
            })

        #self["raytrace_parsec"] = ParsecApplication(envs, "raytrace", {
        #        "native" : ("native", "thai_statue.obj -automove -nthreads $THREADS -frames 200 -res 1920 1080"),
        #        "simlarge" : ("simlarge", "happy_buddha.obj -automove -nthreads $THREADS -frames 3 -res 1920 1080")
        #    })

# SPLASH2 apps

class Splash2Application(ConfigDictApplication):
    def __init__(self, envs, type, name, configDict, variantSubdir = None):
        ConfigDictApplication.__init__(self, envs, name, configDict)
        self.suite = "splash2" + "/" + type # kernels or apps
        self.variantSubdir = variantSubdir

    def getCommand(self):
        cmd = self.envs["BENCH_DIR"] + "/" + self.suite + "/"
        if not self.variantSubdir == None: cmd += self.variantSubdir + "/"
        else:  cmd += self.name + "/"
        if self.name == "lu_nc" or self.name == "ocean_nc":
            cmd += self.name.split("_")[0]
        else:
            cmd += self.name
        return cmd

    def getSetupCommand(self, input):
        res = ConfigDictApplication.getSetupCommand(self, input)
        if self.name in ["barnes", "fmm", "water", "water-nsquared"]:
            # Use sed to adapt a templated input file
            genInputCmd = "cat " + self.configDict[input][2] + ".template" 
            genInputCmd += " | sed s/\$THREADS/" + str(self.envs["THREADS"]) + "/"
            argIdx = 0
            for arg in input.split("-"):
                genInputCmd += " | sed s/\$ARG%d/%s/" % (argIdx, arg)
                argIdx += 1
            genInputCmd += " | cat > " + self.configDict[input][2]
            res += "\n" + genInputCmd + "\n"
        return res

    def getInputRedirect(self, input):
        return self._envSubst(self.configDict[input][2])

class Splash2AppDict(dict):
    def __init__(self, envs):
        ## Apps
        self["barnes"] = Splash2Application(envs, "apps", "barnes", {
                "16384" : (None, "", "barnes.in"),
                "131072" : (None, "", "barnes.in"),
                "131072-long" : (None, "", "barnes-long.in"), #virtually infinite number of timesteps
                "262144" : (None, "", "barnes.in"), # simlarge
                "1048576" : (None, "", "barnes.in"),
                "2097152" : (None, "", "barnes.in"), # native
            })
        self["fmm"] = Splash2Application(envs, "apps", "fmm", {
                "16384" : (None, "", "fmm.in"),
                "131072" : (None, "", "fmm.in"),
                "262144" : (None, "", "fmm.in"), # simlarge
                "1048576" : (None, "", "fmm.in"),
                "4194304" : (None, "", "fmm.in"), # native
            })
        self["ocean"] = Splash2Application(envs, "apps", "ocean", {
                "258" : (None, "-n258 -p$THREADS", ""),
                "1026" : (None, "-n1026 -p$THREADS", ""),
                "2050" : (None, "-n2050 -p$THREADS", ""),
                "simlarge" : (None, "-n2050 -p$THREADS", ""),
                "native" : (None, "-n4098 -p$THREADS", ""),
            }, variantSubdir = "ocean/contiguous_partitions")

        self["ocean_nc"] = Splash2Application(envs, "apps", "ocean_nc", {
                "258" : (None, "-n258 -p$THREADS", ""),
                "1026" : (None, "-n1026 -p$THREADS", ""),
                "2050" : (None, "-n2050 -p$THREADS", ""),
                "simlarge" : (None, "-n2050 -p$THREADS", ""),
                "native" : (None, "-n4098 -p$THREADS", ""),
            }, variantSubdir = "ocean/contiguous_partitions")

        # radiosity does not work yet, see commit/README file for info
        self["raytrace"] = Splash2Application(envs, "apps", "raytrace", {
                "balls4" : ("balls4", "-p$THREADS inputs/balls4.env", ""),
                "simlarge" : ("balls4", "-p$THREADS -a8 inputs/balls4.env", ""),
                # poan: 02/04/16 need 64 MB share mem for the largest input
                "car" : ("car", "-p$THREADS -m64 inputs/car.env", ""),
                "native" : ("car", "-p$THREADS -m64 -a128 inputs/car.env", ""),
                "teapot" : ("teapot", "-p$THREADS inputs/teapot.env", ""),
            })
        # poan: this is from splash2x in parsec
        # Ref: http://parsec.cs.princeton.edu/doc/memo-splash2x-input.pdf
        self["volrend"] = Splash2Application(envs, "apps", "volrend", {
                "native" : ("", "$THREADS head 1000", ""),
                "simlarge" : ("", "$THREADS head-scaleddown2 100", ""),
                "simsmall" : ("", "$THREADS head-scaleddown4 20", ""),
            })

        # FIXME (dsm May 23 2014): Both of these are named water, which confuses the newer scripts
        # poan: 02/04/16 add a soft link called water-nsquared
        self["water_nsquared"] = Splash2Application(envs, "apps", "water-nsquared", {
            "17576" : (None, "", "water.in"), # tiny
            "132651" : (None, "", "water.in"), #~170MB footprint
            "1061208" : (None, "", "water.in"), # long, around 1GB footprint
            }, variantSubdir = "water-nsquared")
        #self["water_spatial"] = Splash2Application(envs, "apps", "water", {
        self["water"] = Splash2Application(envs, "apps", "water", {
            "17576" : (None, "", "water.in"),
            "132651" : (None, "", "water.in"), # simlarge
            "1061208" : (None, "", "water.in"), # native 
            }, variantSubdir = "water-spatial")

        ## Kernels
        # TODO cholesky -- all input files are too small...
        self["fft"] = Splash2Application(envs, "kernels", "fft", {
                "1M" : (None, "-m20 -p$THREADS", ""),
                "4M" : (None, "-m22 -p$THREADS", ""),
                "16M" : (None, "-m24 -p$THREADS", ""),
                "simlarge" : (None, "-m24 -p$THREADS", ""),
                "64M" : (None, "-m26 -p$THREADS", ""), # ~3GB WS, but short
                "native" : (None, "-m28 -p$THREADS", ""),
            })
        self["lu"] = Splash2Application(envs, "kernels", "lu", {
                "simlarge" : (None, "-n2048 -p$THREADS", ""), # short, ~32MB WS
                "2048" : (None, "-n2048 -p$THREADS", ""), # short, ~32MB WS
                "4096" : (None, "-n4096 -p$THREADS", ""), # medium, ~170MB WS
                "native" : (None, "-n8192 -p$THREADS", ""), # long, ~512MB WS
                "8192" : (None, "-n8192 -p$THREADS", ""), # long, ~512MB WS
            }, variantSubdir = "lu/contiguous_blocks")
        self["lu_nc"] = Splash2Application(envs, "kernels", "lu_nc", {
                "simlarge" : (None, "-n2048 -p$THREADS", ""), # short, ~32MB WS
                "2048" : (None, "-n2048 -p$THREADS", ""), # short, ~32MB WS
                "4096" : (None, "-n4096 -p$THREADS", ""), # medium, ~170MB WS
                "native" : (None, "-n8192 -p$THREADS", ""), # long, ~512MB WS
                "8192" : (None, "-n8192 -p$THREADS", ""), # long, ~512MB WS
            }, variantSubdir = "lu/non_contiguous_blocks")
        self["radix"] = Splash2Application(envs, "kernels", "radix", {
                "1M"   : (None, "-m1048576   -n1048576   -p$THREADS", ""),
                "4M"   : (None, "-m4194304   -n4194304   -p$THREADS", ""),
                "16M"  : (None, "-m16777216  -n16777216  -p$THREADS", ""),
                "64M"  : (None, "-m67108864  -n67108864  -p$THREADS", ""),
                "simmedium"  : (None, "-m16777216  -n16777216  -p$THREADS", ""),
                "simlarge"  : (None, "-m67108864  -n67108864  -p$THREADS", ""),
                "128M" : (None, "-m134217728 -n134217728 -p$THREADS", ""),
                "native" : (None, "-m268435456 -n268435456 -p$THREADS", ""),
                "256M" : (None, "-m268435456 -n268435456 -p$THREADS", ""),
            })
 

## BioParallel
class BioParallelApplication(ConfigDictApplication):
    def __init__(self, envs, name, configDict):
        ConfigDictApplication.__init__(self, envs, name, configDict)
        self.suite = "bioparallel"

    def getSetupCommand(self, input):
        res = ConfigDictApplication.getSetupCommand(self, input)
        # These are all openmp apps
        #TODO: Remove once getEnv is always used
        res += "\nexport OMP_NUM_THREADS=" + str(self.envs["THREADS"]) + "\n"
        return res

    def getEnv(self, input):
        # These are all openmp apps
        return "OMP_NUM_THREADS=" + str(self.envs["THREADS"])



class BioParallelAppDict(dict):
    def __init__(self, envs):
        self["103.semphy"] = BioParallelApplication(envs, "103.semphy", {
                "53"  : ("53",  "-s 53.phy -f phylip -m jtt -G 0.3"),
                "108" : ("108", "-s 108.phy -f phylip -m jtt -G 0.3"),
                "220" : ("220", "-s 220.phy -f phylip -m jtt -G 0.3"),
            })
        self["104.svm"] = BioParallelApplication(envs, "104.svm", {
                "30" : ("", "outData.txt 253 15154 30"), # original command line
                # HK: longer runs
                "100" : ("", "outData.txt 253 15154 100"), # 100 iterations
                "3"  : ("", "outData.txt 253 15154 3"), # 3 iterations
                # poan: smaller input set
                "s_1000"  : ("", "smallOutData.txt 30 15154 1000"), # 1000 iterations
                "t_4000"  : ("", "tinyOutData.txt 7 15154 4000"), # 4000 iterations

            })
        self["105.plsa"] = BioParallelApplication(envs, "105.plsa", {
                "30k"   : ("30k",  "30k_1.txt 30k_2.txt pam120.bla 600 400 3 3 1 $THREADS"),
                "100k"  : ("100k", "100k_1.txt 100k_2.txt pam120.bla 600 400 3 3 1 $THREADS"),
                "300k"  : ("300k", "300k_1.txt 300k_2.txt pam120.bla 600 400 3 3 1 $THREADS"),
            })


## MineBench (3.0)
# Commands copied from commandLines.txt, pretty much verbatim
class MineBenchApplication(ConfigDictApplication):
    def __init__(self, envs, name, program, configDict):
        ConfigDictApplication.__init__(self, envs, name, configDict)
        self.suite = "minebench"
        self.setupAction = "none" # will read inputs directly, input file location doesn't follow a clear-cut pattern and they're too large to copy around
        self.program = program

    def getCommand(self):
        return self.envs["BENCH_DIR"] + "/minebench/" + self.program

    def getSetupCommand(self, input):
        # Some of these are OpenMP
        # TODO: Remove once getEnv is always used
        return "\nexport OMP_NUM_THREADS=" + str(self.envs["THREADS"]) + "\n"
   
    def getEnv(self, input):
        # Some of these are OpenMP
        return "OMP_NUM_THREADS=" + str(self.envs["THREADS"])

    def getArguments(self, input):
        args = self._envSubst(self.configDict[input])
        return args

    def getInputRedirect(self, input):
        return "" # MineBench apps don't have input redir


# To give some homogeneity, all apps are lowercased,  and datasets are named ref/train/test 
# NOTE: What the fuck is up with the offset files in some apps, e.g. APR? Looks like they might contain per-thread offsets.
# If this is the case, automating this is going to be hell, so fuck those apps. No sane developer would ever write that shit.
# --
# Checked, these are actually offsets and apps segfault without the correct offset/processor matching. In an attempt to save face, I'm tailoring runs a bit by expanding the THREADS env var. But seriously, who thought this was a reasonable idea??
class MineBenchAppDict(dict):
    def __init__(self, envs):
        self["hop"] = MineBenchApplication(envs, "hop", "HOP/para_hop", {
               "ref"  : "3932160 $INPUTS_DIR/minebench/HOP/particles_0_256 64 16 -1 $THREADS",
               "train"  : "491520 $INPUTS_DIR/minebench/HOP/particles_0_128 64 16 -1 $THREADS",
               "test"  : "61440 $INPUTS_DIR/minebench/HOP/particles_0_64 64 16 -1 $THREADS",
               })
        self["apr"] = MineBenchApplication(envs, "apr", "APR/no_output_apriori", {
               "ref"  : "-i $INPUTS_DIR/minebench/APR/data.ntrans_10000.tlen_20.nitems_1.npats_2000.patlen_6 -f $INPUTS_DIR/minebench/APR/offset_file_10000_P$THREADS.txt -s 0.0075 -n $THREADS",
               # this is the ref dataset, but it's minuscule
               "train"  : "-i $INPUTS_DIR/minebench/APR/data.ntrans_1000.tlen_10.nitems_1.npats_2000.patlen_6 -f $INPUTS_DIR/minebench/APR/offset_file_1000_10_1_P$THREADS.txt -s 0.0075 -n $THREADS",
               })
        self["bayesian"] = MineBenchApplication(envs, "bayesian", "Bayesian/bci", {
               "ref"  : "-d $INPUTS_DIR/minebench/Bayesian/F26-A64-D250K_bayes.dom $INPUTS_DIR/minebench/Bayesian/F26-A64-D250K_bayes.tab $INPUTS_DIR/minebench/Bayesian/F26-A64-D250K_bayes.nbc",
               })
        self["scalparc"] = MineBenchApplication(envs, "scalparc", "ScalParC/scalparc", {
               "ref"  : "$INPUTS_DIR/minebench/ScalParC/para_F26-A64-D2500K/F26-A64-D2500K.tab 2500000 64 2 $THREADS",
               "train"  : "$INPUTS_DIR/minebench/ScalParC/para_F26-A32-D250K/F26-A32-D250K.tab 250000 32 2 $THREADS",
               })
        self["kmeans"] = MineBenchApplication(envs, "kmeans", "kmeans/example", {
               "ref"  : "-i $INPUTS_DIR/minebench/kmeans/edge -b -o -p $THREADS",
               "train"  : "-i $INPUTS_DIR/minebench/kmeans/color -b -o -p $THREADS",
               "test"  : "-i $INPUTS_DIR/minebench/kmeans/texture100 -b -o -p $THREADS",
               })
        self["kmeans.fuzzy"] = MineBenchApplication(envs, "kmeans.fuzzy", "kmeans/example", {
               "ref"  : "-i $INPUTS_DIR/minebench/kmeans/edge -b -o -f -p $THREADS",
               "train"  : "-i $INPUTS_DIR/minebench/kmeans/color -b -o -f -p $THREADS",
               "test"  : "-i $INPUTS_DIR/minebench/kmeans/texture100 -b -o -f -p $THREADS",
               })
        # rsearch is parallel, but segfaults when using more than one thread. Pure joy...
        self["rsearch"] = MineBenchApplication(envs, "rsearch", "rsearch/rsearch", {
               "ref"  : "-n 1000 -c -E 10 -m $INPUTS_DIR/minebench/rsearch/matrices/RIBOSUM85-60.mat $INPUTS_DIR/minebench/rsearch/Queries/mir-40.stk $INPUTS_DIR/minebench/rsearch/Databasefile/100Kdb.fa",
               })
        # We do the serial one one; the parallel one looks quite weird (offset files...)
        self["utility_mine"] = MineBenchApplication(envs, "utility_mine", "utility_mine/tran_utility/utility_mine", {
               "ref"  : "$INPUTS_DIR/minebench/utility_mine/RealData/real_data_aa_binary  $INPUTS_DIR/minebench/utility_mine/RealData/product_price_binary 0.01",
               "train" : "$INPUTS_DIR/minebench/utility_mine/GEN/data.ntrans_1000.tlen_10.nitems_1.patlen_6 $INPUTS_DIR/minebench/utility_mine/GEN/logn1000_binary 0.01",
               })
        # NOTE: geti, getipp, afi, rw, rwpp and pareti all use the same datasets. They are MINUSCULE, leading to tiny runs (0.1 sec or so on a 2GHz Core2...)


class StreamApplication(Application):
    def __init__(self, envs, argDict):
        Application.__init__(self, envs)
        self.name = "stream"
        self.suite = "stream"
        self.argDict = argDict
        self.max_threads = 0

    def getSetupCommand(self, input):
        #TODO: Remove once getEnv is always used
        assert "THREADS" in self.envs.keys()
        res = "export OMP_NUM_THREADS=" + str(self.envs["THREADS"]) + "\n"
        return res

    def getEnv(self, input):
        assert "THREADS" in self.envs.keys()
        res = "OMP_NUM_THREADS=" + str(self.envs["THREADS"])
        return res

    def getCommand(self):
        return self.envs["BENCH_DIR"] + "/" + self.suite + "/" + self.name

    def getArguments(self, input):
        return self.argDict[input] + " " + str(self.envs["THREADS"])

    def getInputSets(self):
        return self.argDict.keys()

## GRAMPS
# NOTE: These apps live on a separate repository, but their dictionaries are included here for
# convenience. You need to specify the GRAMPS_DIR environment variable for these to work.
# Also, specify the optional GRAMPS_OPTS variable for additional GRAMPS options.

class GRAMPSApplication(Application):
    def __init__(self, envs, progName, dir, configDict):
        Application.__init__(self, envs)
        self.dir = dir
        self.progName = progName

        self.inputSets = configDict.keys()
        self.configDict = configDict
        
        self.suite = "gramps"

    def _getAppDir(self):
        return self.envs["GRAMPS_DIR"] + "/tests/" + self.dir + "/"

    def getCommand(self):
        if self.progName == "grampsh":
            return self.envs["GRAMPS_DIR"] + "/tests/grampsh/grampsh"
        else:
            return self._getAppDir() + self.progName

    def getSetupCommand(self, input):
        cmds =  'export GRAMPS_CONFIGONLY=gramps-settings\n' #TODO: Remove once getEnv is always used
        cmds += 'echo "hw.numThreads = ' + str(self.envs["THREADS"]) + '" > gramps-settings\n'
        cmds += 'echo "hw.threadsPerCore = 1" >> gramps-settings\n'
        cmds += 'echo "hw.noPinPThreads = 1" >> gramps-settings\n'
        if "GRAMPS_OPTS" in self.envs:
            for line in self.envs["GRAMPS_OPTS"]:
                cmds += 'echo "'+ line +'" >> gramps-settings\n'
        return cmds
    
    def getEnv(self, input):
        return "GRAMPS_CONFIGONLY=gramps-settings"

    def getTeardownCommand(self, input):
        return "" #TODO

    def getInputRedirect(self, input):
        return "" # No GRAMPS program ever uses this

    def getArguments(self, input):
        self.envs["APP_DIR"] = self._getAppDir() # careful, this may have global side-effects, envs is not cloned...
        if self.progName == "grampsh":
            args = "--appdir "
        else:
            args = "--app_dir "
        args += self._getAppDir() + " "
        args += self._envSubst(self.configDict[input])
        return args

    def getInputSets(self):
        return self.inputSets

class GRAMPSAppDict(dict):
    def __init__(self, envs):
        if "GRAMPS_DIR" not in envs:
            # Don't even warn, most people will not use this
            #print "GRAMPS_DIR env var not specified, GRAMPS application dictionary will be empty"
            return

        self["mergesort"] = GRAMPSApplication(envs, "grampsh", "mergesort", {
                "test"  : "$APP_DIR/mergesort.cfg",
                "large" : "$APP_DIR/mergesort-large.cfg",
            })
        self["histogram"] = GRAMPSApplication(envs, "histogram", "histogram", {
                "red-courtyard-1080p"  : "$APP_DIR/courtyard-1080p.png",
                "com-courtyard-1080p"  : "$APP_DIR/courtyard-1080p.png --combine",
            })
        self["lr"] = GRAMPSApplication(envs, "lr", "linear-regression", {
                "red-60M"  : "--numPoints 60M",
                "com-60M"  : "--numPoints --combine",
            })
        self["pca"] = GRAMPSApplication(envs, "pca", "pca", {
                "1M"  : "--rows 1024 --cols 1024",
            })
        for app in ["fm", "tde", "recursiveGaussian", "srad"]:
            self[app] = GRAMPSApplication(envs, app, app, {
                    "small" : "--i=small",
                    "medium" : "--i=medium",
                    "large"  : "--i=large",
                    "xlarge" : "--i=xlarge",
                })
        self["fft2"] = GRAMPSApplication(envs, "fft2", "fft2", {
                "1K-10K"  : "--f=1024 --n=10000",
                "64K-5K"  : "--f=65536 --n=5000",
            })
        self["serpent"] = GRAMPSApplication(envs, "serpent", "serpent", {
                "24K"  : "--n=24000",
            })
        self["packetTracer"] = GRAMPSApplication(envs, "packetTracer", "packetTracer", {
                "fairy-512-0" : "--output ray-fairy-512-0.png --scene fairy --size 512 --maxdepth 0",
                "fairy-512-1" : "--output ray-fairy-512-1.png --scene fairy --size 512 --maxdepth 1",
                "fairy-2048-0" : "--output ray-fairy-2048-0.png --scene fairy --size 2048 --maxdepth 0",
                "fairy-4096-0" : "--output ray-fairy-4096-0.png --scene fairy --size 4096 --maxdepth 0",
            })


        
## Phoenix2
# NOTE: Similarly to GRAMPS, these apps live on a separate repository, but their dictionaries are included here for
# convenience. You need to specify the PHOENIX2_DIR environment variable for these to work.

class Phoenix2Application(Application):
    def __init__(self, envs, progName, dir, configDict):
        Application.__init__(self, envs)
        self.dir = dir
        self.progName = progName

        self.inputSets = configDict.keys()
        self.configDict = configDict
        
        self.suite = "phoenix2"

    def _getAppDir(self):
        return self.envs["PHOENIX2_DIR"] + "/tests/" + self.dir + "/"

    def getCommand(self):
        return self._getAppDir() + self.progName

    def getSetupCommand(self, input):
        # TODO: Remove exports (getEnv)
        cmds  = 'export MAPRED_NPROCESSORS=' + str(self.envs["THREADS"]) + '\n'
        cmds += 'export MAPRED_NOBIND=1\n'
        return cmds

    def getEnv(self, input):
        return "MAPRED_NPROCESSORS=" + str(self.envs["THREADS"]) + " MAPRED_NOBIND=1"

    def getTeardownCommand(self, input):
        return "" #TODO

    def getInputRedirect(self, input):
        return "" # No GRAMPS program ever uses this

    def getArguments(self, input):
        self.envs["APP_DIR"] = self._getAppDir() # careful, this may have global side-effects, envs is not cloned...
        args = self._envSubst(self.configDict[input])
        return args

    def getInputSets(self):
        return self.inputSets

class Phoenix2AppDict(dict):
    def __init__(self, envs):
        if "PHOENIX2_DIR" not in envs:
            # Don't even warn, most people will not use this
            #print "PHOENIX2_DIR env var not specified, Phoenix2 application dictionary will be empty"
            return

        self["phoenix2_histogram"] = Phoenix2Application(envs, "histogram", "histogram", {
                "small"  : "$APP_DIR/inputs/histogram_datafiles/small.bmp",
                "med" : "$APP_DIR/inputs/histogram_datafiles/med.bmp",
                "large" : "$APP_DIR/inputs/histogram_datafiles/large.bmp"
            })
        self["phoenix2_kmeans"] = Phoenix2Application(envs, "kmeans", "kmeans", {
                "d3c100p100Ks1K"  : "-d 3 -c 100 -p 100000 -s 1000", #default, ~10B instrs
                "d3c100p500Ks1K"  : "-d 3 -c 100 -p 500000 -s 1000" #~10x the work, ~100B instrs
            })
        self["phoenix2_linear_regression"] = Phoenix2Application(envs, "linear_regression", "linear_regression", {
                "50M"  : "$APP_DIR/inputs/linear_regression_datafiles/key_file_50MB.txt",
                "100M" : "$APP_DIR/inputs/linear_regression_datafiles/key_file_100MB.txt",
                "500M" : "$APP_DIR/inputs/linear_regression_datafiles/key_file_500MB.txt"
            })
        # TODO: matrix_multiply needs fixing to not dump input and output
        self["phoenix2_pca"] = Phoenix2Application(envs, "pca", "pca", {
                "r1Kc1K" : "-r 1000 -c 1000 -s 10000", #5B instrs
                "r2Kc2K" : "-r 2000 -c 2000 -s 10000" #~10x the work
            })
        self["phoenix2_string_match"] = Phoenix2Application(envs, "string_match", "string_match", {
                "50M"  : "$APP_DIR/inputs/string_match_datafiles/key_file_50MB.txt",
                "100M" : "$APP_DIR/inputs/string_match_datafiles/key_file_100MB.txt",
                "500M" : "$APP_DIR/inputs/string_match_datafiles/key_file_500MB.txt"
            })
        self["phoenix2_word_count"] = Phoenix2Application(envs, "word_count", "word_count", {
                "10M"  : "$APP_DIR/inputs/word_count_datafiles/word_10MB.txt",
                "50M"  : "$APP_DIR/inputs/word_count_datafiles/word_50MB.txt",
                "100M" : "$APP_DIR/inputs/word_count_datafiles/word_100MB.txt"
            })



# Ad-hoc JBB for HPCA submission. Hacky, ask Daniel before using!
class SPECJBBApplication(Application):
    def __init__(self, envs):
        Application.__init__(self, envs)
        self.name = "specjbb"
        self.suite = "jbb"
        self.basePath = "/filer2/ssd1/sanchezd/specjbb/"

    def getCommand(self):
        java = self.basePath + "jre1.5.0_22/bin/java"
        return java

    def getSetupCommand(self, input=""):
        cmds =  "export LD_LIBRARY_PATH=" + self.basePath + "zsim_jni:$LD_LIBRARY_PATH\n"
        cmds += "export CLASSPATH=" + self.basePath + "jbb.jar:" + self.basePath + "check.jar:" + self.basePath + "zsim_jni/zsim.jar\n"
        cmds += "export PATH=" + self.basePath + "jdk1.5.0_22/bin/:$PATH\n"
        cmds += "mkdir results\n" #just in case
        cmds += "ln -s " + self.basePath + "xml xml" 
        return cmds

    def getArguments(self, input=""):
        return "-Xms44000m -Xmx44000m -Xrs spec.jbb.JBBmain -propfile " + self.basePath + "SPECjbb_512wh.props"

    def getInputSets(self):
        return ["512wh"]


# Put them all together

class FullAppDict(dict):
    def __init__(self, envs):
        self.update(SPECAppDict(envs))
        self.update(ParsecAppDict(envs))
        self.update(Splash2AppDict(envs))
        self.update(BioParallelAppDict(envs))
        self.update(MineBenchAppDict(envs))
        self.update(GRAMPSAppDict(envs))
        self.update(Phoenix2AppDict(envs))
        self["specjbb"] = SPECJBBApplication(envs)
        self["stream"] = StreamApplication(envs, {
            "1M3R"   : "1000000 3",
            "10M3R"  : "10000000 3",
            "100M3R" : "100000000 3"
        })


class SPEC2006NamesDict(dict):
    def __init__(self):
        self["400"] = "400.perlbench"
        self["perlbench"] = "400.perlbench"

        self["401"] = "401.bzip2"
        self["bzip2"] = "401.bzip2"

        self["403"] = "403.gcc"
        self["gcc"] = "403.gcc"

        self["410"] = "410.bwaves"
        self["bwaves"] = "410.bwaves"

        self["416"] = "416.gamess"
        self["gamess"] = "416.gamess"

        self["429"] = "429.mcf"
        self["mcf"] = "429.mcf"

        self["433"] = "433.milc"
        self["milc"] = "433.milc"

        self["434"] = "434.zeusmp"
        self["zeusmp"] = "434.zeusmp"

        self["435"] = "435.gromacs"
        self["gromacs"] = "435.gromacs"

        self["436"] = "436.cactusADM"
        self["cactusADM"] = "436.cactusADM"

        self["437"] = "437.leslie3d"
        self["leslie3d"] = "437.leslie3d"

        self["444"] = "444.namd"
        self["namd"] = "444.namd"

        self["445"] = "445.gobmk"
        self["gobmk"] = "445.gobmk"

        self["447"] = "447.dealII"
        self["dealII"] = "447.dealII"

        self["450"] = "450.soplex"
        self["450"] = "450.soplex"

        self["453"] = "453.povray"
        self["povray"] = "453.povray"

        self["454"] = "454.calculix"
        self["calculix"] = "454.calculix"

        self["458"] = "458.sjeng"
        self["sjeng"] = "458.sjeng"

        self["459"] = "459.GemsFDTD"
        self["GemsFDTD"] = "459.GemsFDTD"

        self["462"] = "462.libquantum"
        self["libquantum"] = "462.libquantum"

        self["464"] = "464.h264ref"
        self["h264ref"] = "464.h264ref"

        self["465"] = "465.tonto"
        self["tonto"] = "465.tonto"

        self["470"] = "470.lbm"
        self["lbm"] = "470.lbm"

        self["471"] = "471.omnetpp"
        self["omnetpp"] = "471.omnetpp"

        self["473"] = "473.astar"
        self["astar"] = "473.astar"

        self["481"] = "481.wrf"
        self["wrf"] = "481.wrf"

        self["482"] = "482.sphinx3"
        self["sphinx3"] = "482.sphinx3"

        self["483"] = "483.xalancbmk"
        self["xalancbmk"] = "483.xalancbmk"

        self["998"] = "998.specrand"

        self["999"] = "999.specrand"


