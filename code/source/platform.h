#pragma once

#if defined(_WIN32)
# if defined(_M_X64)

# elif defined(_M_ARM64) //# if defined(_M_X64)

# else //# elif defined(_M_ARM64) //# if defined(_M_X64)
#  error Unsupported platform. Only x64 and arm64 currently supported.
# endif //# else //# elif defined(_M_ARM64) //# if defined(_M_X64)
#elif defined(__linux__) //#if defined(__WIN32)
# if defined(__x86_64__)

 #elif defined(__aarch64__) //# if defined(__x86_64__)

# else //#elif defined(__aarch64__) //# if defined(__x86_64__)
#  error Unsupported platform. Only x64 and arm64 currently supported.
# endif //# else //#elif defined(__aarch64__) //# if defined(__x86_64__)
#else //#elif defined(__linux__) //#if defined(__WIN32)
# error Unsupported operating system. Only Windows and Linux currently supported.
#endif //#else //#elif defined(__linux__) //#if defined(__WIN32)