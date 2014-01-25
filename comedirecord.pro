## Modify these variables:
TEMPLATE	= app
CONFIG		+= qt warn_on release
HEADERS		= comedirecord.h comediscope.h gain.h ext_data_receive.h dc_sub.h hp.h lp.h fftscope.h channel.h
SOURCES		= comedirecord.cpp comediscope.cpp gain.cpp ext_data_receive.cpp dc_sub.cpp lp.cpp hp.cpp fftscope.cpp channel.cpp
TARGET		= comedirecord
LIBS            += -lcomedi -liir -lqwt -lfftw3
target.path     = /usr/local/bin
INSTALLS        += target
ICON		= comedirecord.svg
