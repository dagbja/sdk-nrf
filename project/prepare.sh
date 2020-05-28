#!/usr/bin/env bash

set -e

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
cpu_variant="cortex-m33"
output_dir="output"
output_lib="lwm2m_carrier"
temp_dir="temp"

if [ "$debug" = true ] ; then
	project_file="$project_file overlay-debug.conf"
fi

declare -a lib_variants=("hard-float"
#			 "soft-float"
#			 "softfp-float"
			)

declare -a lib_names=("nrf_coap"
		      "nrf_lwm2m"
		      "nrf_lwm2m_carrier")

declare -A lib_paths=([${lib_names[0]}]="coap"
		      [${lib_names[1]}]="lwm2m"
		      [${lib_names[2]}]="lwm2m_carrier")

declare -a api_headers_nrf_lwm2m_carrier=("lwm2m_carrier.h"
				      "lwm2m_os.h")

declare -a api_impl_nrf_lwm2m_carrier=("lwm2m_os.c")

function obfuscate {
	# $1 library name
	# $2 cpu variant
	# $3 float variant
	target_dir=$project_dir/$output_dir/lib/$2/$3
	script_dir=$project_dir/obfuscate
	name=$1.a
	relinked_name=$1_relinked.a
	obfuscated_name=$1_obfuscated.a
	mapping_name=$1_mapping.txt

	python3 $script_dir/obfuscate_symbols.py \
	--input_archive $target_dir/$name \
	--output_archive $target_dir/$obfuscated_name \
	--relinked_archive $target_dir/$relinked_name \
	--mapping_file $project_dir/temp/$mapping_name \
	--symbol_rename_regex_file $script_dir/symbol_filters_$1.txt \
	--section_filter_file $script_dir/section_filters_$1.txt \
	--temp_directory $project_dir/temp/obfuscate

	# Cleanup
	rm -rf $target_dir/$name
	rm -rf $target_dir/$relinked_name

	mv $target_dir/$obfuscated_name $target_dir/$name
}

rm -rf $project_dir/$temp_dir
rm -rf $project_dir/$output_dir

# Build and copy libraries into /temp

for i in "${lib_variants[@]}"
do
	echo "Preparing $i"
	rm -rf $project_dir/build-$i
	mkdir $project_dir/build-$i
	cd $project_dir/build-$i

	cmake -GNinja -DBOARD=nrf9160_pca10090ns -DCONF_FILE="$project_file overlay-$i.conf" ..

	for j in "${lib_names[@]}"
	do
		ninja lib$j.a || exit 1

		mkdir -p $project_dir/$temp_dir/$j/lib/$cpu_variant/$i
		cp $project_dir/build-$i/modules/lwm2m/lib/${lib_paths[$j]}/lib$j.a $project_dir/$temp_dir/$j/lib/$cpu_variant/$i
	done
done

# Merge the libraries into a single archive

for i in "${lib_variants[@]}"
do
	output_path=$project_dir/$output_dir/lib/$cpu_variant/$i
	mkdir -p $output_path

	ar_script="create $output_path/lib$output_lib.a\n"

	for j in "${lib_names[@]}"
	do
		ar_script=$ar_script"addlib $project_dir/$temp_dir/$j/lib/$cpu_variant/$i/lib$j.a\n"
	done

	ar_script=$ar_script"save\nend\n"

	echo -e "$ar_script" | ar -M
done

# Obfuscate whole carrier library

if [ "$debug" = false ] ; then
	for i in "${lib_variants[@]}"
	do
		obfuscate "lib$output_lib" $cpu_variant $i
	done
fi

# Copy headers

mkdir -p $project_dir/$output_dir/include

for i in "${api_headers_nrf_lwm2m_carrier[@]}"
do
	cp $project_dir/../lib/lwm2m_carrier/include/$i $project_dir/$output_dir/include
done

# Copy OS glue

mkdir -p $project_dir/$output_dir/os

for i in "${api_impl_nrf_lwm2m_carrier[@]}"
do
	cp $project_dir/../lib/lwm2m_carrier/src/$i $project_dir/$output_dir/os
done

# TODO This is not needed at the moment, we do not make other headers public
# Replace Kconfig macros with actual values

# autoconf_file="$project_dir/build-${lib_variants[0]}/zephyr/include/generated/autoconf.h"
# target_file="$project_dir/$temp_dir/nrf_lwm2m/include/lwm2m_cfg.h"

# while read -r def conf val
# do
# 	if [[ $conf == CONFIG_NRF_LWM2M_* ]]
# 	then
# 		sed -i "s/$conf/$val/g" $target_file
# 	fi
# done < "$autoconf_file"
