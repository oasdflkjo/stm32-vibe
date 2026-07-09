#include "image/app_manifest.h"

__attribute__((section(".app_manifest"), used))
const app_manifest_t app_manifest = {
    .magic = APP_MANIFEST_MAGIC,
    .manifest_version = APP_MANIFEST_VERSION,
    .manifest_size = APP_MANIFEST_SIZE,
};
