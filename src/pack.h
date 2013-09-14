#if defined(__TURBOC__) || defined(__DMC__) || defined(__LCC__) || defined(__GNUC__) || defined(_MSC_VER)
#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif
#ifdef __TURBOC__
#pragma warn -pck
#pragma option -w-8059
#endif
#pragma pack(1)
#elif defined(__WATCOMC__)
#pragma pack(__push, 1);
#elif defined(__HIGHC__)
#pragma push_align_members(1);
#endif
