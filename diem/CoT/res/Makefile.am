## Process this file with automake to produce Makefile.in

## Installation folder for all configuration files
## In common case it will be resolved to /etc/cot/
pkgsysconfdir	= $(sysconfdir)/$(PACKAGE_NAME)

## Data files for server
dist_pkgsysconf_DATA  	=   obix-lighttpd.conf \
							server_about.xml \
							server_config.xml \
							server_devices.xml \
							server_lobby.xml \
							server_sys_objects.xml \
							server_watch.xml \
							server_def.xml \
							server_test_device.xml
			   
## Data files for adapters
dist_pkgsysconf_DATA	+=  sensor_floor_config.xml \
							example_timer_config.xml

## Other data			   
dist_check_DATA			=   test_devices.xml \
							test_obix_client_config.xml
							
EXTRA_DIST				=   OpenWrt-SDK/README \
							OpenWrt-SDK/cot/Makefile \
							OpenWrt-SDK/lwl/Makefile							