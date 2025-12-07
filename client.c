// client.c
// Raspberry Pi 5 + DHT11 (kernel IIO driver) + Button + LED + TCP Client
// Sends status to the server using four logical machine IDs:
//   - ARM01   : Overall mode + temperature + emergency warnings
//   - TEMP02  : Temperature only
//   - BUTTON01: Button state
//   - LED01   : LED state

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

#define SERVER_IP "192.168.0.14"   // <-- change to your server PC IP if needed
#define PORT      8080

#define ID_ARM   "ARM01"
#define ID_TEMP  "TEMP02"
#define ID_BTN   "BUTTON01"
#define ID_LED   "LED01"

// ================= GPIO via pinctrl (Button & LED) =================

// Control LED (1: ON, 0: OFF) on GPIO18
void set_led(int state) {
    if (state)
        system("pinctrl set 18 op dh > /dev/null"); // GPIO18 High
    else
        system("pinctrl set 18 op dl > /dev/null"); // GPIO18 Low
}

// Read button state (1: pressed, 0: not pressed) on GPIO17
int read_btn() {
    FILE *fp;
    char output[256];

    fp = popen("pinctrl get 17", "r");
    if (!fp) return 0;

    if (fgets(output, sizeof(output), fp) != NULL) {
        if (strstr(output, "hi") != NULL) { // "hi" means high level
            pclose(fp);
            return 1;
        }
    }
    pclose(fp);
    return 0;
}

// Initialize GPIO for button/LED using pinctrl
void init_gpio_shell() {
    system("pinctrl set 18 op dl"); // LED OFF
    system("pinctrl set 17 ip pd"); // Button input, pull-down
}

// ================= DHT11 via kernel IIO driver =================
//
// Make sure you have in /boot/firmware/config.txt (or /boot/config.txt):
//   dtoverlay=dht11,gpiopin=4
//
// After reboot, the DHT11 appears under /sys/bus/iio/devices as iio:deviceX.
//   name               -> "dht11"
//   in_temp_input      -> temperature in millidegree Celsius
//   in_humidityrelative_input -> humidity in milli-%RH

static char dht11_base_path[128] = "";
static int dht11_found = 0;

// Find the DHT11 device path, e.g. "/sys/bus/iio/devices/iio:device0"
int find_dht11_device() {
    if (dht11_found) return 0;  // already found

    const char *base = "/sys/bus/iio/devices";
    DIR *dir = opendir(base);
    if (!dir) {
        printf("[WARN] Cannot open %s\n", base);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) == 0) {
            char name_path[256];
            snprintf(name_path, sizeof(name_path),
                     "%s/%s/name", base, entry->d_name);

            FILE *fp = fopen(name_path, "r");
            if (!fp) continue;

            char name_buf[64] = {0};
            if (fgets(name_buf, sizeof(name_buf), fp) != NULL) {
                name_buf[strcspn(name_buf, "\n")] = 0; // strip newline

                // Be flexible: any name containing "dht" is treated as DHT11
                if (strstr(name_buf, "dht") != NULL) {
                    snprintf(dht11_base_path, sizeof(dht11_base_path),
                             "%s/%s", base, entry->d_name);
                    dht11_found = 1;
                    printf("[INFO] DHT sensor found at: %s (name=%s)\n",
                           dht11_base_path, name_buf);
                    fclose(fp);
                    closedir(dir);
                    return 0;
                }
            }
            fclose(fp);
        }
    }

    closedir(dir);
    printf("[WARN] No DHT11 device found under %s\n", base);
    return -1;
}

// Read DHT11 temperature via IIO sysfs.
// Returns temperature in °C on success,
// or keeps last valid temperature, or -1000.0 if none yet.
float read_temperature() {
    static float last_temp = -1000.0f;

    if (!dht11_found) {
        if (find_dht11_device() != 0) {
            // DHT11 device not found; keep last temp
            return last_temp;
        }
    }

    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path),
             "%s/in_temp_input", dht11_base_path);

    FILE *fp = fopen(temp_path, "r");
    if (!fp) {
        printf("[WARN] Cannot open %s\n", temp_path);
        return last_temp;
    }

    long milli = 0;
    if (fscanf(fp, "%ld", &milli) != 1) {
        fclose(fp);
        printf("[WARN] Failed to read temperature from %s\n", temp_path);
        return last_temp;
    }
    fclose(fp);

    float temp_c = milli / 1000.0f;
    last_temp = temp_c;
    return temp_c;
}

// ================= Networking Utilities =================

// Connect to server and send "ID:Just Connected"
int connect_to_server(const char *id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[ERROR] socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("[ERROR] Invalid SERVER_IP\n");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[ERROR] Connection Failed");
        close(sock);
        return -1;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "%s:Just Connected", id);
    write(sock, msg, strlen(msg));
    printf("[INFO] Connected as %s\n", id);

    return sock;
}

// Send status in "ID:STATUS" format and log locally
void send_status(int sock, const char *id, const char *status) {
    if (sock < 0) return;

    char msg[512];
    snprintf(msg, sizeof(msg), "%s:%s", id, status);
    write(sock, msg, strlen(msg));

    printf("[SEND][%s] %s\n", id, status);
}

void *recv_thread(void *arg) {
    int sock = *(int *)arg;
    char buffer[256];

    while (1) {
        int len = read(sock, buffer, sizeof(buffer) - 1);

        if (len <= 0) {
            printf("\n[FATAL] Server disconnected.\n");
            kill(getpid(), SIGINT); 
            break; 
        }

        buffer[len] = 0;
        printf("\n[COMMAND RECEIVED] Server says: %s\n", buffer);

        if (strstr(buffer, "RESET") != NULL) {
            printf(">> RESET command detected! Rebooting system state...\n");
            set_led(0); 
            usleep(500000);
            set_led(1);
        }
    }
    return NULL;
}

void cleanup_handler(int sig) {
    printf("\n[SYSTEM] Cleaning up resources...\n");
    set_led(0); // LED 끄기
    printf("[SYSTEM] LED turned OFF.\n");
    printf("[SYSTEM] Client terminated safely.\n");
    exit(0);
}

int main(void) {
    signal(SIGINT, cleanup_handler);

    printf("[INFO] Initializing GPIO (button & LED) via pinctrl...\n");
    init_gpio_shell();

    // Connect to the server with four logical IDs
    int sock_arm  = connect_to_server(ID_ARM);
    int sock_temp = connect_to_server(ID_TEMP);
    int sock_btn  = connect_to_server(ID_BTN);
    int sock_led  = connect_to_server(ID_LED);

    if (sock_arm < 0 && sock_temp < 0 && sock_btn < 0 && sock_led < 0) {
        printf("[FATAL] Failed to establish any connection.\n");
        return 1;
    }

    pthread_t r_tid;
    int ret = pthread_create(&r_tid, NULL, recv_thread, (void *)&sock_arm);
    
    if (ret != 0) {
        perror("[ERROR] Failed to create receive thread");
    }
    
    pthread_detach(r_tid);

    int is_emergency = 0;
    int led_state    = 1;
    set_led(led_state);

    // Initial status messages
    send_status(sock_arm,  ID_ARM,  "System Started");
    send_status(sock_led,  ID_LED,  "LED:ON");
    send_status(sock_btn,  ID_BTN,  "BUTTON:RELEASED");
    send_status(sock_temp, ID_TEMP, "TEMP:INIT");

    unsigned long loop_count = 0;

    printf(">> System running... (LED ON)\n");
    printf(">> Press the button (GPIO17) to trigger EMERGENCY STOP.\n");

    while (1) {
        loop_count++;

        int btn = read_btn();

        // --- Button pressed -> EMERGENCY mode ---
        if (btn == 1 && is_emergency == 0) {
            is_emergency = 1;
            led_state = 0;
            set_led(led_state);

            printf("\n=======================================\n");
            printf("   [ WARNING ] INTERRUPT DETECTED !!   \n");
            printf("=======================================\n");
            printf("   !!! EMERGENCY STOP ACTIVATED !!!    \n");
            printf("=======================================\n\n");

            send_status(sock_arm, ID_ARM, "WARNING - INTERRUPT DETECTED!");
            send_status(sock_btn, ID_BTN, "BUTTON:PRESSED (EMERGENCY)");
            send_status(sock_led, ID_LED, "LED:OFF (EMERGENCY)");
        }

        // --- Button released -> back to normal ---
        if (btn == 0 && is_emergency == 1) {
            is_emergency = 0;
            led_state = 1;
            set_led(led_state);

            printf(">> System restarting...\n");
            send_status(sock_arm, ID_ARM, "System Resumed");
            send_status(sock_btn, ID_BTN, "BUTTON:RELEASED (RUNNING)");
            send_status(sock_led, ID_LED, "LED:ON (RUNNING)");
        }

        // --- Periodic status update (~every 1 second) ---
        if (loop_count % 20 == 0) {  // 0.05s * 20 = 1s
            float temp = read_temperature();
            char buf[256];
            int temp_valid = (temp > -200.0f);  // crude check: below -200 => invalid

            // ARM01: overall summary (mode + temperature)
            if (temp_valid) {
                snprintf(buf, sizeof(buf),
                         "MODE:%s TEMP:%.1fC",
                         is_emergency ? "EMERGENCY" : "RUNNING",
                         temp);
            } else {
                snprintf(buf, sizeof(buf),
                         "MODE:%s TEMP:N/A",
                         is_emergency ? "EMERGENCY" : "RUNNING");
            }
            send_status(sock_arm, ID_ARM, buf);

            // TEMP02: temperature only
            if (temp_valid) {
                snprintf(buf, sizeof(buf), "TEMP:%.1fC", temp);
            } else {
                snprintf(buf, sizeof(buf), "TEMP:N/A");
            }
            send_status(sock_temp, ID_TEMP, buf);

            // BUTTON01: current button state
            snprintf(buf, sizeof(buf),
                     "BUTTON:%s",
                     btn ? "PRESSED" : "RELEASED");
            send_status(sock_btn, ID_BTN, buf);

            // LED01: current LED state
            snprintf(buf, sizeof(buf),
                     "LED:%s",
                     led_state ? "ON" : "OFF");
            send_status(sock_led, ID_LED, buf);
        }

        usleep(50000);  // 0.05 seconds
    }

    // (Normally never reached)
    close(sock_arm);
    close(sock_temp);
    close(sock_btn);
    close(sock_led);
    return 0;
}