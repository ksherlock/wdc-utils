#ifndef endian_h
#define endian_h

enum class endian {
#ifdef _WIN32
    little = 1234,
    big    = 4321,
    native = little
#else
	little = __ORDER_LITTLE_ENDIAN__,
	big = __ORDER_BIG_ENDIAN__,
	native = __BYTE_ORDER__
#endif
};

#endif
