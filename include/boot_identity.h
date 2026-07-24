#pragma once

#include <cstddef>
#include <cstdint>

void formatBootId(uint32_t entropy, char out[9]);
void formatClientId(uint64_t efuseMac, char* out, size_t outSize);
