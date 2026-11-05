#pragma once

// The ESP32 SDK already ships headers named "config.h", hence the unique name.
#if __has_include("gold_config.h")
#include "gold_config.h"
#else
#warning "include/gold_config.h not found - using gold_config.example.h . Copy include/gold_config.example.h to include/gold_config.h"
#include "gold_config.example.h"
#endif
