echo "Building target" $1
docker run --rm -v ${PWD}:/config -it doskoi/ravenlrs "TARGET=$1 make"