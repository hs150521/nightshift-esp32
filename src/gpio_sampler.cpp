#include "gpio_sampler.h"

#include "config.h"

#include <Arduino.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>

void GpioSampler::begin() {
    for (const int pin : nightshift::SENSOR_PINS) {
        pinMode(pin, INPUT_PULLUP);
    }
}

GpioSnapshot GpioSampler::sample() const {
    // GPIO4..7 are in the same input register, so this is one coherent read.
    const uint32_t levels = REG_READ(GPIO_IN_REG);
    const auto triggered = [levels](int pin) {
        const bool electricalHigh = (levels & (1UL << pin)) != 0;
        return nightshift::sensorTriggeredFromElectricalLevel(electricalHigh);
    };
    return {
        triggered(nightshift::PIN_CUSHION_LEFT),
        triggered(nightshift::PIN_CUSHION_RIGHT),
        triggered(nightshift::PIN_FOOTREST_LEFT),
        triggered(nightshift::PIN_FOOTREST_RIGHT)
    };
}
