#!/usr/bin/env bash

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf $project_dir/../../nrfxlib/lwm2m/lib
rm -rf $project_dir/../../nrfxlib/lwm2m/include

cp -r $project_dir/output/lib $project_dir/../../nrfxlib/lwm2m
cp -r $project_dir/output/include $project_dir/../../nrfxlib/lwm2m
