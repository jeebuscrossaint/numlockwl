#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <dirent.h>

#define DEVICE_NAME "numlockwl-device"

// Function to check NumLock state using 'xset' command
int is_numlock_on() {
    FILE *fp;
    char buffer[128];

    // Run 'xset q' and check if NumLock is on
    fp = popen("xset q | grep 'Num Lock' | awk '{print $4}'", "r");
    if (fp == NULL) {
        perror("Failed to check NumLock state");
        return 0;  // Default to off if check fails
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strncmp(buffer, "on", 2) == 0) {
            fclose(fp);
            return 1;  // NumLock is on
        }
    }

    fclose(fp);
    return 0;  // NumLock is off
}

void toggle_numlock(int fd) {
    struct input_event ev;

    // Press NumLock key
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    ev.code = KEY_NUMLOCK;
    ev.value = 1;
    if (write(fd, &ev, sizeof(struct input_event)) < 0) {
        perror("Failed to send NumLock press event");
        exit(1);
    }

    // Send SYN_REPORT to indicate event is finished
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    if (write(fd, &ev, sizeof(struct input_event)) < 0) {
        perror("Failed to send SYN_REPORT");
        exit(1);
    }

    usleep(50000);  // Sleep for 50ms (to simulate the key press duration)

    // Release NumLock key
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    ev.code = KEY_NUMLOCK;
    ev.value = 0;
    if (write(fd, &ev, sizeof(struct input_event)) < 0) {
        perror("Failed to send NumLock release event");
        exit(1);
    }

    // Send SYN_REPORT to indicate event is finished
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    if (write(fd, &ev, sizeof(struct input_event)) < 0) {
        perror("Failed to send SYN_REPORT");
        exit(1);
    }

    printf("NumLock toggled.\n");
}

int check_device_has_numlock(struct libevdev *dev) {
    if (libevdev_has_event_type(dev, EV_KEY)) {
        if (libevdev_has_event_code(dev, EV_KEY, KEY_NUMLOCK)) {
            return 1;
        }
    }
    return 0;
}

void list_and_toggle_devices() {
    struct libevdev *dev = NULL;
    char device_path[256];
    DIR *dir;
    struct dirent *entry;

    dir = opendir("/dev/input");
    if (!dir) {
        perror("Failed to open /dev/input");
        return;
    }

    // Loop through all devices in /dev/input
    while ((entry = readdir(dir)) != NULL) {
        // Check if it's an input device
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(device_path, sizeof(device_path), "/dev/input/%s", entry->d_name);

            int fd = open(device_path, O_RDWR);
            if (fd == -1) {
                continue;  // Can't open device, skip it
            }

            // Initialize libevdev device
            if (libevdev_new_from_fd(fd, &dev) < 0) {
                close(fd);
                continue;
            }

            // Check if the device has NumLock capability
            if (check_device_has_numlock(dev)) {
                printf("Device %s supports NumLock, checking state...\n", device_path);

                // Only toggle NumLock if it is off
                if (!is_numlock_on()) {
                    printf("NumLock is off, toggling...\n");
                    toggle_numlock(fd);  // Toggle NumLock
                } else {
                    printf("NumLock is already on, skipping toggle.\n");
                }
            }

            libevdev_free(dev);
            close(fd);
        }
    }

    closedir(dir);
}

int main() {
    list_and_toggle_devices();
    return 0;
}
