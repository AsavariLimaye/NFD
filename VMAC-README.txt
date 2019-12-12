***********************************
Installing VMAC Headers and Library
***********************************

1.To install the real vmac library if installing on Raspberry Pi:

Copy the real vmac.a library into the folder vmac-library in the NFD repository.
Execute the following commands:

cd vmac-library
sudo cp vmac.h /usr/include/vmac/
cp vmac.a libvmac.a
rm *.o
ar -x vmac.a
gcc -shared -o libvmac.so *.o
sudo cp libvmac.so /usr/lib
sudo cp libvmac.a /usr/lib

2. To install a dummy vmac library if not installing on RaspberryPi:

cd vmac-library
sudo cp vmac.h /usr/include/vmac
rm *.o
gcc -c vmac.c
ar rcs libvmac.a *.o
gcc -shared -o libvmac.so *.o
sudo cp libvmac.so /usr/lib
sudo cp libvmac.a /usr/lib

***********************************
Installing VMAC Kernel Modules
***********************************
vmac-11-11-19
Contains the vmac kernel modules. Follow the instructions in the VMAC document to install them.
This does not need to be done if dummy vmac library is being used.

***********************************
Setting up SSH to RPis (Windows):
***********************************
RPi's are named rpi1 and rpi2
Username is pi for both
Password for both is password

Open "Control Panel"->"Network and Internet"->"Network and Sharing Center"->"Change Adapter Settings"
Right click on Wi-Fi, "Properties"->"Sharing", and check both boxes.
Choose "Ethernet" to share with
Check "ping rpi1"/"ping rpi2"
"ssh pi@rpi1"
Password: password

***********************************
Setting up Internet Access (Windows):
***********************************
Disconnect LAN cable, RPi switched off
Connect WiFi interface to the network
Disable sharing for the WiFi network
For BOTH WiFi and ethernet:
	Right click -> Properties ->
	Double Click IPv4 -> Set IP address and DNS server to AUTOMATICALLY
Select both WiFi and Ethernet interface
Right click with mouse on ethernet interface
Create Bridge, Wait till Bridge had Wifi network's name
Once bridge is ready, right click on bridge and make sure both Ethernet and Wifi are checked
Now connect LAN cable, switch on RPi
Run ipconfig and find address range of bridge
Used advanced IP Scanner to find other hosts on this network
Use the RPi's IP ADDRESS to ssh with username pi and password "password"
From the RPi, "ping 8.8.8.8"

***********************************
Building NFD and ndn-cxx on RPi
***********************************
Install ndn-cxx and NFD following the instructions:
https://named-data.net/doc/NFD/current/INSTALL.html
and
https://named-data.net/doc/ndn-cxx/current/INSTALL.html

Common Issues:
Code is based on latest master branch of ndn-cxx and NFD
If it does not compile:
1. Delete /usr/local/include/ndn-cxx
2. Delete /usr/local/lib/libndn-*
3. Re-build and install ndn-cxx

Use -j1 while building on RPi
Increase swap space on RPi to 1024 MB instead of 100 MB
/etc/dphys-swapfile, CONF_SWAPSIZE=1024

***********************************
Building NFD with VMAC
***********************************
Clone the repositiory:
https://github.com/AsavariLimaye/NFD.git.
Switch to branch dev-vmac-3

Make sure the latest master branch of ndn-cxx is installed.
Install the vmac library and header.
If compiling on RPi, make sure the kernel modules have been installed by running install.sh.

./waf configure --with-vmac --with-tests
./waf
sudo ./waf install

Run with -j1 to compile on RPis.

To run tests:
./build/unit-tests-daemon -t Face/TestVmac -l all
./build/unit-tests-daemon -t Face/TestVmacLinkService -l all

If nfd with VMAC is runnin on the other RPi, messages about receiving a valid interest will be seen.
Make sure log level is set to DEBUG in NFD config file.

***********************************
Building ndn-cxx with VMAC
***********************************
Clone the repositiory:
https://github.com/AsavariLimaye/ndn-cxx.git
Switch to branch dev-vmac

Install the vmac library and header.
If compiling on RPi, make sure the vmac kernel modules are installed by running install.sh.

./waf configure --with-vmac --with-examples
./waf
sudo ./waf install

Run with -j1 to compile on RPis.

To run the examples:

sudo env NDN_LOG="ndn.VmacTransport=DEBUG" ./build/examples/vmac-consumer

If nfd with VMAC is runnin on the other RPi, messages about receiving a valid interest will be seen.
Make sure log level is set to DEBUG in NFD config file.

If ndn-cxx with VMAC has been installed, NFD will not run correctly on it. Install master branch of ndn-cxx if NFD is to be used.
