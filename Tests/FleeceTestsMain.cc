//
// FleeceTestsMain.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define CATCH_CONFIG_CONSOLE_WIDTH 120
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

#include "CaseListReporter.hh"
#include "FleeceTests.hh"


#ifdef FL_ESP32
// Below is for ESP32 development:

#include <signal.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"

static const char *TAG = "FleeceTests";

static bool initFilesystem();


extern "C"
void app_main() {
    esp_task_wdt_init(90, false);   // lower watchdog threshold to 90 sec
    if (!initFilesystem())
        return;
    const char* args[3] = {"FleeceTests", "-r", "list"};
    main(3, (char**)args);
}

static bool initFilesystem() {
    // Adapted from ESP-IDF's example:
    // https://github.com/espressif/esp-idf/tree/9a55b42/examples/storage/spiffs
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/tmp",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return false;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        return true;
    }
}

// ESP32 lib has no signal() function, so fix the link error:
extern "C"
_sig_func_ptr signal(int, _sig_func_ptr) {
    //printf("*** signal() called\n");
    return nullptr;
}

#endif // FL_ESP32
