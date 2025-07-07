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

// Print help information
void print_help(const char *program_name) {
  printf("numlockwl - NumLock toggler for Wayland\n");
  printf("Usage: %s [--on|--off|--device=PATH|--all|-h|--help]\n", program_name);
  printf("Source: https://github.com/jeebuscrossaint/numlockwl\n\n");
  printf("Options:\n");
  printf("  --on              Force NumLock on\n");
  printf("  --off             Force NumLock off\n");
  printf("  --device=PATH     Use specific device (e.g., --device=/dev/input/event3)\n");
  printf("  --all             Process all suitable keyboards\n");
  printf("  -h, --help        Show this help message\n\n");
  printf("MIT License\n\n");
  printf("Copyright (c) 2025 Amarnath P.\n\n");
  printf("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
  printf("of this software and associated documentation files (the \"Software\"), to deal\n");
  printf("in the Software without restriction, including without limitation the rights\n");
  printf("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
  printf("copies of the Software, and to permit persons to whom the Software is\n");
  printf("furnished to do so, subject to the following conditions:\n\n");
  printf("The above copyright notice and this permission notice shall be included in all\n");
  printf("copies or substantial portions of the Software.\n\n");
  printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
  printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
  printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
  printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
  printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
  printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
  printf("SOFTWARE.\n");
}

// Check if device has NumLock capability and LED indicator
int check_device_has_numlock(struct libevdev *dev) {
  // Check if device has NumLock key and LED
  if (libevdev_has_event_type(dev, EV_KEY) &&
      libevdev_has_event_code(dev, EV_KEY, KEY_NUMLOCK) &&
      libevdev_has_event_type(dev, EV_LED) &&
      libevdev_has_event_code(dev, EV_LED, LED_NUML)) {
    return 1;
  }
  return 0;
}

// More robust keyboard detection - check for multiple keyboard-specific keys
int is_likely_keyboard(struct libevdev *dev) {
  const char *name = libevdev_get_name(dev);
  
  // Skip devices that are obviously not keyboards
  if (name && (
      strstr(name, "Mouse") || strstr(name, "mouse") ||
      strstr(name, "Trackpad") || strstr(name, "trackpad") ||
      strstr(name, "Touchpad") || strstr(name, "touchpad") ||
      strstr(name, "Tablet") || strstr(name, "tablet") ||
      strstr(name, "Stylus") || strstr(name, "stylus") ||
      strstr(name, "Trackball") || strstr(name, "trackball"))) {
    return 0;
  }
  
  // Check for a good set of keyboard keys that are unlikely to be on mice
  int keyboard_keys = 0;
  int required_keys[] = {
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y,  // QWERTY row
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H,  // ASDF row  
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N,  // ZXCV row
    KEY_SPACE, KEY_ENTER, KEY_BACKSPACE, KEY_TAB,
    KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL
  };
  
  for (int i = 0; i < sizeof(required_keys)/sizeof(required_keys[0]); i++) {
    if (libevdev_has_event_code(dev, EV_KEY, required_keys[i])) {
      keyboard_keys++;
    }
  }
  
  // Require at least 20 keyboard keys to be considered a keyboard
  return keyboard_keys >= 20;
}

// Process a single device
int process_device(const char *device_path, int force_on, int force_off, int *processed_count) {
  struct libevdev *dev = NULL;
  int success = 0;
  
  int fd = open(device_path, O_RDWR);
  if (fd == -1) {
    return 0; // Can't open device, skip it
  }

  // Initialize libevdev device
  if (libevdev_new_from_fd(fd, &dev) < 0) {
    close(fd);
    return 0;
  }

  // Check if the device has NumLock capability and is likely a keyboard
  if (check_device_has_numlock(dev) && is_likely_keyboard(dev)) {
    printf("Found keyboard with NumLock: %s (%s)\n", device_path,
           libevdev_get_name(dev));

    // Check current NumLock state
    int numlock_state = is_numlock_on(fd);

    if (numlock_state < 0) {
      printf("Could not determine NumLock state for %s.\n", device_path);
    } else if (numlock_state) {
      printf("NumLock is currently ON for %s.\n", device_path);

      // Toggle off if forced or in toggle mode
      if (force_off || (!force_on)) {
        toggle_numlock(fd);
        (*processed_count)++;
        success = 1;
      }
    } else {
      printf("NumLock is currently OFF for %s.\n", device_path);

      // Toggle on if forced or in toggle mode
      if (force_on || (!force_off)) {
        toggle_numlock(fd);
        (*processed_count)++;
        success = 1;
      }
    }
  }

  libevdev_free(dev);
  close(fd);
  return success;
}

// Find and use keyboard(s) with NumLock capability
int main(int argc, char *argv[]) {
  struct libevdev *dev = NULL;
  char device_path[256];
  DIR *dir;
  struct dirent *entry;
  int processed_count = 0;
  int force_on = 0;
  int force_off = 0;
  int process_all = 0;
  char *specific_device = NULL;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--on") == 0) {
      force_on = 1;
    } else if (strcmp(argv[i], "--off") == 0) {
      force_off = 1;
    } else if (strcmp(argv[i], "--all") == 0) {
      process_all = 1;
    } else if (strncmp(argv[i], "--device=", 9) == 0) {
      specific_device = argv[i] + 9;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_help(argv[0]);
      return 0;
    } else {
      printf("Unknown option: %s\n", argv[i]);
      printf("Use -h or --help for usage information.\n");
      return 1;
    }
  }

  // If specific device is requested, process only that device
  if (specific_device) {
    printf("Processing specific device: %s\n", specific_device);
    if (process_device(specific_device, force_on, force_off, &processed_count)) {
      printf("Successfully processed device: %s\n", specific_device);
      return 0;
    } else {
      printf("Failed to process device or device is not a suitable keyboard: %s\n", specific_device);
      return 1;
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
      
      if (process_device(device_path, force_on, force_off, &processed_count)) {
        // If not processing all devices, stop after first successful one
        if (!process_all) {
          break;
        }
      }
    }
  }

  closedir(dir);

  if (processed_count == 0) {
    printf("No suitable keyboards found or NumLock already in desired state.\n");
    return 1;
  } else {
    printf("Successfully processed %d keyboard(s).\n", processed_count);
  }

  return 0;
}
