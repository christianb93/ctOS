#include <mntent.h>
#include <stdio.h>

int main() {
    struct mntent *fs;
    FILE* fp;
    fp = setmntent(MOUNTED, "r");
    if (0 == fp) {
        printf("Could not open file %s\n", MOUNTED);
        return 1;
    }
    while ((fs = getmntent(fp)) != NULL) {
        printf("File system name:    %s\n", fs->mnt_fsname);
        printf("Mounted on:          %s\n", fs->mnt_dir);
        printf("Filesystem type:     %s\n", fs->mnt_type);
        printf("Options:             %s\n", fs->mnt_opts);
        printf("Dump frequency:      %d\n", fs->mnt_freq);
        printf("Passes to fsck:      %d\n", fs->mnt_passno);
    }
    endmntent(fp);
    return 0;
}

