bin_PROGRAMS = tonio

if MEDIA_VLC
MEDIA_SRC = media_vlc.c
else
MEDIA_SRC = media_opus.c
endif

tonio_SOURCES = http.c $(MEDIA_SRC) mfrc522.c mfrc522_hal_linux.c tonio.c volume_mapping.c input.c

tonio_CFLAGS = -g -Werror -Wall -Wextra -Wno-unused -pedantic @DEPENDENCIES_CFLAGS@
tonio_LDADD = -lm -liw @DEPENDENCIES_LIBS@
