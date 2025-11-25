#include <stdio.h>
#include "../include/isa.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <guest_image.bin> [guest2.bin ...]\n", argv[0]);
        fprintf(stderr, "Example: %s examples/programs/test.bin\n", argv[0]);
        fprintf(stderr, "Multiple guests: %s guest1.bin guest2.bin\n", argv[0]);
        return 1;
    }

    /* Create hypervisor */
    hypervisor_t* hv = hypervisor_create();
    if (!hv) {
        fprintf(stderr, "[ERROR] Failed to create hypervisor\n");
        return 1;
    }

    printf("======================================\n");
    printf("  vISA Hypervisor Virtual Machine\n");
    printf("======================================\n\n");

    /* Load guest VMs */
    for (int i = 1; i < argc; i++) {
        uint32_t guest_id = hypervisor_create_guest(hv, argv[i]);
        if (guest_id == 0) {
            fprintf(stderr, "[ERROR] Failed to create guest from %s\n", argv[i]);
            hypervisor_destroy(hv);
            return 1;
        }
    }

    /* Run each guest */
    for (uint32_t i = 1; i <= hv->guest_count; i++) {
        hypervisor_run_guest(hv, i);
    }

    /* Final state */
    hypervisor_dump_state(hv);

    hypervisor_destroy(hv);
    return 0;
}
