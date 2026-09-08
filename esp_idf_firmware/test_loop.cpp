#include <stdio.h>

int main() {
    int current_task_count = 0;
    int loops = 0;
    for (int i = 0; i < current_task_count - 1; i++) {
        loops++;
        if (loops > 10) break;
    }
    printf("Loops: %d\n", loops);
    return 0;
}
