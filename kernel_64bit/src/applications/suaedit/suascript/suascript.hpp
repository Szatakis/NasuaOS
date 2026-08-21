#ifndef SUASCRIPT_H
#define SUASCRIPT_H

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cstdint>
#include <cstddef>
#include <limine.h>

#include "drivers/audio/driver.hpp"
#include "drivers/disk/driver.hpp"
#include "drivers/memory/driver.hpp"
#include "drivers/rtc/driver.hpp"
#include "drivers/uart/driver.hpp"
#include "drivers/gpu/driver.hpp"


void compile_code(char* code);

#endif // SUASCRIPT_H