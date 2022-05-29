
#ifndef KTRANSCRIPT_EXPORT_H
#define KTRANSCRIPT_EXPORT_H

#ifdef KTRANSCRIPT_STATIC_DEFINE
#  define KTRANSCRIPT_EXPORT
#  define KTRANSCRIPT_NO_EXPORT
#else
#  ifndef KTRANSCRIPT_EXPORT
#    ifdef ktranscript_EXPORTS
        /* We are building this library */
#      define KTRANSCRIPT_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define KTRANSCRIPT_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef KTRANSCRIPT_NO_EXPORT
#    define KTRANSCRIPT_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef KTRANSCRIPT_DEPRECATED
#  define KTRANSCRIPT_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef KTRANSCRIPT_DEPRECATED_EXPORT
#  define KTRANSCRIPT_DEPRECATED_EXPORT KTRANSCRIPT_EXPORT KTRANSCRIPT_DEPRECATED
#endif

#ifndef KTRANSCRIPT_DEPRECATED_NO_EXPORT
#  define KTRANSCRIPT_DEPRECATED_NO_EXPORT KTRANSCRIPT_NO_EXPORT KTRANSCRIPT_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef KTRANSCRIPT_NO_DEPRECATED
#    define KTRANSCRIPT_NO_DEPRECATED
#  endif
#endif

#endif /* KTRANSCRIPT_EXPORT_H */
