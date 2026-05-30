cd $(dirname -- $0)

if [ -z "$LINUX_SRC" ]; then
	echo "LINUX_SRC is not set"
	exit 1
fi

cd $LINUX_SRC
sudo make headers_install INSTALL_HDR_PATH=/usr
sudo make install
