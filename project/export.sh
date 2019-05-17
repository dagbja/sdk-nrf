#!/usr/bin/env bash

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf $project_dir/../../nrfxlib/lwm2m/nrf_lwm2m
rm -rf $project_dir/../../nrfxlib/lwm2m/nrf_vzw_lwm2m

cp -r $project_dir/output/nrf_lwm2m $project_dir/../../nrfxlib/lwm2m
cp -r $project_dir/output/nrf_vzw_lwm2m $project_dir/../../nrfxlib/lwm2m
