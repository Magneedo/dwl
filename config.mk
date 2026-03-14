_VERSION = 0.7
VERSION  = `git describe --tags --dirty 2>/dev/null || echo $(_VERSION)`

PKG_CONFIG = pkg-config

# paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man
DATADIR = $(PREFIX)/share

XWAYLAND =
XLIBS =
# Uncomment to build XWayland support
#XWAYLAND = -DXWAYLAND
#XLIBS = xcb xcb-icccm

CC = gcc

# Personal-build friendly defaults focused on runtime performance and
# smaller/faster links. Override from environment if needed.
CFLAGS ?= -O2 -march=native -pipe -fomit-frame-pointer -flto=auto
LDFLAGS ?= -Wl,-O1,--as-needed -flto=auto

# Optional install artifacts.
INSTALL_MANPAGE ?= 0
INSTALL_SESSION_FILE ?= 1
