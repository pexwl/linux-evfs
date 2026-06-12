# Extended VFS in Linux

This repository is a development fork of a collaborative codebase built for an academic research project focused on linux VFS development. Its purpose is to reduce the effort in the development of file system management software.

## My Contributions
- Refined block allocation/freeing.
- Fixed consistency issues with directory entry adding/deleting/updating.
- Implemented inode allocation/freeing by inode number;
- Extent swapping in one atomic transaction of one inode.
- bitmap tracker to track blocks/inode allocated within eVFS.
- Built all unit-test scripts for all eVFS features.

## Prerequisites
> Testing on eVFS is currently limited to linux-6.8 on Debian 12 and Ubuntu 22.04

### Debian-based (Debian, Ubuntu)
```
sudo apt-get install libncurses5-dev gcc make git exuberant-ctags bc libssl-dev rsync
```

### RedHat-based (RedHat, Fedora, CentOS)
```
sudo yum install gcc make git ctags ncurses-devel openssl-devel rsync
```

## Building the Kernel
1. Download the linux source archive linux-6.8.0 from [kernel.org](kernel.org). Put the source code archive in ~/dev. (You can put it anywhere you like, but we would use this path for this tutorial)
```sh
cd ~/dev
wget https://www.kernel.org/pub/linux/kernel/v6.x/linux-6.8.tar.gz
```

2. Extract the source code
```sh
tar -xvf linux-6.8.tar.gz
```

3. Make the default configuration once
```sh
cd linux-6.8
make defconfig
```

4. Change your configuration
```sh
make nconfig
```

5. Build the kernel
```sh
make
```
or if you are on a multi-processor machine where X is the cores you want to use
```sh
make -jX
```

6. Install the kernel
> You may need to add `sudo` before the install command if you do not have permission to install the kernel.
```sh
make install
```

Before going in to the next steps, please reboot and check the latest kernel build and installation is successful.

## Installing eVFS into the kernel
1. Clone the source code
```sh
cd ~/dev
git clone https://github.com/pexwl/linux-evfs.git
```

2. Run our script
```sh
cd linux-evfs
sudo chmod u+x build.sh install.sh
export LINUX_SRC=../linux-6.8
./build.sh
./install.sh
```

3. Reboot
```sh
sudo reboot
```

4. Run our testing script (as an example we run `ialloc.sh` which allocates an inode)
```sh
cd ~/dev/evfs/
test/ialloc.sh
```

## Enabling Kernel Debugging
Below are some useful resources you can refer to.
- [**Turning on Kernel Flags**](https://www.kernel.org/doc/html/v6.8/dev-tools/kgdb.html)
- [**Debugging with QEMU**](https://www.redhat.com/zh-tw/blog/debugging-kernel-qemulibvirt)

## References
- [KernelBuild - Linux Kernel Newbies](https://kernelnewbies.org/KernelBuild)

