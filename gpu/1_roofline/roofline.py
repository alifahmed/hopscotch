#!/usr/bin/env python3

###############################################################################
#
# Roofline plotter for CUDA enabled devices.
#
# Author:   Alif Ahmed
# email:    alifahmed@virginia.edu
# Updated:  Aug 06, 2019
#
###############################################################################
import argparse
import json
import subprocess
import sys
import shlex
import math
import matplotlib.pyplot as plt


###############################################################################
# Function definitions
###############################################################################

# Check if sval is a positive integer. Used for argument validity checking.
def positive_integer(sval):
    ival = int(sval)
    if ival <= 0:
        raise argparse.ArgumentTypeError("%s must be a positive integer" % sval)
    return ival

# Build and run the roofline kernel with different configurations
def run_bench(data_type, args):
    ai = []
    bw = []
    perf = []
    curr_flop = args.flop_min
    with open('.results_' + data_type + '.tmp', 'w') as file_out:
        while(curr_flop <= args.flop_max):
            make_cmd =  "make kernel USER_DEFS=\"-DFLOPS_PER_ELEM=" + str(curr_flop) + " -DDATA_T=" + data_type \
                        + " -DHS_ARRAY_SIZE_BYTE=" + str(args.working_set_size) \
                        + " -DNTRIES=" + str(args.ntries) + " -DHS_DEVICE=" + str(args.device) + "\""
            subprocess.run(shlex.split(make_cmd), check=True, stdout=subprocess.DEVNULL);
            #run kernel
            results_str = subprocess.check_output(shlex.split("./kernel"), universal_newlines=True)
            file_out.write(results_str)
            print(results_str, end='')
            results_arr = results_str.split()
            ai.append(float(results_arr[2]))
            bw.append(float(results_arr[3]))
            perf.append(float(results_arr[4]))
            curr_flop = math.ceil(curr_flop * args.flop_rate)
    return ai, bw, perf


###############################################################################
# Arguments
###############################################################################
parser = argparse.ArgumentParser(allow_abbrev=False)
parser.add_argument('--device', default ='0', type=int, metavar='X',
                    help='Device on which the benchmark will run. (default: %(default)s)')

parser.add_argument('--working-set-size', type=positive_integer, metavar='X',
                    help='Working set size in bytes. (default: half of global memory of selected device)')

parser.add_argument('--ntries', default='5', type=positive_integer, metavar='X',
                    help='Number of runs for each configuration (default: %(default)s)')

parser.add_argument('--flop-min', default='1', type=positive_integer, metavar='X',
                    help='Minimum value of FLOPs per data element. (default: %(default)s)')

parser.add_argument('--flop-max', default='8192', type=positive_integer, metavar='X',
                    help='Maximum value of FLOPs per data element. Actual value may differ depending on --min-flops and --flop-rate. (default: %(default)s)')

parser.add_argument('--flop-rate', default='2', type=float, metavar='X',
                    help='Increament rate of FLOPs per data element. Refer to README for detail. (default: %(default)s)')

parser.add_argument('--disable-sp', action='store_true',
                    help='Disable roofline plot for single precision floating point. Enabled by default.')

parser.add_argument('--disable-dp', action='store_true',
                    help='Disable roofline plot for double precision floating point. Enabled by default.')

parser.add_argument('--plot-file', default='roofline.pdf', type=str, metavar='X',
                    help='Roofline plot will be saved in this file. (default: %(default)s)')

args = parser.parse_args()


###############################################################################
# Query device
###############################################################################

#build query program
subprocess.run(shlex.split("make query"), check=True, stdout=subprocess.DEVNULL);

#run query
devices_json_str = subprocess.check_output("./query", universal_newlines=True)
devices = json.loads(devices_json_str)["cudaDevices"]
num_device = len(devices)
if (num_device == 0):
    print("No GPU found! Exiting...")
    sys.exit(-1)

print("")
print("================================================================================")

print("Available Device(s):")
for i in range(0, num_device):
    print ("    [device " + str(i) + "] " + devices[i]["name"])


###############################################################################
# Validate arguments
###############################################################################
if (args.device >= num_device):
    raise argparse.ArgumentTypeError("--device must be less than " + str(num_device))

selected_device = devices[args.device]
print("Selected [device " + str(args.device) + "]. Use \"--device\" to run on other available devices.")

global_mem_bytes = selected_device["totalGlobalMem"]
if args.working_set_size == None:
    args.working_set_size = global_mem_bytes // 2

if args.working_set_size > global_mem_bytes:
    raise argument.ArgumentTypeError("--working-set-size cannot be more than %d for the selected device (%d)" % global_mem_bytes % args.device);

args.working_set_size = (args.working_set_size // 1024 // 1024) * 1024 *1024;

print("Aligned working set size: " + str(args.working_set_size) + " bytes")

if (args.flop_min > args.flop_max):
    raise argparse.ArgumentTypeError("--flop-min must be <= --flop-max.")

if (args.flop_rate <= 1):
    raise argparse.ArgumentTypeError("--flop-rate must be greater than 1.0")


###############################################################################
# Run kernel and plot the results
###############################################################################
print("")
print("================================================================================")
print("Type         FLOP/elem           AI            BW (GB/s)          Perf (GFLOP/s)")
print("================================================================================")

# set plot properties
plt.figure()
plt.xlabel('Arithmetic Intensity (FLOP/Byte)')
plt.ylabel('Performance (GFLOP/s)')
plt.xscale('log')
plt.yscale('log')
plt.title('Roofline Plot (' + selected_device["name"] + ')')
plt.grid(which='major', axis='both')
plt.grid(which='minor', axis='both', linestyle=':')

# run kernel for single precision floating point
if (args.disable_sp == False):
    sp_ai, sp_bw, sp_perf = run_bench("float", args)
    plt.plot(sp_ai, sp_perf, '-bo', label='Performance (SP)')
    mb_sp = max(sp_perf)/max(sp_bw)
    plt.axvline(x=mb_sp, color='b', linestyle='--', label='Machine Balance (SP)')


# run kernel for double precision floating point
if (args.disable_dp == False):
    dp_ai, dp_bw, dp_perf = run_bench("double", args)
    plt.plot(dp_ai, dp_perf, '-r^', label='Performance (DP)')
    mb_dp = max(dp_perf)/max(dp_bw)
    plt.axvline(x=mb_dp, color='r', linestyle='--', label='Machine Balance (DP)')

plt.legend()

# save the plot
plt.savefig(args.plot_file, format='pdf', bbox_inches='tight')

print("================================================================================")
print("Machine Balance (Single Precision): %0.1f" % mb_sp)
print("Machine Balance (Double Precision): %0.1f" % mb_dp)
print("Roofline plot saved as " + args.plot_file)
print("")








