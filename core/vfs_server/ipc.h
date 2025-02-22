#pragma once

#define IPC_FLAG_CAN_MODIFY 1
#define IPC_FLAG_IS_DIRECTORY 2
#define IPC_FLAG_IS_MOUNT_POINT 3

#define IPC_FLAGS(badge) ((badge) & 3)
#define IPC_ID(badge) ((badge) >> 2)
#define IPC_BADGE(id, flags) (((id) << 2) | ((flags) & 3))
