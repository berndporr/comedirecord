## Modify these variables:
TEMPLATE	= app
CONFIG		+= qt warn_on debug
HEADERS		= comedirecord.h comediscope.h gain.h ext_data_receive.h dc_sub.h fftscope.h
SOURCES		= comedirecord.cpp comediscope.cpp gain.cpp ext_data_receive.cpp dc_sub.cpp fftscope.cpp
TARGET		= comedirecord
LIBS            += -lcomedi -liir -lqwt -lfftw3
target.path     = /usr/local/bin
INSTALLS        += target
ICON		= comedirecord.svg
