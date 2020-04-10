#ifndef ARPA_INET_H
#define ARPA_INET_H

#if defined(ESP8266)
#include <machine/endian.h>
#endif

#if BYTE_ORDER == BIG_ENDIAN

#define HTONS(n) (n)
#define NTOHS(n) (n)
#define HTONL(n) (n)
#define NTOHL(n) (n)

#else

#define HTONS(n) (((((uint16_t)(n) & 0xFF)) << 8) | (((uint16_t)(n) & 0xFF00) >> 8))
#define NTOHS HTONS

#define HTONL(n) (((((uint32_t)(n) & 0xFF)) << 24) | \
                  ((((uint32_t)(n) & 0xFF00)) << 8) | \
                  ((((uint32_t)(n) & 0xFF0000)) >> 8) | \
                  ((((uint32_t)(n) & 0xFF000000)) >> 24))
#define NTOHL HTONL

#endif

#define htons HTONS
#define ntohs NTOHS

#define htonl HTONL
#define ntohl NTOHL

#endif //ARPA_INET_H
