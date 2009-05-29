# This is a makefile for compiling LWL library for OpenWrt platform.
#
# Author: Andrey Litvinov
# See /COPYING for license information.

include $(TOPDIR)/rules.mk

PKG_NAME:=lwl
PKG_VERSION:=1.1
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://download.gna.org/lwl/

include $(INCLUDE_DIR)/package.mk

define Package/lwl
	SECTION:=libs
	CATEGORY:=Libraries
	TITLE:=The Log Writer Library
	URL:=http://home.gna.org/lwl/
endef

define Package/lwl/description
 The Log Writer Library (LWL) is a collection of routines for writing 
 logs messages to files from C programms. It is a portable and 
 re-entrant library fully written from scratch in pure ANSI C.
endef

CONFIGURE_ARGS += \
	--enable-shared \
	--enable-static \

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		DESTDIR="$(PKG_INSTALL_DIR)" \
		CC="$(TARGET_CC)" \
		install
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_INSTALL_DIR)/usr/include/lwl.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/liblwl.{a,so*} $(1)/usr/lib/
endef

define Package/lwl/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/liblwl.so.* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,lwl))