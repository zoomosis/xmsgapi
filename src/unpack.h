#if defined(__TURBOC__) || defined(__DMC__) || defined(__LCC__) || defined(__GNUC__) || defined(_MSC_VER)
#ifdef __TURBOC__
#pragma warn -pck
#endif
#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif
#pragma pack()
#elif defined(__WATCOMC__)
#pragma pack(__pop);
#elif defined(__HIGHC__)
#pragma pop_align_members();
#endif
