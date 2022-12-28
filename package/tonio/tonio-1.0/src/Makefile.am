bin_PROGRAMS = tonio
tonio_SOURCES = http.c json.c media.c mfrc522.c mfrc522_hal_linux.c tonio.c volume_mapping.c

tonio_CFLAGS = -g -Werror -Wall -Wextra -Wno-unused -pedantic @DEPENDENCIES_CFLAGS@
tonio_LDADD = -lm -liw @DEPENDENCIES_LIBS@
