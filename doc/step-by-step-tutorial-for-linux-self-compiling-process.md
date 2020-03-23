# SELF-COMPILING PIVX CORE WALLET FOR UBUNTU 18.04

These are detailed step-by-step instructions for beginners on how to self-compile PIVX Core Wallet on Ubuntu Linux 18.04 directly from Master branch. Even if you are using Windows or any other operating system, you can run Ubuntu 18.04 on Virtual Machine. This tutorial will not cover Virtal Machine setup, but you can follow this detailed tutorial on how to setup Ubuntu 18.04 using VirtualBox:
[YouTube step by step tutorial](https://www.youtube.com/watch?v=QbmRXJJKsvs)

This tutorial is intended for anyone who is willing to help with testing of latest features on **testnet** that PIVX Core Developers are planning to implement on PIVX mainnet. If you find any bugs, please report them directly on PIVX Github:

https://github.com/PIVX-Project/PIVX/issues

**NOTE:** I wouldn't recommend doing this on the same machine where you are running your main PIVX wallet, not to break something and lose/damage your wallet.dat file. If you are doing it, please make sure that you have backup of your wallet.dat file safely stored somewhere before starting this. It's safer to run it on a different machine or Virtual Machine.

---------------------------------------------------------

## Let's start...

Open the terminal (command line) on Ubuntu 18.04. Type in the commands line by line as following:

```
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install build-essential libtool bsdmainutils autotools-dev autoconf pkg-config automake python3 -y
sudo apt-get install libssl-dev libgmp-dev libevent-dev libboost-all-dev software-properties-common -y
sudo add-apt-repository ppa:pivx/pivx
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install libdb4.8-dev libdb4.8++-dev libminiupnpc-dev libzmq3-dev -y
sudo apt-get install libqt5gui5 libqt5core5a libqt5dbus5 libqt5svg5-dev libqt5charts5-dev qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev -y
git clone https://github.com/pivx-project/pivx.git
cd pivx
./autogen.sh
./configure
make
sudo make install
./pivx-qt --testnet &
```
--------------------------------------------
**Congratulations, you have successfully compiled and started PIVX Qt Core Wallet!**

**That's it, play around with the latest version of PIVX Core Wallet directly compiled from master branch!**

**NOTE:** If you are testing the wallet through terminal (command line) only, to start the wallet use the `./pivxd --testnet` command instead of the last line (`./pivx-qt --testnet`).

---------------------------------------------
## COMPILING THE WALLET FOR THE FIRST TIME

Open the Terminal (command line) and type line by line:

```
git clone https://github.com/pivx-project/pivx.git PIVX
cd PIVX
./autogen.sh && ./configure
make -j<threads_num> 
./src/qt/pivx-qt --testnet &
```

## COMPILING WHEN YOU ALREADY HAVE THE WALET

Inside PIVX directory:
```
git pull origin master
make -j<threads_num>
./src/qt/pivx-qt --testnet &
```

Extra information:

The <threads_num> needs to be less than the amount of processors, otherwise it will slow down the process.

#### **Congratulations, you have successfully compiled and started FRESH PIVX Qt Core Wallet!**
