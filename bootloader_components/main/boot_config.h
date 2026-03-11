#pragma once

#include <stdint.h>

// Shared limits across boot validation, recovery console, and update protocol.
#define BL_RECOVERY_CMD_MAX_LEN         32U
#define BL_UPDATE_CMD_MAX_LEN            BL_RECOVERY_CMD_MAX_LEN
#define BL_UPDATE_HEADER_MAX_LEN         96U
#define BL_UPDATE_CHUNK_MAX_SIZE       1024U
#define BL_UPDATE_CHUNK_TIMEOUT_MS   120000U
#define BL_UPDATE_MAX_ERRORS             10U
#define BL_UPDATE_MAX_TRACKED_SECTORS  1024U
#define BL_CRC_READ_CHUNK_SIZE         1024U
