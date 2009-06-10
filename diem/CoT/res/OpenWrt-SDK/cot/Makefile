# This is a makefile for compiling CoT project for OpenWrt platform.
#
# Author: Andrey Litvinov
# See /COPYING for license information.

include $(TOPDIR)/rules.mk

PKG_NAME:=cot
PKG_VERSION:=0.0.0
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://www.tml.tkk.fi/~litvinov/

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
	DEPENDS:=+libcot
endef

define Package/libcot
	SECTION:=libs
	CATEGORY:=Libraries
	TITLE:=C oBIX Tools
	DEPENDS:=+libupnp +libcurl +lwl
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
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libcot-{client,utils}.{a,so*} $(1)/usr/lib/
endef

define Package/cot-server/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/cot
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/obix.fcgi $(1)/usr/bin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/server_*.xml $(1)/etc/cot/
endef

define Package/cot-adapters/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/cot
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/sensor_floor_driver $(1)/usr/bin/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/res/sensor_floor_config.xml $(1)/etc/cot/
endef

define Package/libcot/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libcot-{client,utils}.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libcot))
$(eval $(call BuildPackage,cot-server))
$(eval $(call BuildPackage,cot-adapters))
