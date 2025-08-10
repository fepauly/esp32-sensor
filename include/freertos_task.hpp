#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <functional>

class FreeRtosTask {
public:
    template<typename F>
    FreeRtosTask(const char* name, const uint32_t stackDepth, F&& function,
                UBaseType_t priority = 5, BaseType_t core = tskNO_AFFINITY) 
                    : _handle(nullptr), _name(name), _function(std::forward<F>(function))
    {
        // Create task function
        auto taskFn = [](void* arg) {
            auto* fn = static_cast<FreeRtosTask*>(arg);
            fn->_function();
            vTaskDelete(nullptr);
        };
        
        // Create freertos task
        xTaskCreatePinnedToCore(
            taskFn,
            name,
            stackDepth,
            this,
            priority, 
            &_handle,
            core
        );

        if(!_handle) {
            ESP_LOGE("TASK", "Failed to create task %s", name);
        }
    }

    ~FreeRtosTask() 
    {
        if(_handle && eTaskGetState(_handle) != eDeleted) {
            vTaskDelete(_handle);
        }
    }

    // No copy
    FreeRtosTask(const FreeRtosTask&) = delete;
    FreeRtosTask& operator=(const FreeRtosTask&) = delete;

    TaskHandle_t getHandle() const noexcept { return _handle; }

private:
    TaskHandle_t _handle{};
    const char* _name;
    std::function<void()> _function;

};