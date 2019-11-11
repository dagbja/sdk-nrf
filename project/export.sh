#!/usr/bin/env bash

set -e

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

lib_out_dir="${project_dir}/../../nrf/lib/bin/lwm2m_carrier"

# Create output folder if it does not exist.
mkdir -p $lib_out_dir

# Remove old ".a" library from "-nrf" repository.
# Needed to make sure the new library is copied. Will fail is the new build is not found.
rm -rf $lib_out_dir/lib
rm -rf $lib_out_dir/include

# Remove "tmp-" files generated during the library compilation.
# Do not store "tmp-" files as artifacts.
rm -rf $project_dir/output/tmp-*

cp -r $project_dir/output/* $lib_out_dir