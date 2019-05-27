#!/usr/bin/env bash

usage() { echo "Usage $0 [-d]" 1>&2; exit 1; }

debug=false

while getopts ":d" arg; do
	case $arg in
		d)	echo ""
			echo "****************************************************"
			echo "* Generating debug library. Do not make it public! *"
			echo "****************************************************"
			echo ""
			debug=true
			;;
		h | *)
			usage
			;;
	esac
done

project_file="prj.conf"
project_dir="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
output_dir="output"
cpu_variant="cortex-m33"

if [ "$debug" = true ] ; then
	project_file="$project_file overlay-debug.conf"
fi

declare -a lib_variants=("hard-float"
#			 "soft-float"
#			 "softfp-float"
			)

declare -a lib_names=("nrf_lwm2m"
		      "nrf_lwm2m_vzw")

declare -A lib_paths=([${lib_names[0]}]="lwm2m"
		      [${lib_names[1]}]="lwm2m_vzw")

declare -a api_headers_nrf_lwm2m=("lwm2m_cfg.h"
				  "lwm2m_api.h"
				  "lwm2m_objects.h"
				  "lwm2m_remote.h"
				  "lwm2m_acl.h"
				  "lwm2m_objects_tlv.h"
				  "lwm2m_tlv.h"
				  "lwm2m_objects_plain_text.h"
				  "lwm2m.h")

declare -a api_headers_nrf_lwm2m_vzw=(
				#       "app_debug.h"
				#       "at_interface.h"
				#       "common.h"
				#       "lwm2m_conn_mon.h"
				#       "lwm2m_vzw_main.h"
				#       "lwm2m_device.h"
				#       "lwm2m_firmware.h"
				#       "lwm2m_instance_storage.h"
				#       "lwm2m_os.h"
				#       "lwm2m_retry_delay.h"
				#       "lwm2m_security.h"
				#       "lwm2m_server.h"
				      )

function obfuscate {
	# $1 library name
	# $2 cpu variant
	# $3 float variant
	target_dir=$project_dir/$output_dir/$1/lib/$2/$3
	script_dir=$project_dir/obfuscate
	name=lib$1.a
	obfuscated_name=lib$1_obfuscated.a
	relinked_name=lib$1_relinked.a
	mapping_name=lib$1_mapping.txt

	python $script_dir/obfuscate_symbols.py \
	--input_archive $target_dir/$name \
	--output_archive $target_dir/$obfuscated_name \
	--relinked_archive $target_dir/$relinked_name \
	--mapping_file $target_dir/$mapping_name \
	--symbol_rename_regex_file $script_dir/symbol_filters_$1.txt \
	--section_filter_file $script_dir/section_filters_$1.txt \
	--temp_directory $target_dir/tmp

	# Cleanup
	rm -rf $target_dir/$name
	rm -rf $target_dir/$relinked_name
	rm -rf $target_dir/$mapping_name
	rm -rf $target_dir/tmp

	mv $target_dir/$obfuscated_name $target_dir/$name
}

rm -rf $project_dir/$output_dir

# Build and copy libraries

for i in "${lib_variants[@]}"
do
	echo "Preparing $i"
	rm -rf $project_dir/build-$i
	mkdir $project_dir/build-$i
	cd $project_dir/build-$i

	cmake -GNinja -DBOARD=nrf9160_pca10090ns -DCONF_FILE="$project_file overlay-$i.conf" ..

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

mkdir -p $project_dir/$output_dir/nrf_lwm2m_vzw/include

for i in "${api_headers_nrf_lwm2m_vzw[@]}"
do
	cp $project_dir/../lib/lwm2m_vzw/include/$i $project_dir/$output_dir/nrf_lwm2m_vzw/include
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

# Obfuscate the Verizon LWM2M library

if [ "$debug" = false ] ; then
	for i in "${lib_variants[@]}"
	do
		obfuscate "nrf_lwm2m_vzw" $cpu_variant $i
	done
fi
