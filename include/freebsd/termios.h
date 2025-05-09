#ifndef __WISE_TERMIOS_H__
#define __WISE_TERMIOS_H__

#include_next <termios.h>


/*
 * Standard speeds in BSD style
 */
#undef	B0
#undef	B50
#undef	B75
#undef	B110
#undef	B134
#undef	B150
#undef	B200
#undef	B300
#undef	B600
#undef	B1200
#undef	B1800
#undef	B2400
#undef	B4800
#undef	B9600
#undef	B19200
#undef	B38400
#undef	B7200
#undef	B14400
#undef	B28800
#undef	B57600
#undef	B76800
#undef	B115200
#undef	B230400
#undef	B460800
#undef	B921600
#undef	EXTA
#undef	EXTB

#define	B0	0
#define	B50	50
#define	B75	75
#define	B110	110
#define	B134	134
#define	B150	150
#define	B200	200
#define	B300	300
#define	B600	600
#define	B1200	1200
#define	B1800	1800
#define	B2400	2400
#define	B4800	4800
#define	B9600	9600
#define	B19200	19200
#define	B38400	38400
#define	B7200	7200
#define	B14400	14400
#define	B28800	28800
#define	B57600	57600
#define	B76800	76800
#define	B115200	115200
#define	B230400	230400
#define	B460800	460800
#define	B921600	921600
#define	EXTA	19200
#define	EXTB	38400


#ifndef TCGETS
#define TCGETS 		0x5401
#endif
#ifndef TCSETS
#define TCSETS 		0x5402
#endif



/* FIXME: baudrate */

#endif /* __WISE_TERMIOS_H__ */
