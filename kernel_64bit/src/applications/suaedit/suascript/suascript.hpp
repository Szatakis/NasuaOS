#ifndef SUASCRIPT_H
#define SUASCRIPT_H

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <cstdint>
#include <cstddef>
#include <limine.h>

#include "system/drivers/audio/driver.hpp"
#include "system/drivers/disk/driver.hpp"
#include "system/drivers/memory/driver.hpp"
#include "system/drivers/rtc/driver.hpp"
#include "system/drivers/uart/driver.hpp"
#include "system/drivers/gpu/driver.hpp"


void compile_code(char* code);

#endif // SUASCRIPT_H