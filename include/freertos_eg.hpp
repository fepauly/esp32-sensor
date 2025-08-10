#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

class FreeRtosEventGroup {
public:
    FreeRtosEventGroup(const char* name)
        : _name(name)
    {
        _handle = xEventGroupCreate();

        if(!_handle) {
            ESP_LOGE("EVENT GROUP", "Event group %s failed to create", _name);
        }
    }

    ~FreeRtosEventGroup() 
    {
        if(_handle) {
            vEventGroupDelete(_handle);
        }
    }

    FreeRtosEventGroup(const FreeRtosEventGroup&) = delete;
    FreeRtosEventGroup& operator=(const FreeRtosEventGroup&) = delete;

    EventBits_t set(const EventBits_t bits) 
    {
        return xEventGroupSetBits(_handle, bits);
    }

    EventBits_t get() 
    {
        return xEventGroupGetBits(_handle);
    }

    EventBits_t wait(const EventBits_t bits, bool waitAll, const bool clearOnExit, TickType_t t)
    {
        return xEventGroupWaitBits(
            _handle,
            bits,
            clearOnExit ? pdTRUE : pdFALSE,
            waitAll ? pdTRUE : pdFALSE,
            t
        );
    }

    EventGroupHandle_t getHandle() const noexcept { return _handle; }
private:
    EventGroupHandle_t _handle;
    const char* _name;
};