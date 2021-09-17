################################################################################
#
# tonio
#
################################################################################

TONIO_VERSION = 1.0
TONIO_SITE = $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/tonio-$(TONIO_VERSION)
TONIO_SITE_METHOD = local
TONIO_DEPENDENCIES = host-pkgconf vlc alsa-lib libgpiod libmicrohttpd libconfuse

define TONIO_BUILD_CMDS
    PKG_CONFIG_PATH="$(STAGING_DIR)/usr/lib/pkgconfig:$(PKG_CONFIG_PATH)" \
        $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define TONIO_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/tonio $(TARGET_DIR)/usr/bin
    mkdir -p $(TARGET_DIR)/usr/share/tonio/www
    $(INSTALL) -D -m 0755 $(@D)/www/index.html $(TARGET_DIR)/usr/share/tonio/www/index.html
    $(INSTALL) -D -m 0755 $(@D)/www/tonio.css $(TARGET_DIR)/usr/share/tonio/www/tonio.css
    $(INSTALL) -D -m 0755 $(@D)/www/tonio.js $(TARGET_DIR)/usr/share/tonio/www/tonio.js
endef

define TONIO_INSTALL_INIT_SYSV
    $(INSTALL) -D -m 0755 $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/S15tonio \
        $(TARGET_DIR)/etc/init.d/S15tonio
endef

$(eval $(generic-package))

tonio-deploy: tonio-rebuild
	ssh root@tonio.local '/etc/init.d/S15tonio stop'
	scp $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/tonio-1.0/www/index.html root@tonio.local:/usr/share/tonio/www/index.html
	scp $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/tonio-1.0/www/tonio.css root@tonio.local:/usr/share/tonio/www/tonio.css
	scp $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/tonio-1.0/www/tonio.js root@tonio.local:/usr/share/tonio/www/tonio.js
	scp $(TARGET_DIR)/usr/bin/tonio root@tonio.local:/usr/bin/tonio
	ssh root@tonio.local '/etc/init.d/S15tonio start'
