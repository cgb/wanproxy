.if !defined(TOPDIR)
.error "TOPDIR must be defined."
.endif

.PATH: ${TOPDIR}/io

SRCS+=	file.cc
SRCS+=	file_descriptor.cc
SRCS+=	io_system.cc
SRCS+=	serial.cc
SRCS+=	socket.cc
