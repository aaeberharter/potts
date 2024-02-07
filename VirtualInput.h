//
// Created by alex on 28/11/2021.
//

#ifndef CPP_SDL2_VIRTUALINPUT_H
#define CPP_SDL2_VIRTUALINPUT_H

#include <cstdint>

enum class DeviceType { Keyboard, Mouse };

class VirtualInput
{
public:
    // Uses global input data from keyboard, mouse etc. to update this VirtualInput
    virtual void update() = 0;
    void configure(DeviceType deviceType, uint32_t config0, uint32_t config1 = 0, uint32_t config2 = 0);
protected:
    DeviceType device_type;
    uint32_t config0, config1, config2;
};

// For on/off data
class VirtualToggle: public VirtualInput
{
public:
    int value;
    int delta;

    void update() override;
};

// For floats in range -1, 1. Also supports the range 0, 1
class VirtualAxis: public VirtualInput
{
public:
    float value;
    float delta;

    void update() override;
};

#endif //CPP_SDL2_VIRTUALINPUT_H
