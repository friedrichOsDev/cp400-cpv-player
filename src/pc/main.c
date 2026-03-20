#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("CPV Converter\n\n");

    if (argc > 1) {
        printf("Load Videofile: %s\n", argv[1]);
    } else {
        printf("No Input given.\n");
        printf("Usage: ./cpv_convert <dateiname.cpv>\n");
    }

    printf("\nTest finished.\n");

    return 0;
}
