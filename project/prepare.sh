#!/usr/bin/env bash

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
lib_name="libnrf_lwm2m.a"
output_dir="output"
cpu_variant="cortex-m33"
declare -a lib_variant=("hard-float"
			"soft-float"
			"softfp-float")

rm -rf $project_dir/$output_dir

# Build and copy libraries
for i in "${lib_variant[@]}"
do
	echo "Preparing $i"
	rm -rf $project_dir/build-$i
	mkdir $project_dir/build-$i
	cd $project_dir/build-$i

	cmake -GNinja -DBOARD=nrf9160_pca10090ns -DOVERLAY_CONFIG=overlay-$i.conf ..
	ninja $lib_name

	mkdir -p $project_dir/$output_dir/lib/$cpu_variant/$i
	cp $project_dir/build-$i/zephyr/lwm2m/lib/lwm2m/$lib_name $project_dir/$output_dir/lib/$cpu_variant/$i/$lib_name
done

# Copy headers
declare -a api_headers=("lwm2m_cfg.h"
			"lwm2m_api.h"
			"lwm2m_objects.h"
			"lwm2m_remote.h"
			"lwm2m_acl.h"
			"lwm2m_objects_tlv.h"
			"lwm2m_tlv.h"
			"lwm2m_objects_plain_text.h"
			"lwm2m.h")

mkdir mkdir -p $project_dir/$output_dir/include

for i in "${api_headers[@]}"
do
	cp $project_dir/../lib/lwm2m/include/$i $project_dir/$output_dir/include
done

# Replace Kconfig macros with acual values
autoconf_file="$project_dir/build-${lib_variant[0]}/zephyr/include/generated/autoconf.h"
target_file="$project_dir/$output_dir/include/lwm2m_cfg.h"

while read -r def conf val
do
	if [[ $conf == CONFIG_NRF_LWM2M_* ]]
	then
		sed -i "s/$conf/$val/g" $target_file
	fi
done < "$autoconf_file"
