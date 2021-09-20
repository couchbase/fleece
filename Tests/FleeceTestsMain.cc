//
// FleeceTestsMain.cc
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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
