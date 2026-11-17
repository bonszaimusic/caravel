/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* A cold boot retains NOR bytes, never the previous process's settings RAM. */
#define FORMAT2_EDITOR_MAIN release_boot_fixture_main
#include "format2_editor_test.c"
#include <unistd.h>
#include <sys/wait.h>

static int cold_boot(const char *path, int first)
{
    FILE *f;
    if (first) web_nor_erase();
    else {
        f = fopen(path, "rb"); assert(f);
        assert(fread(nor, 1, sizeof nor, f) == sizeof nor); fclose(f);
        assert(!(UXL_SEEN & 0x80u)); /* fresh RAM before settings_import */
    }
    web_boot();
    assert(uxl.play && uxl.lvl == !!first && (UXL_SEEN & 0x80u));
    for (unsigned i = 0; i < 2400; i++) web_render(44);
    persist_t disk;
    assert(st_load(OBJ_SETTINGS, &disk, sizeof disk) == sizeof disk);
    assert(disk.favorites.factory[15][31] & 0x80u);
    if (first) {
        f = fopen(path, "wb"); assert(f);
        assert(fwrite(nor, 1, sizeof nor, f) == sizeof nor); fclose(f);
    } else assert(!uxl.lvl);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3) return cold_boot(argv[2], !strcmp(argv[1], "first"));
    char path[] = "/tmp/caravel-cold-boot-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0); close(fd);
    for (unsigned i = 0; i < 2; i++) {
        pid_t pid = fork(); assert(pid >= 0);
        if (!pid) { execl(argv[0], argv[0], i ? "second" : "first", path, (char *)0); _exit(127); }
        int status; assert(waitpid(pid, &status, 0) == pid);
        assert(WIFEXITED(status) && !WEXITSTATUS(status));
    }
    unlink(path);
    puts("release boot: PLAY and LEVELS once; seen bit committed to NOR and restored in a fresh process");
    return 0;
}
