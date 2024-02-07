//
// Created by alex on 28/11/2021.
//

#include "VirtualInput.h"
#include "global.h"

void VirtualInput::configure(DeviceType device_type, uint32_t config0, uint32_t config1, uint32_t config2)
{
    this->device_type = device_type;
    this->config0 = config0;
    this->config1 = config1;
    this->config2 = config2;
}

// Here the raw input data from SDL is interpreted for the various virtual input classes

void VirtualToggle::update()
{
    if(device_type == DeviceType::Keyboard)
    {
        // config0 is the key
        value = G.keyboard_state_current[config0];
        delta = value - G.keyboard_state_previous[config0];
    }
    else if(device_type == DeviceType::Mouse)
    {
        if(config0 == 0) // Mouse buttons, config1 is button index
        {
            value = G.mouse_state_current->button[config1];
            delta = value - G.mouse_state_previous->button[config1];
        }
        // Mapping a toggle to the mouse cursor probably makes no sense but it is still possible
        // It triggers if the mouse cursor is outside of a threshold
        else if(config0 == 1) // Mouse cursor, config1 is index into (x,y)
        {
            constexpr float threshold = 0.1;
            float2 rel_pos_current = abs( (G.mouse_state_current->pos-0.5f*G.fwsize) / G.fwsize);
            float2 rel_pos_previous = abs( (G.mouse_state_previous->pos-0.5*G.fwsize) / G.fwsize);
            if(config1 == 0)
            {
                value = (rel_pos_current.x > threshold);
                delta = value - (rel_pos_previous.x > threshold);
            }
            else if(config1 == 1)
            {
                value = (rel_pos_current.y > threshold);
                delta = value - (rel_pos_previous.y > threshold);
            }
        }
        else if(config0 == 2) // Mouse wheel, config1 is index into (x,y). Triggers at any wheel movement.
        {
            if(config1 == 0)
            {
                value = G.mouse_state_current->wheel.x != 0;
                delta = value - (G.mouse_state_previous->wheel.x != 0);
            }
            else if(config1 == 1)
            {
                value = G.mouse_state_current->wheel.y != 0;
                delta = value - (G.mouse_state_previous->wheel.y != 0);
            }
        }
    }
}

void VirtualAxis::update()
{
    if(device_type == DeviceType::Keyboard)
    {
        // config0 is code of key for + direction, config1 for - directions
        value = G.keyboard_state_current[config0] - G.keyboard_state_current[config1];
        delta = value - (G.keyboard_state_previous[config0] - G.keyboard_state_previous[config1]);
    }
    else if(device_type == DeviceType::Mouse)
    {
        if(config0 == 0) // Mouse buttons, config1 is button1 index, config2 is button 2 index
        {
            value = G.mouse_state_current->button[config1] - G.mouse_state_current->button[config2];
            delta = value - (G.mouse_state_previous->button[config1] - G.mouse_state_previous->button[config2]);
        }
        else if(config0 == 1) // Mouse cursor, config1 is index into (x,y)
        {
            float2 rel_pos_current = 2.0f*(G.mouse_state_current->pos-0.5f*G.fwsize) / G.fwsize;
            float2 rel_pos_previous = 2.0f*(G.mouse_state_previous->pos-0.5f*G.fwsize) / G.fwsize;
            if(config1 == 0)
            {
                value = rel_pos_current.x;
                delta = value - rel_pos_previous.x;
            }
            else if(config1 == 1)
            {
                value = -rel_pos_current.y;
                delta = value + rel_pos_previous.y;
            }
        }
        else if(config0 == 2) // Mouse wheel, config1 is index into (x,y).
        {
            if(config1 == 0)
            {
                value = (float)G.mouse_state_current->wheel.x;
                delta = value - (float)G.mouse_state_previous->wheel.x;
            }
            else if(config1 == 1)
            {
                value = (float)G.mouse_state_current->wheel.y;
                delta = value - (float)G.mouse_state_previous->wheel.y;
            }
        }
    }
}