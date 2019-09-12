#!/usr/bin/env bash

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf $project_dir/../../nrfxlib/lwm2m_carrier/lib
rm -rf $project_dir/../../nrfxlib/lwm2m_carrier/include

cp -r $project_dir/output/* $project_dir/../../nrf/lib/lwm2m_carrier
