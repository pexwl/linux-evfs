cd $(dirname -- $0)
cd ../linux-6.8/
sudo make headers_install INSTALL_HDR_PATH=/usr
sudo make install
