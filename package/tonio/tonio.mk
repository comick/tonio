################################################################################
#
# tonio
#
################################################################################

TONIO_VERSION = 1.0
TONIO_SITE = $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/tonio-$(TONIO_VERSION)
TONIO_SITE_METHOD = local
TONIO_AUTORECONF = YES
TONIO_DEPENDENCIES = host-pkgconf vlc alsa-lib libgpiod libmicrohttpd libconfuse

define TONIO_INSTALL_INIT_SYSV
    $(INSTALL) -D -m 0755 $(BR2_EXTERNAL_TONIO_PATH)/package/tonio/S15tonio \
        $(TARGET_DIR)/etc/init.d/S15tonio
endef

$(eval $(autotools-package))

tonio-deploy: tonio-rebuild
	ssh root@$(target) '/etc/init.d/S15tonio stop'
	scp -O $(TONIO_SITE)/www/index.html root@$(target):/usr/share/tonio/www/index.html
	scp -O $(TONIO_SITE)/www/tonio.css root@$(target):/usr/share/tonio/www/tonio.css
	scp -O $(TONIO_SITE)/www/*.js root@$(target):/usr/share/tonio/www/
	scp -O $(TARGET_DIR)/usr/bin/tonio root@$(target):/usr/bin/tonio
	ssh root@$(target) '/etc/init.d/S15tonio start'
