#pragma once

#include "debounce.h"

class GpioSampler {
public:
    void begin();
    GpioSnapshot sample() const;
};
