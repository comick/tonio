################################################################################
#
# tonio
#
################################################################################

TONIO_VERSION = 1.0
TONIO_SITE = $(BR2_EXTERNAL_TONIO_PI3_PATH)/package/tonio/tonio-$(TONIO_VERSION)
TONIO_SITE_METHOD = local
TONIO_DEPENDENCIES = host-pkgconf vlc alsa-lib wiringpi libmicrohttpd

define TONIO_BUILD_CMDS
    PKG_CONFIG_PATH="$(STAGING_DIR)/usr/lib/pkgconfig:$(PKG_CONFIG_PATH)" \
        $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define TONIO_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/tonio $(TARGET_DIR)/usr/bin
    mkdir -p $(TARGET_DIR)/usr/share/tonio/www
    $(INSTALL) -D -m 0755 $(@D)/index.html $(TARGET_DIR)/usr/share/tonio/www/index.html
endef

$(eval $(generic-package))
