#include "gpio_sampler.h"

#include "config.h"

#include <Arduino.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>

void GpioSampler::begin() {
    for (const int pin : nightshift::SENSOR_PINS) {
        pinMode(pin, INPUT_PULLDOWN);
    }
}

GpioSnapshot GpioSampler::sample() const {
    // GPIO4..7 are in the same input register, so this is one coherent read.
    const uint32_t levels = REG_READ(GPIO_IN_REG);
    return {
        (levels & (1UL << nightshift::PIN_CUSHION_LEFT)) != 0,
        (levels & (1UL << nightshift::PIN_CUSHION_RIGHT)) != 0,
        (levels & (1UL << nightshift::PIN_FOOTREST_LEFT)) != 0,
        (levels & (1UL << nightshift::PIN_FOOTREST_RIGHT)) != 0
    };
}
