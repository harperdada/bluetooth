/*
 * How many unique BLE advertisers have been seen recently with signal stronger than -75 dBm?
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

// --- Configuration ---
#define RSSI_THRESHOLD  -75          // Only count devices seen stronger than -75 dBm
#define DEVICE_TIMEOUT  (5 * 60)     // Device is considered "gone" after 5 minutes (in seconds)
#define AGGREGATION_INTERVAL  (5 * 60) // Aggregate and save data every 1 hour (in seconds)
#define HASH_TABLE_SIZE  1024        // Size of the hash table array (must be power of 2)
#define OUTPUT_FILE     "foot_traffic_log.csv"
// ---------------------

// Global flag for control-C handler
static volatile int keep_running = 1;

// Global lock for hash table access (since the scanner and cleanup thread both use it)
pthread_mutex_t hash_table_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Hash Table Structures ---

// A single entry in the hash table
typedef struct DeviceEntry {
    char addr[18];        // MAC Address string (e.g., "AA:BB:CC:DD:EE:FF")
    time_t last_seen;     // Unix timestamp of when the device was last observed
    struct DeviceEntry *next; // For collision resolution (Separate Chaining)
} DeviceEntry;

// The hash table itself (an array of pointers to DeviceEntry)
DeviceEntry *dev_table[HASH_TABLE_SIZE] = {NULL};

// --- Hash Table Functions ---

// Simple DJB2 hash function for strings
unsigned long hash_mac_addr(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash & (HASH_TABLE_SIZE - 1); // Mask to table size
}

// Search for a device entry. Returns the entry or NULL if not found.
DeviceEntry *find_device(const char *addr) {
    unsigned long index = hash_mac_addr(addr);
    DeviceEntry *current = dev_table[index];

    while (current != NULL) {
        if (strcmp(current->addr, addr) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Add or update a device entry.
void update_device(const char *addr, time_t now) {
    DeviceEntry *entry = find_device(addr);

    if (entry != NULL) {
        // Device found, just update the timestamp
        entry->last_seen = now;
    } else {
        // Device not found, create a new entry
        unsigned long index = hash_mac_addr(addr);
        entry = (DeviceEntry *)malloc(sizeof(DeviceEntry));
        if (entry == NULL) {
            perror("malloc");
            return;
        }

        strncpy(entry->addr, addr, sizeof(entry->addr) - 1);
        entry->addr[sizeof(entry->addr) - 1] = '\0';
        entry->last_seen = now;

        // Prepend to the front of the list at the index
        entry->next = dev_table[index];
        dev_table[index] = entry;
    }
}

// Remove entries that have expired (i.e., not seen for DEVICE_TIMEOUT seconds)
void prune_expired_devices() {
    time_t now = time(NULL);
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        DeviceEntry **current_ptr = &dev_table[i];
        
        while (*current_ptr != NULL) {
            DeviceEntry *current = *current_ptr;
            if (now - current->last_seen > DEVICE_TIMEOUT) {
                // Device has expired, remove it
                *current_ptr = current->next; // Bypass the current node
                free(current);
            } else {
                // Device is still active, move to the next
                current_ptr = &current->next;
            }
        }
    }
}

// Count the number of unique devices currently in the hash table
int get_active_device_count() {
    int count = 0;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        DeviceEntry *current = dev_table[i];
        while (current != NULL) {
            count++;
            current = current->next;
        }
    }
    return count;
}

// Free all memory used by the hash table
void free_hash_table() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        DeviceEntry *current = dev_table[i];
        DeviceEntry *next;
        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }
        dev_table[i] = NULL;
    }
}

// --- Data Logging ---

void save_traffic_log(time_t timestamp, int count) {
    FILE *fp = fopen(OUTPUT_FILE, "a");
    if (fp == NULL) {
        perror("Could not open foot_traffic_log.csv");
        return;
    }

    // Format timestamp for CSV (e.g., "YYYY-MM-DD HH:MM:SS")
    struct tm *tm_info = localtime(&timestamp);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Write to CSV: Timestamp, Unique_Device_Count
    fprintf(fp, "%s,%d\n", time_str, count);
    
    fclose(fp);
}

// --- Background Pruning/Aggregation Thread ---

void *aggregation_thread(void *arg) {
    time_t last_aggregation_time = time(NULL);
    
    // Always overwrite the file and write the header on startup.
    FILE *fp = fopen(OUTPUT_FILE, "w");
    if (fp != NULL) {
        fprintf(fp, "Timestamp,Unique_Device_Count\n");
        fclose(fp);
    } else {
        perror("Failed to open output file for header.");
    }
    
    while (keep_running) {
        sleep(10); // Check every 10 seconds

        pthread_mutex_lock(&hash_table_mutex);
        
        // 1. Prune expired devices frequently
        prune_expired_devices();

        // 2. Aggregate and save data on the interval
        time_t now = time(NULL);
        if (now - last_aggregation_time >= AGGREGATION_INTERVAL) {
            int current_traffic = get_active_device_count();
            save_traffic_log(now, current_traffic);
            
            // Adjust the next aggregation time to be exactly on the interval
            last_aggregation_time += AGGREGATION_INTERVAL;
            printf("--- Aggregation Saved --- Active Devices: %d\n", current_traffic);
        }

        pthread_mutex_unlock(&hash_table_mutex);
    }
    return NULL;
}


// --- Main Program Logic ---

void int_handler(int dummy) {
    keep_running = 0;
}

int main(void)
{
    // === ADD THIS LINE ===
    setvbuf(stdout, NULL, _IONBF, 0);
    int dev_id, sock;
    struct hci_filter old_filter, new_filter;
    socklen_t old_filter_len;
    int err;
    pthread_t agg_tid; // Aggregation thread ID

    signal(SIGINT, int_handler); // Ctrl+C to stop

    // Start the aggregation and cleanup thread
    if (pthread_create(&agg_tid, NULL, aggregation_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    // Get first available HCI device
    dev_id = 0; // Explicitly use hci0
    if (dev_id < 0) { // Keep the check just in case, though it won't be used now
        perror("hci_get_route");
        goto thread_cleanup;
    }

    // Open HCI socket
    sock = hci_open_dev(dev_id);
    if (sock < 0) {
        perror("hci_open_dev");
        goto thread_cleanup;
    }

    // Save current socket filter
    old_filter_len = sizeof(old_filter);
    if (getsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, &old_filter_len) < 0) {
        perror("getsockopt");
        goto cleanup;
    }

    // Set new filter: only LE meta events
    hci_filter_clear(&new_filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
    if (setsockopt(sock, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0) {
        perror("setsockopt");
        goto cleanup;
    }

    // Set BLE scan parameters (Active Scanning)
    err = hci_le_set_scan_parameters(
        sock, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 1000
    );
    if (err < 0) {
        perror("hci_le_set_scan_parameters");
        goto cleanup;
    }

    // Enable scanning (Filter Duplicates Enabled)
    err = hci_le_set_scan_enable(
        sock, 0x01, 0x01, 1000
    );
    if (err < 0) {
        perror("hci_le_set_scan_enable");
        goto cleanup;
    }

    printf("BLE foot traffic counter started. RSSI > %d dBm. Press Ctrl+C to stop.\n", RSSI_THRESHOLD);
    printf("Data is logged to %s every %d minutes.\n", OUTPUT_FILE, AGGREGATION_INTERVAL / 60);

    // --- Main Scanning Loop ---
    while (keep_running) {
        unsigned char buf[HCI_MAX_EVENT_SIZE];
        int len;

        // Use a timeout for read so the loop can check `keep_running`
        struct timeval tv = {0, 500000}; // 0.5 sec timeout
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        
        if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0) {
            continue; // Timeout or error, check keep_running
        }

        len = read(sock, buf, sizeof(buf));
        if (len < 0 && keep_running) {
            if (errno == EINTR) continue; // Interrupted by signal
            perror("read");
            break;
        }
        if (len < 0) continue; // Error during read

        // Parse the LE Meta Event packet
        evt_le_meta_event *meta;
        meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));

        if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
            continue;

        uint8_t reports_count = meta->data[0];
        uint8_t *ptr = meta->data + 1;

        for (uint8_t i = 0; i < reports_count; i++) {
            le_advertising_info *info = (le_advertising_info *)ptr;

            char addr[18];
            ba2str(&info->bdaddr, addr);

            uint8_t data_len = info->length;
            int8_t rssi = (int8_t)info->data[data_len];

            // --- Core Logic: Filter and Track ---
            if (rssi > RSSI_THRESHOLD) {
                pthread_mutex_lock(&hash_table_mutex);
                update_device(addr, time(NULL));
                pthread_mutex_unlock(&hash_table_mutex);
                // printf("WALK-IN: %s, RSSI: %d dBm\n", addr, rssi); // Debug line
            }

            // Move pointer to next report
            ptr += sizeof(le_advertising_info) + data_len + 1;
        }
    }

    // --- Cleanup ---
cleanup:
    // Disable scanning
    hci_le_set_scan_enable(sock, 0x00, 0x00, 1000);

    // Restore original filter
    setsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));

    close(sock);
    printf("\nBLE scan stopped.\n");

thread_cleanup:
    // Signal the thread to stop and wait for it
    keep_running = 0;
    pthread_join(agg_tid, NULL);
    
    // Free all hash table memory
    free_hash_table();
    
    printf("Foot traffic counter program finished.\n");
    return 0;
}
