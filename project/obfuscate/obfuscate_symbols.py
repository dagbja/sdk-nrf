#!/usr/bin/env/python

"""
Copyright (c) 2018 - 2019 Nordic Semiconductor ASA. All Rights Reserved.

The information contained herein is confidential property of Nordic Semiconductor ASA.
The use, copying, transfer or disclosure of such information is prohibited except by
express written agreement with Nordic Semiconductor ASA.
"""

# Obfuscate symbols in elf file to make reverse engineering harder.
# Depends on both GNU tools and armlink.
import argparse
import os
import random
import re
import subprocess
import shutil
from time import sleep


def retry_command(*subprocess_args, **subprocess_kwargs):
    """ Retry command up to 10 times as this may depend on processes which are slow to finish on certain build on the build server """
    for i in range(10):
        retval = subprocess.call(*subprocess_args, **subprocess_kwargs)
        if not retval:
            break
        print("Sleep and retry, try: {0}; Retval: {1}".format(i, retval))
        sleep(1)

    if retval:
        raise subprocess.CalledProcessError(retval, subprocess_args)


# Relinks all object files into one archive
def relink_library(input_archive_name, relinked_elf, relinked_library_name, temp_directory):
    # Extract object files
    retry_command(['arm-none-eabi-ar', '-xc', os.path.abspath(input_archive_name)], cwd=temp_directory)

    # Get object file names directly to avoid having to use shell substitution
    objectfiles = [os.path.join(temp_directory, filename) for filename in os.listdir(temp_directory) if os.path.isfile(os.path.join(temp_directory, filename)) and filename[-4:] == ".obj"]

    # Link object files into one elf file
    retry_command(['arm-none-eabi-ld', '-o', relinked_elf, '-r', '-x'] + objectfiles, cwd=temp_directory)

    if os.path.exists(relinked_library_name):
        os.remove(relinked_library_name)

    # Create archive
    retry_command(['arm-none-eabi-ar', 'rs', os.path.abspath(relinked_library_name), relinked_elf], cwd=temp_directory)


def make_parse_nm_symbols_args(symbol_list_output):
    """ This function extracts a list of all symbols in the library file from the output of nm.
        The format is described in the nm manpage """
    orig_symbols = []
    for line in symbol_list_output.split('\n'):
        entry = line.split()
        if len(entry) == 4:
            # Text segments: ['sd_radio_request', 'T', '00000384', '00000056']
            # Data segments: ['m_hal_aar', 'D', '00000000', '00000003']
            # Zero-initialized data segments: ['m_peer_list', 'B', '00000000', '000001d3']
            orig_symbols.append(entry[0])
            assert( entry[1] == 'T' or 'D' or 'B' )
        elif len(entry) == 2:
            # Weak symbols:['test_current_time_capture', 'w'] or ['Lib$$Request$$armlib', 'w']
            # Undefined symbols: ['blectlr_assertion_handler', 'U']
            orig_symbols.append(entry[0])
            assert( entry[1] == 'w' or 'U' )
        elif len(entry) == 3:
            # local text segments: ['t', '00000000', '00000018']
            # local data segments: ['b', '0000159b', '00000043']
            assert ( entry[0] == 'b' or 't' )
        else:
            # Empty line
            assert( len(entry) == 0 )
    return orig_symbols


def objcopy_rewrite_cmd_elf(section_filters, mapping_file, stripped_archive, obfuscated_elf):
    # Invoke objcopy to rename symbols and remove sections and symbols
    objcopy_args = ["arm-none-eabi-objcopy"]

    for section_filter in section_filters:
        objcopy_args.append('--remove-section={0}'.format(section_filter))

    objcopy_args.extend(['--redefine-syms={0}'.format(os.path.abspath(mapping_file)),
                         stripped_archive, obfuscated_elf])

    return objcopy_args

def obfuscate(temp_directory, input_archive, output_archive, relinked_archive, symbol_rename_regex_file, section_filter_file, mapping_file, armlink_path):
    # Clean and remake directory
    if os.path.exists(temp_directory):
        shutil.rmtree(temp_directory)
    os.mkdir(temp_directory)
    if os.path.exists(output_archive):
        os.remove(output_archive)
    if os.path.exists(relinked_archive):
        os.remove(relinked_archive)

    relinked_elf = os.path.join(temp_directory, "relinked.elf")
    stripped_archive = os.path.join(temp_directory, "stripped.a")
    obfuscated_elf = os.path.join(temp_directory, "obfuscated.elf")

    # Relink, this links all the object files in archive to one big object file
    relink_library(input_archive, relinked_elf, relinked_archive, temp_directory)

    if armlink_path:
        partial_linked = os.path.join(temp_directory, "partial_linked.a")
        # Get rid of the arm memcpys by partial linking - this uses armlink, but is only relevant if arm compiler was used in the first place
        retry_command([armlink_path, '-o', partial_linked, '--partial', '--privacy', relinked_elf], cwd=temp_directory)
    else:
        partial_linked = relinked_elf

    # Strip away debug symbols
    retry_command(['arm-none-eabi-strip', '-o', stripped_archive, partial_linked, '--discard-all'], cwd=temp_directory)

    # List global symbols in archive and parse them to a list
    symbol_list_output = subprocess.check_output(['arm-none-eabi-nm', '-f', 'posix', stripped_archive], cwd=temp_directory).decode("utf-8")
    orig_symbols = make_parse_nm_symbols_args(symbol_list_output)

    # Read symbol filter regular expressions, one per line (to filter out symbol names to keep), from file
    symbol_filters = None
    with open(symbol_rename_regex_file, 'rb') as rfh:
        symbol_filters = [re.compile(line.strip()) for line in rfh.read().decode("utf-8").split("\n") if len(line.strip()) and not line[0] == "#"]

    # Generate dict for symbol renaming
    rename_symbols = [symbol for symbol in orig_symbols if not any([sfilter.match(symbol) for sfilter in symbol_filters])]
    # Do not go wild here, a very high upper bound might crash some environments
    symbol_indices = random.sample(range(1, 0x100000), len(rename_symbols))
    renamed_symbols = {symbol: "liblwm2m_carrier_symbol_{:07x}".format(i) for i, symbol in zip(symbol_indices, rename_symbols)}

    # Create edit symbol rename file for objcopy
    with open(mapping_file, 'wb') as wfh:
        for old_name, new_name in renamed_symbols.items():
            wfh.write("{0} {1}\n".format(old_name, new_name).encode("utf-8"))

    # Create list of sections to remove for objcopy
    with open(section_filter_file, 'rb') as rfh:
        section_filters = [line.strip() for line in rfh.read().decode("utf-8").split("\n") if len(line.strip()) and not line[0] == "#"]

    # Invoke objcopy to rename symbols and remove sections and symbols
    objcopy_args = objcopy_rewrite_cmd_elf(section_filters,  mapping_file, stripped_archive, obfuscated_elf)
    retry_command(objcopy_args, cwd=temp_directory)

    # Embed the elf-file into an archive using ar to be able to link to it
    # Make sure to delete any preexisting .a files as ar only updates them and may leave old stuff laying around
    if os.path.exists(output_archive):
        os.remove(output_archive)

    retry_command(['arm-none-eabi-ar', 'rs', os.path.abspath(output_archive), obfuscated_elf], cwd=temp_directory)

    # Sleep a bit to avoid concurrency issues related to files on windows
    sleep(.5)

    # Clean up temp directory
    shutil.rmtree(temp_directory)

    # Check if the archive is broken (objdump will return nonzero), and we should retry obfuscate
    proc = subprocess.Popen(["arm-none-eabi-objdump","-a", output_archive], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc.communicate()
    retval = proc.returncode
    return retval

def main():
    parser = argparse.ArgumentParser(description='AR/ELF Symbol obfuscation script')
    parser.add_argument('-i', '--input_archive', required=True)
    parser.add_argument('-o', '--output_archive', required=True)
    parser.add_argument('-l', '--relinked_archive', required=True)
    parser.add_argument('-r', '--symbol_rename_regex_file', required=True)
    parser.add_argument('-s', '--section_filter_file', required=True)
    parser.add_argument('-m', '--mapping_file', required=True)
    parser.add_argument('-t', '--temp_directory', required=True)
    parser.add_argument('-a', '--armlink_path', required=False)
    args = parser.parse_args()

    # Try obfuscating a few times, some times this fails due to concurrency issues. Then a retry
    # should be attempted to avoid breaking the build
    for i in range(10):
        retval = obfuscate(
                            args.temp_directory,
                            args.input_archive,
                            args.output_archive,
                            args.relinked_archive,
                            args.symbol_rename_regex_file,
                            args.section_filter_file,
                            args.mapping_file,
                            args.armlink_path
                          )
        if not retval:
            break
        print("Archive broken, sleep and retry obfuscate: {0}; Retval: {1}".format(i, retval))
        sleep(3)


if __name__ == "__main__":
    main()
