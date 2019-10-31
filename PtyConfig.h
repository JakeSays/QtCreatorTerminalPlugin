/* Defined to the path of the PTY multiplexer device, if any */
/* #undef PTM_DEVICE */

#define HAVE_POSIX_OPENPT 1
#define HAVE_GETPT  0
#define HAVE_GRANTPT 0
#define HAVE_OPENPTY 1
#define HAVE_PTSNAME 1
#define HAVE_REVOKE 0
#define HAVE_UNLOCKPT 0
#define HAVE__GETPTY 0
#define HAVE_TCGETATTR 1
#define HAVE_TCSETATTR 1

#define HAVE_LIBUTIL_H 0
#define HAVE_UTIL_H 0
#define HAVE_PTY_H 1
/* Unused ? */
#define HAVE_TERMIOS_H 1
#define HAVE_TERMIO_H 1
#define HAVE_SYS_STROPTS_H 0
#define HAVE_SYS_FILIO_H 0

#define HAVE_UTEMPTER 1
#define HAVE_LOGIN 0
#define HAVE_UTMPX 0
#define HAVE_LOGINX 0
#define HAVE_STRUCT_UTMP_UT_TYPE 0
#define HAVE_STRUCT_UTMP_UT_PID 0
#define HAVE_STRUCT_UTMP_UT_SESSION 0
#define HAVE_STRUCT_UTMP_UT_SYSLEN 0
#define HAVE_STRUCT_UTMP_UT_ID 0

#define HAVE_SYS_TIME_H 1

/*
 * Steven Schultz <sms at to.gd-es.com> tells us :
 * BSD/OS 4.2 doesn't have a prototype for openpty in its system header files
 */
#ifdef __bsdi__
__BEGIN_DECLS
int openpty(int *, int *, char *, struct termios *, struct winsize *);
__END_DECLS
#endif

