#!/usr/bin/env sh

input_filename="$1"
base_filename="$(basename -- $input_filename)"
backslash='\\'
invision_notice=$'/*!\n * '"$backslash"'file '"$base_filename"'\n * '"$backslash"'brief '"$base_filename"'\n *\n * Copyright 2020 by InvisionApp.\n *\n * Contact kevinrogovin@invisionapp.com\n */'

echo "$invision_notice" > tmp
cat tmp $input_filename > tmp2
cp tmp2 $input_filename
rm tmp tmp2
