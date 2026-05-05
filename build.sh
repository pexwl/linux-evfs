cd $(dirname -- $0)

rsync -av --exclude='.*.swp' linux-6.8/* ../linux-6.8/.
cd ../linux-6.8/
x=$(expr $(nproc) - 1)
if [ "$x" -le 1 ]; then
	x=1
fi
echo "using $x cores"
make -j$x

