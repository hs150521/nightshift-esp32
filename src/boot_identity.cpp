#include "boot_identity.h"

#include "config.h"

#include <cstdio>

void formatBootId(uint32_t entropy, char out[9]) {
    std::snprintf(out, 9, "%08lx", static_cast<unsigned long>(entropy));
}

void formatClientId(uint64_t efuseMac, char* out, size_t outSize) {
    const uint32_t suffix = static_cast<uint32_t>(efuseMac & 0x00ffffffULL);
    std::snprintf(
        out,
        outSize,
        MQTT_CLIENT_ID_PREFIX "%06lx",
        static_cast<unsigned long>(suffix)
    );
}
