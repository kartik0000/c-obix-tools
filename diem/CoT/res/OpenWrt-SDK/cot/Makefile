# This is a makefile for compiling CoT project for OpenWrt platform.
#
# Author: Andrey Litvinov
# See /COPYING for license information.

include $(TOPDIR)/rules.mk

PKG_NAME:=cot
PKG_VERSION:=0.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=http://c-obix-tools.googlecode.com/files/ \
				http://www.tml.tkk.fi/~litvinov/

include $(INCLUDE_DIR)/package.mk

CONFIGURE_ARGS += \
	--enable-shared \
	--enable-static \

define Package/cot-server
	SECTION:=net
	CATEGORY:=Network
	TITLE:=C oBIX Server
	DEPENDS:=+libcot +fcgi +lighttpd-mod-fastcgi
endef

define Package/cot-adapters
	SECTION:=net
	CATEGORY:=Network
	TITLE:=C oBIX Server
	DEPENDS:=+libcot +libxml2
endef

define Package/libcot
	SECTION:=libs
	CATEGORY:=Libraries
	TITLE:=C oBIX Tools
	DEPENDS:=+libupnp +libcurl
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		DESTDIR="$(PKG_INSTALL_DIR)" \
		CC="$(TARGET_CC)" \
		install
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/cot 
	$(CP) $(PKG_INSTALL_DIR)/usr/include/cot/* $(1)/usr/include/cot	
	$(INSTALL_DIR) $(1)/usr/lib $(1)/usr/lib/pkgconfig
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libcot-{client,utils}.{a,so*} $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/pkgconfig/libcot.pc $(1)/usr/lib/pkgconfig/
endef

define Package/cot-server/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/cot
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/obix.fcgi $(1)/usr/bin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/server_*.xml $(1)/etc/cot/
endef

define Package/cot-adapters/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/cot
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/sensor_floor_obix $(1)/usr/bin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/sensor_floor_obix_config.xml $(1)/etc/cot/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/sensor_floor_pico $(1)/usr/bin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/sensor_floor_pico_config.xml $(1)/etc/cot/
#	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/memtest_adapter $(1)/usr/bin/
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/adapters/max_mem_test.sh $(1)/usr/bin/
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/adapters/max_mem_test_stop.sh $(1)/usr/bin/
#	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/memtest_adapter_config.xml $(1)/etc/cot/
endef

define Package/libcot/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libcot-{client,utils}.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libcot))
$(eval $(call BuildPackage,cot-server))
$(eval $(call BuildPackage,cot-adapters))
