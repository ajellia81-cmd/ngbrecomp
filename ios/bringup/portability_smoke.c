/* Compile-check the production portability header against the iPhoneOS SDK.
 * This file intentionally does not execute memory protection operations.
 */
#include "../../src/port.h"
int ngb_ios_portability_smoke(void) {
    char path[1024];
    ngb_config_dir(path, sizeof(path), 0);
    return (int)(ngb_exe_path(path, sizeof(path)) < 0);
}
