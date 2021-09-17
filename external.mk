include $(sort $(wildcard $(BR2_EXTERNAL_TONIO_PATH)/package/*/*.mk))

flash-light:
	umount $(dev)2 || true
	sudo dd if=output/images/rootfs.ext4 of=$(dev)2
	sync
	echo Done

flash:
	sudo dd if=output/images/sdcard.img of=$(dev)
	sync
	umount $(dev)3 || true
	sudo parted $(dev) unit B resizepart 3 $(( `sudo fatresize -i $(dev)3 | grep -Po '(?<=Max size: )[0-9]+(?=$)'` - 1 ))
	sudo fatresize -s $(( `sudo fatresize -i $(dev)3 | grep -Po '(?<=Max size: )[0-9]+(?=$)'` - 1 )) $(dev)3
	echo Done