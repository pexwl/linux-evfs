cd $(dirname -- $0)
cd ../linux-6.8/
sudo make headers_install ARCH=arm64 INSTALL_HDR_PATH=/usr
sudo make install
