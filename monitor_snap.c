// monitor_snap.c
// Tool to kill the Samsung snap cache service: vendor.samsung.hardware.snap-service
//
// Copyright (c) 2025 Flopster101
// SPDX-License-Identifier: MIT

#define _GNU_SOURCE
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

static const char *TARGET = "vendor.samsung.hardware.snap-service";
static volatile int keep_running = 1;

static void handle_sig(int s){ (void)s; keep_running = 0; }

static void logk(const char *fmt, ...) {
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return;
    va_list ap; va_start(ap, fmt);
    vdprintf(fd, fmt, ap);
    va_end(ap);
    close(fd);
}

static int is_numeric(const char *s){
    if(!s || !*s) return 0;
    for(; *s; ++s) if(!isdigit((unsigned char)*s)) return 0;
    return 1;
}

/* Read cmdline without delay; return 1 if contains TARGET, else 0 */
static int cmdline_contains_target_no_sleep(pid_t pid) {
    char path[256], buf[2048];
    ssize_t r;

    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY);
    if(fd < 0) return 0;
    r = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if(r <= 0) return 0;
    buf[r] = '\0';
    for(ssize_t i=0;i<r;i++) if(buf[i]=='\0') buf[i]=' ';
    return (strstr(buf, TARGET) != NULL);
}

/* Check a single pid with a short sleep (for newly-created PID cases).
 *   Returns 1 if killed, 0 otherwise. */
static int check_and_kill_pid_with_delay(pid_t pid) {
    char path[256], buf[2048];
    ssize_t r;

    // small delay to allow cmdline to appear for brand new PIDs
    usleep(30000); // 30 ms

    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    int fd = open(path, O_RDONLY);
    if(fd < 0) return 0;
    r = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if(r <= 0) return 0;
    buf[r] = '\0';
    for(ssize_t i=0;i<r;i++) if(buf[i]=='\0') buf[i]=' ';
    if(strstr(buf, TARGET) != NULL){
        if (kill(pid, SIGKILL) == 0) {
            logk("monitor_snap: killed %d (cmdline: %s)\n", pid, buf);
            return 1;
        } else {
            logk("monitor_snap: failed to kill %d: %s\n", pid, strerror(errno));
        }
    }
    return 0;
}

/* Rate-limited full scan (no sleep per pid). This is cheap if run once/second. */
static void scan_existing_procs(){
    DIR *d = opendir("/proc");
    if(!d) return;
    struct dirent *e;
    while(keep_running && (e = readdir(d)) != NULL){
        if(e->d_type == DT_DIR && is_numeric(e->d_name)){
            pid_t pid = (pid_t)atoi(e->d_name);
            if(cmdline_contains_target_no_sleep(pid)){
                // best-effort kill (no extra delay)
                if (kill(pid, SIGKILL) == 0) {
                    logk("monitor_snap: killed %d (found in full scan)\n", pid);
                } else {
                    logk("monitor_snap: failed to kill %d (full scan): %s\n", pid, strerror(errno));
                }
            }
        }
    }
    closedir(d);
}

static long now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000 + ts.tv_nsec/1000000;
}

int main(int argc, char **argv){
    (void)argc;(void)argv;
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    setpriority(PRIO_PROCESS, 0, 19);

    // initial quick full sweep
    scan_existing_procs();

    int in = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if(in < 0){
        perror("inotify_init");
        return 1;
    }

    // watch for create and moved-to (some kernels use move)
    int wd = inotify_add_watch(in, "/proc", IN_CREATE | IN_MOVED_TO | IN_ATTRIB);
    if(wd < 0){
        perror("inotify_add_watch /proc");
        close(in);
        return 1;
    }

    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    long last_scan = now_ms();

    while(keep_running){
        ssize_t len = read(in, buf, sizeof(buf));
        if(len < 0){
            if(errno == EAGAIN || errno == EINTR){
                // no events right now; do a periodic scan if enough time passed
                long cur = now_ms();
                if(cur - last_scan >= 3000){ // 3s
                    scan_existing_procs();
                    last_scan = cur;
                }
                // small sleep to avoid busy loop
                usleep(100000); // 100ms
                continue;
            } else {
                break;
            }
        }

        // process any events
        for(char *ptr = buf; ptr < buf + len; ){
            struct inotify_event *ev = (struct inotify_event *)ptr;
            if(ev->len) {
                logk("monitor_snap: inotify mask=0x%x name=%s\n", ev->mask, ev->name);
            } else {
                logk("monitor_snap: inotify mask=0x%x (no name)\n", ev->mask);
            }

            // If the event name appears to be a PID, check that PID with a tiny delay
            if(ev->len && is_numeric(ev->name)){
                pid_t pid = (pid_t)atoi(ev->name);
                check_and_kill_pid_with_delay(pid);
            }

            ptr += sizeof(struct inotify_event) + ev->len;
            if(!keep_running) break;
        }

        // After processing events, do a quick rate-limited full scan (once/sec)
        long cur = now_ms();
        if(cur - last_scan >= 1000){
            scan_existing_procs();
            last_scan = cur;
        }
    }

    close(in);
    return 0;
}
