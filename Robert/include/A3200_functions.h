#pragma once
#include<source_location>

#ifndef A3200_FNS_H
#define A3200_FNS_H

/// @brief Prints the most recent error from the A3200.
void A3200Error(const std::source_location location = std::source_location::current());

#endif // A3200_FNS_H