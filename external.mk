include $(sort $(wildcard $(BR2_EXTERNAL_TONIO_PATH)/package/*/*.mk))

flash-boot:
	$(shell dirname $(BR2_ROOTFS_OVERLAY))/flash-single.sh $(BINARIES_DIR)/boot.vfat 1 $(dev)

flash-rootfs:
	$(shell dirname $(BR2_ROOTFS_OVERLAY))/flash-single.sh $(BINARIES_DIR)/rootfs.ext4 2 $(dev)

flash:
	$(shell dirname $(BR2_ROOTFS_OVERLAY))/flash.sh $(BINARIES_DIR)/sdcard.img $(dev)
