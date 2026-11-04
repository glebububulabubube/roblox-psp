TARGET = roblox
OBJS = source/main.o

# Use psplib's own include paths
INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

LIBS = -lpspgu -lpspvfpu -lm -lpspdebug -lpspctrl_stub -lpsputility_stub -lpspdisplay_stub -lpspge_stub -lpspsdk_inet_stub

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Roblox PSP
PSP_EBOOT_ICON = 

# PSPDEV build rules
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak