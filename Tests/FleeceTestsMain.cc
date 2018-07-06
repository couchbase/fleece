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


#ifdef ESP_PLATFORM
// Below is for ESP32 development:

#include <signal.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"

static const char *TAG = "FleeceTests";


extern "C"
void app_main() {
    esp_task_wdt_init(90, false);   // lower watchdog threshold to 90 sec
    nvs_flash_init();
    const char* args[3] = {"FleeceTests", "-r", "list"};
    main(3, (char**)args);
}

// ESP32 lib has no signal() function, so fix the link error:
extern "C"
_sig_func_ptr signal(int, _sig_func_ptr) {
    //printf("*** signal() called\n");
    return nullptr;
}

#endif // ESP_PLATFORM
