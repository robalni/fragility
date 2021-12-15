tools=`dirname $0`
name='make-project-files'
cc -O1 $tools/$name.c -o $tools/$name && $tools/$name "$@"
