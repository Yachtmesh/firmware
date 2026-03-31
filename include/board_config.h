#pragma once

// Hardware pin assignments, overridden per environment via build flags in platformio.ini.
// Defaults match esp32s3 board layout.

#ifndef BOARD_TWAI_TX
#define BOARD_TWAI_TX 5
#endif

#ifndef BOARD_TWAI_RX
#define BOARD_TWAI_RX 4
#endif

#ifndef BOARD_I2C_SDA
#define BOARD_I2C_SDA 8
#endif

#ifndef BOARD_I2C_SCL
#define BOARD_I2C_SCL 9
#endif

#ifndef BOARD_SERIAL_RX
#define BOARD_SERIAL_RX 33
#endif

#ifndef BOARD_SERIAL_TX
#define BOARD_SERIAL_TX 34
#endif
