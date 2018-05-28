#ifndef	_MNTENT_H
#define	_MNTENT_H	

#include "stdio.h"

#define	_PATH_MOUNTED	"/etc/mtab"
/* Alias to make configure happy */
#define	MOUNTED _PATH_MOUNTED


/*
 * This is an abstraction of an entry in the
 * mount table
 */
struct mntent
{
    char *mnt_fsname;   /* The name of the mounted file system, i.e. the device name */
    char *mnt_dir;		/* Mount point */
    char *mnt_type;		/* String describing the file system type, for instance ext2 */
    char *mnt_opts;		/* Option string */
    int   mnt_freq;		/* How often do we dump - see /etc/fstab */
    int   mnt_passno;	/* How often do we check - see /etc/fstab  */
};

FILE* setmntent(const char *file, const char *mode);
int endmntent (FILE *stream);
struct mntent * getmntent (FILE *stream);

#endif
