#!/usr/bin/env bash

set -e

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf $project_dir/../../nrfxlib/lwm2m_carrier/lib
rm -rf $project_dir/../../nrfxlib/lwm2m_carrier/include

# Remove old ".a" library from "-nrf" repository.
# Needed to make sure the new library is copied. Will fail is the new build is not found.
rm -rf $project_dir/../../nrf/lib/lwm2m_carrier/lib
rm -rf $project_dir/../../nrf/lib/lwm2m_carrier/include

# Remove "tmp-" files generated during the library compilation.
# Do not store "tmp-" files as artifacts.
rm -rf $project_dir/output/tmp-*

cp -r $project_dir/output/* $project_dir/../../nrf/lib/lwm2m_carrier