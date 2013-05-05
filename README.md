ELV Max! Arduino sketch
-----------------------
This sketch allows an Arduino to listen to messages sent by devices in
the ELV Max! heating control system.

The ELV Max! system allows automatic control of radiator valves, but
misses a central control of the boiler, it assumes that the hot water
supply is always on.

This sketch allows the Arduino to listen to the messages from the ELV
Max! devices and switches on the boiler (through a relay) when there is
(sufficient) heat demand in the radiator valves.

It also serves as a debug tool to display all received packets, which
helps to reverse engineer the RF protocol.

Finally, it can log all status updates it receives, so you can use other
tools to draw graphs or keep other statistics.

Debug and logging output is presented over serial, but can also be sent
through TCP when an Arduino Ethernet or Ethernet shield is used.

This tool is still a work in progress.

Compiling
---------
This sketch uses the RF22 library for the RF communication. However, the
RF22 hardware is not perfectly suited to communicate with the ELV Max!,
so it needs some patching. The patched library is available here:

	https://github.com/matthijskooijman/RF22

Furthermore, the sketch uses the TStreaming library to get more consise
printing and formatting, which can be found at:

	https://github.com/matthijskooijman/TStreaming


Finally, the TStreaming library and parts of the sketch are programmed
using new C++ features, from the C++11 standard. This requires the
program to be compiled using the `-std=c++11` gcc option. Since the
Arduino IDE does not support passing custom options to gcc, this sketch
cannot be compiled using the Arduino IDE currently.


The recommended way of compiling this sketch is to use the
Arduino-mk Makefile, available at:

	https://github.com/mjoldfield/Arduino-Makefile

This sketch ships a Makefile that includes files from the above
repository to do its work. It assumes you have a `$(ARDMK_DIR)` variable
in your shell that points to a checkout of the above repository.

If you have that, run `make` to compile the sketch, `make size` to get a
memory usage report and `make upload` to upload the sketch.

Known devices
-------------
Inside MaxRFProto.cpp, there is a hardcoded list of known devices, of
which state is kept. Leaving the list empty will just add any devices
when a message from or to them is reveived (up to a number of devices
hardcoded in MaxRFProto.h). Adding devices to the list helps to give
them a name and let the code know about the device type (which cannot
always be determined automically).
