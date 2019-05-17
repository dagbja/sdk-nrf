#!/usr/bin/env bash

project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
output_dir="output"
cpu_variant="cortex-m33"

declare -a lib_variants=("hard-float"
			 "soft-float"
			 "softfp-float")

declare -a lib_names=("nrf_lwm2m"
		      "nrf_vzw_lwm2m")
declare -A lib_paths=([${lib_names[0]}]="lwm2m"
		      [${lib_names[1]}]="vzw_lwm2m")

declare -a api_headers_nrf_lwm2m=("lwm2m_cfg.h"
				  "lwm2m_api.h"
				  "lwm2m_objects.h"
				  "lwm2m_remote.h"
				  "lwm2m_acl.h"
				  "lwm2m_objects_tlv.h"
				  "lwm2m_tlv.h"
				  "lwm2m_objects_plain_text.h"
				  "lwm2m.h")

declare -a api_headers_nrf_vzw_lwm2m=("common.h"
				      "lwm2m_conn_mon.h"
				      "lwm2m_device.h"
				      "lwm2m_firmware.h"
				      "lwm2m_instance_storage.h"
				      "lwm2m_retry_delay.h"
				      "lwm2m_security.h"
				      "lwm2m_server.h")

rm -rf $project_dir/$output_dir

# Build and copy libraries

for i in "${lib_variants[@]}"
do
	echo "Preparing $i"
	rm -rf $project_dir/build-$i
	mkdir $project_dir/build-$i
	cd $project_dir/build-$i

	cmake -GNinja -DBOARD=nrf9160_pca10090ns -DOVERLAY_CONFIG=overlay-$i.conf ..

	for j in "${lib_names[@]}"
	do
		ninja lib$j.a

		mkdir -p $project_dir/$output_dir/$j/lib/$cpu_variant/$i
		cp $project_dir/build-$i/zephyr/lwm2m/lib/${lib_paths[$j]}/lib$j.a $project_dir/$output_dir/$j/lib/$cpu_variant/$i
	done
done

# Copy headers

mkdir -p $project_dir/$output_dir/nrf_lwm2m/include

for i in "${api_headers_nrf_lwm2m[@]}"
do
	cp $project_dir/../lib/lwm2m/include/$i $project_dir/$output_dir/nrf_lwm2m/include
done

mkdir -p $project_dir/$output_dir/nrf_vzw_lwm2m/include

for i in "${api_headers_nrf_vzw_lwm2m[@]}"
do
	cp $project_dir/../lib/vzw_lwm2m/include/$i $project_dir/$output_dir/nrf_vzw_lwm2m/include
done

# Replace Kconfig macros with acual values

autoconf_file="$project_dir/build-${lib_variants[0]}/zephyr/include/generated/autoconf.h"
target_file="$project_dir/$output_dir/${lib_names[0]}/include/lwm2m_cfg.h"

while read -r def conf val
do
	if [[ $conf == CONFIG_NRF_LWM2M_* ]]
	then
		sed -i "s/$conf/$val/g" $target_file
	fi
done < "$autoconf_file"
