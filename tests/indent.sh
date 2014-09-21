#!/bin/sh

case $# in
	0)
		source_dir="."
		;;
	1)
		source_dir=$1
		;;
	*)
		echo "usage: $0 [source_dir]"
		exit 1
		;;
esac

ret=0
find "$source_dir" -name "*.c" -o -name "*.h" | while read f; do
	case $f in
		${source_dir}/*/CMakeFiles/*)
			;;
		${source_dir}/CMakeFiles/*)
			;;
		*)
			INDENT_PROFILE="${source_dir}/.indent.pro" indent -st "$f" | diff -u "$f" -
			ret=$((ret+$?))
			;;
	esac
done

exit $ret