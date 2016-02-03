C oBIX Tools (CoT) is an open source project dedicated for the development of embedded home and building automation solutions based on the [oBIX standard](http://www.obix.org). The standard defines Web services interface and XML-based data model for distributing building automation data.

The whole project is written in C and has tiny resource requirements. It allows the project to run on embedded Linux platforms, such as [OpenWrt](http://openwrt.org). Therefore, CoT can be used for integration of the oBIX interface into an embedded automation device.

The project consists of 2 parts:
  * [C oBIX Server](#C_oBIX_Server.md) and
  * [C oBIX Client library](#C_oBIX_Client_Library.md)

# How It Works #

The architecture of an automation system, which can be created based on CoT project is shown on the figure below. All field devices are connected to the central [server](#C_oBIX_Server.md). They use oBIX interface to publish information about current device state, its sensor readings and available configuration parameters. User applications use the same oBIX interface to access device information at the server. Thus, the server itself acts only as a data storage compliant with oBIX standard. This makes the solution completely device independent.

<img src='http://www.tml.tkk.fi/~litvinov/COT-architecture.png' align='right' alt='System Model' />

Of course, devices, which do not support oBIX protocol, would require special adapters to convert device specific protocol into oBIX. These adapters can be easily implemented using [C oBIX Client library](#C_oBIX_Client_Library.md). It does not matters whether such adapter is running on the same physical device with the oBIX server, or on any other machine, e.g., close to the device it controls. oBIX messages can be transferred over any existing TCP/IP network channel, including Wi-Fi or general purpose LAN cable. Therefore, the system allows to reduce the amount of device specific cabling.

The project can be used to build a home automation system with Web interface. In that case, oBIX server should be connected to the Internet and then any oBIX-compatible UI application can be used to manage connected devices (examples of such applications to be added here). Alternatively, the project can act as a module in a bigger building automation system. oBIX protocol can be used for communication with other parts of the system.

# C oBIX Server #

C oBIX Server is a stand-alone application intended for storing building automation data from various sources. It is a core for automation systems, which provides unified Web services interface to control connected devices (security sensors, heating/ventilation systems, lights, switches, etc.). The same interface is used also to connect devices.

The main difference of this oBIX server from other available implementations, like [oFMS](http://www.stok.fi/eng/ofms/index.html) or [oX](http://obix-server.sourceforge.net) is that it is written in C and thus can be run on cheap low-resource platforms.

The whole project was developed with the main idea to minimize resource consumption and make the implementation as simple as possible. Thus, the server implements only basic oBIX functionality. If you are not going to run the server on embedded device than you may consider using another more complete oBIX implementation.
Here is the list of currently implemented oBIX features:
  * Read, write and invoke requests handling;
  * Lobby object;
  * Full implementation of WatchService;
  * Batch operation;
  * HTTP protocol binding.
Things that are _**not**_ yet supported:
  * Alarms;
  * Feeds;
  * Histories;
  * Writable points (simple writable objects can be used instead);
  * Permission based degradation;
  * Localization;
  * SOAP binding.
If some feature doesn’t appear in either of the lists above, most probably it is not implemented.

In addition to standard oBIX functionality, the server has the following additional features:
  * signUp operation, accessible from the Lobby object. It allows clients (device adapters) to publish new data at the server.
  * Device list, showing all devices connected to the server. The list is also available from the Lobby object.
  * [Long polling](http://c-obix-tools.googlecode.com/files/long_polling.pdf) mode for Watch objects. This mode allows clients to reduce amount of poll requests and in the same time receive immediate updates from the server.
An example of running server can be found at http://testbed.tml.hut.fi/obix/.

# C oBIX Client Library #

C oBIX Client Library (libcot) is intended mainly for the development of oBIX device adapters. It hides details of communication with oBIX server, providing a simple API to publish, read and write data at the server. The library should be compatible with any other oBIX server implementations (if not – please, report the bug). Together with C oBIX Server, the libcot library forms a complete framework for rapid development of embedded home or building automation server.

The library allows to perform the following oBIX actions:
  * Read objects from oBIX server;
  * Change object values at the server;
  * Publish new data to the server (using signUp operation);
  * Subscribe to data updates at the server. From the developer’s point of view it happens as a simple callback function registration, while library deals with oBIX Watch objects management.
  * Grouping several read and write operations in one Batch request.

In addition, library contains a set of utilities, which can be useful during oBIX client application development (e.g., timer implementation, logging utilities, etc.). Several examples in the distribution show how all these APIs can be used.

Library documentation is available online [here](http://www.tml.tkk.fi/~litvinov/cot-doc/libcot/index.html). Offline version can be either taken from the [Download](http://code.google.com/p/c-obix-tools/downloads/list) section or compiled from project sources (see building instructions for details).

# Project Requirements #

The project is created for running on Linux platforms. It was tested on various platforms including embedded devices with [OpenWrt](http://openwrt.org) installed. An example of tested embedded platform is Asus WL-500g Premium V2 router (32 MB of RAM and 240 MHz CPU). Any other device capable to run OpenWrt can be used as well.

Other Linux distributions for embedded devices were not tested but can be possibly used if all project dependencies are satisfied.

The project has the following library dependencies:
  * libfcgi
  * libupnp
  * libcurl

C oBIX Server is implemented as a [FastCGI](http://www.fastcgi.com) application and requires a Web server with FastCGI support. It was tested only with [Lighttpd](http://www.lighttpd.net/), but theoretically, it can be used with any other Web server. The only requirement is that the chosen Web server should support FastCGI multiplexing feature (ability to handle multiple requests simultaneously through one FastCGI connection), which is not supported, for instance, by Apache. Additional instructions on compiling and configuring the oBIX server can be found at README file in the project root folder.

OpenWrt SDK is required in order to cross-compile the project for OpenWrt platform. Further instructions on cross-compiling can be found at ./res/OpenWrt-SDK/README file. Precompiled binaries for OpenWrt 7.09 are available at [Download](http://code.google.com/p/c-obix-tools/downloads/list) section.

Further information on project compilation, installation and configuring can be found in [README](http://code.google.com/p/c-obix-tools/source/browse/trunk/diem/CoT/README) file of project distribution.