#pragma once

#if defined(__has_attribute)
#if __has_attribute(visibility)
#define CPPREACT_VISIBLE __attribute__((visibility("default")))
#endif
#endif

#ifndef CPPREACT_VISIBLE
#define CPPREACT_VISIBLE
#endif
