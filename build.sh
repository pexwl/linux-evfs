cd $(dirname -- $0)

if [ -z "$LINUX_SRC" ]; then
	echo "LINUX_SRC is not set"
	exit 1
fi

rsync -av --exclude={'.*.swp','.*.swo'} linux-6.8/* $LINUX_SRC/.
cd $LINUX_SRC
x=$(expr $(nproc) - 1)
if [ "$x" -le 1 ]; then
	x=1
fi
echo "using $x cores"
make -j$x

