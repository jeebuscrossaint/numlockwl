#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Include with the full paths that match your system
#include <libevdev-1.0/libevdev/libevdev.h>

#define DEVICE_NAME "numlockwl-device"

// Function to check NumLock state using input device LED state
int is_numlock_on(int fd) {
  struct libevdev *dev = NULL;
  int result = -1;

  if (libevdev_new_from_fd(fd, &dev) >= 0) {
    // Check LED state directly using the LED_NUML constant
    // Alternative to libevdev_get_led_value which may not be available
    if (libevdev_has_event_type(dev, EV_LED) &&
        libevdev_has_event_code(dev, EV_LED, LED_NUML)) {

      // Get LED state by querying directly
      unsigned long leds;
      if (ioctl(fd, EVIOCGLED(sizeof(leds)), &leds) >= 0) {
        // Check if bit corresponding to LED_NUML is set
        result = !!(leds & (1 << LED_NUML));
      }
    }
    libevdev_free(dev);
  }

  return result;
}

// Toggle NumLock state
void toggle_numlock(int fd) {
  struct input_event ev;

  // Press NumLock key
  memset(&ev, 0, sizeof(struct input_event));
  ev.type = EV_KEY;
  ev.code = KEY_NUMLOCK;
  ev.value = 1;
  if (write(fd, &ev, sizeof(struct input_event)) < 0) {
    perror("Failed to send NumLock press event");
    return;
  }

  // Send SYN_REPORT to indicate event is finished
  memset(&ev, 0, sizeof(struct input_event));
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  if (write(fd, &ev, sizeof(struct input_event)) < 0) {
    perror("Failed to send SYN_REPORT");
    return;
  }

  usleep(50000); // Sleep for 50ms (to simulate the key press duration)

  // Release NumLock key
  memset(&ev, 0, sizeof(struct input_event));
  ev.type = EV_KEY;
  ev.code = KEY_NUMLOCK;
  ev.value = 0;
  if (write(fd, &ev, sizeof(struct input_event)) < 0) {
    perror("Failed to send NumLock release event");
    return;
  }

  // Send SYN_REPORT to indicate event is finished
  memset(&ev, 0, sizeof(struct input_event));
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  if (write(fd, &ev, sizeof(struct input_event)) < 0) {
    perror("Failed to send SYN_REPORT");
    return;
  }

  printf("NumLock toggled.\n");
}

// Check if device has NumLock capability and LED indicator
int check_device_has_numlock(struct libevdev *dev) {
  // Check if device has NumLock key and LED
  if (libevdev_has_event_type(dev, EV_KEY) &&
      libevdev_has_event_code(dev, EV_KEY, KEY_NUMLOCK) &&
      libevdev_has_event_type(dev, EV_LED) &&
      libevdev_has_event_code(dev, EV_LED, LED_NUML)) {

    // Only consider keyboard devices
    if (libevdev_has_event_code(dev, EV_KEY, KEY_Q) && // Basic keyboard check
        libevdev_has_event_code(dev, EV_KEY, KEY_SPACE)) {
      return 1;
    }
  }
  return 0;
}

// Find and use a single keyboard with NumLock capability
int main(int argc, char *argv[]) {
  struct libevdev *dev = NULL;
  char device_path[256];
  DIR *dir;
  struct dirent *entry;
  int toggled = 0;
  int force_on = 0;
  int force_off = 0;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--on") == 0) {
      force_on = 1;
    } else if (strcmp(argv[i], "--off") == 0) {
      force_off = 1;
    }
  }

  dir = opendir("/dev/input");
  if (!dir) {
    perror("Failed to open /dev/input");
    return 1;
  }

  // Loop through all devices in /dev/input
  while ((entry = readdir(dir)) != NULL) {
    // Check if it's an input device
    if (strncmp(entry->d_name, "event", 5) == 0) {
      snprintf(device_path, sizeof(device_path), "/dev/input/%s",
               entry->d_name);
      int fd = open(device_path, O_RDWR);
      if (fd == -1) {
        continue; // Can't open device, skip it
      }

      // Initialize libevdev device
      if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        continue;
      }

      // Check if the device has NumLock capability
      if (check_device_has_numlock(dev)) {
        printf("Found keyboard with NumLock: %s (%s)\n", device_path,
               libevdev_get_name(dev));

        // Check current NumLock state
        int numlock_state = is_numlock_on(fd);

        if (numlock_state < 0) {
          printf("Could not determine NumLock state.\n");
        } else if (numlock_state) {
          printf("NumLock is currently ON.\n");

          // Toggle off if forced or in toggle mode
          if (force_off || (!force_on && !toggled)) {
            toggle_numlock(fd);
            toggled = 1;
          }
        } else {
          printf("NumLock is currently OFF.\n");

          // Toggle on if forced or in toggle mode
          if (force_on || (!force_off && !toggled)) {
            toggle_numlock(fd);
            toggled = 1;
          }
        }

        // We found and processed a keyboard, stop here
        libevdev_free(dev);
        close(fd);
        break;
      }

      libevdev_free(dev);
      close(fd);
    }
  }

  closedir(dir);

  if (!toggled) {
    printf("No suitable keyboard found or NumLock already in desired state.\n");
    return 1;
  }

  return 0;
}
