#include "image/app_manifest.h"

__attribute__((section(".app_manifest"), used))
const app_manifest_t app_manifest = {
    .magic = APP_MANIFEST_MAGIC,
};
