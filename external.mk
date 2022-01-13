include $(sort $(wildcard $(BR2_EXTERNAL_TONIO_PATH)/package/*/*.mk))

flash-root:
	$(shell dirname $(BR2_ROOTFS_OVERLAY))/flash-root.sh $(BINARIES_DIR)/rootfs.ext4 $(dev)

flash:
	$(shell dirname $(BR2_ROOTFS_OVERLAY))/flash.sh $(BINARIES_DIR)/sdcard.img $(dev)
