if [ $# -lt 1 ]; then
 echo "Missing 1 positional arguments: version"
 exit 1
fi

set -e

mkdir -p builds 
make version=$1
make clean
./builds/$1
