/*
 * tty_ld.c
 *
 * This is the source code of the default TTY line discipline
 *
 * Currently the following flags defined by POSIX are processed:
 *
 * Special characters:
 *
 * INTR
 * KILL
 * ERASE
 * SUSP
 * EOF
 * EOL
 * NL
 * CR
 *
 * Input modes:
 *
 * ICRNL
 * IGNCR
 * INLCR
 *
 * Output modes:
 *
 * none
 *
 * Control modes:
 *
 * none
 *
 * Local modes:
 *
 * ICANON
 * ECHO
 * ECHOE
 * ECHOK
 * ECHONL
 * ISIG
 * NOFLSH
 *
 */

#include "tty.h"
#include "tty_ld.h"
#include "lib/string.h"
#include "lib/os/signals.h"
#include "pm.h"
#include "debug.h"
#include "vga.h"

/*
 * Flags used by handle_character
 */
#define CHAR_EOL     1
#define CHAR_DISCARD 2
#define CHAR_ECHO    4
#define CHAR_KILL    8
#define CHAR_DEL    16

/*
 * Initialize a TTY structure
 */
void tty_ld_init(tty_t* tty) {
    spinlock_init(&tty->lock);
    sem_init(&tty->data_available, 0);
    sem_init(&tty->available, 1);
    tty->settings.c_lflag = ICANON + ECHO + ISIG + ECHOE + ECHOK + ECHOCTL;
    tty->settings.c_iflag = 0;
    tty->settings.c_oflag = 0;
    tty->settings.c_cc[VMIN] = 1;
    tty->settings.c_cc[VERASE] = 127;
    tty->settings.c_cc[VEOF] = 4;
    tty->settings.c_cc[VINTR] = 3;
    tty->settings.c_cc[VEOL] = 255;
    tty->settings.c_cc[VKILL] = 21;
    tty->settings.c_cc[VSUSP] = 26;
    tty->settings.orate = B19200;
    tty->settings.irate = B19200;
    tty_ld_flush(tty);
    tty->pgrp = 1;
}



/*
 * Utility function to process a character. This function will
 * 1) check whether the character is an end-of-line character
 * 2) handle line editing commands like DEL or KILL
 * 3) handle signal generation for keys like Ctrl-C
 * 4) potentially change the character to handle things like ICRNL
 * Its return value is the combination of several flags which
 * define how the character is to be handled.
 */
static int handle_character(tty_t* tty, unsigned char* c) {
    int flags = 0;
    /*
     * ICRNL
     */
    if ((*c=='\r') && (tty->settings.c_iflag & ICRNL) && (!(tty->settings.c_iflag & IGNCR)))
        *c='\n';
    /*
     * IGNCR
     */
    if ((*c=='\r') && (tty->settings.c_iflag & IGNCR))
        flags |= CHAR_DISCARD;
    /*
     * INLCR
     */
    else if ((*c=='\n') && (tty->settings.c_iflag & INLCR))
        *c='\r';
    /*
     * EOL of EOF or NL
     */
    if (tty->settings.c_lflag & ICANON) {
        if ((*c=='\n') || (*c==tty->settings.c_cc[VEOL]) || (*c==tty->settings.c_cc[VEOF])) {
            flags |= CHAR_EOL;
        }
    }
    else {
        if ((tty->line_buffer_end + 2) >= tty->settings.c_cc[VMIN]) {
            flags |= CHAR_EOL;
        }
    }
    /*
     * ECHO
     */
    if (tty->settings.c_lflag & ECHO)
        flags |= CHAR_ECHO;
    /*
     * ERASE
     */
    if ((*c==tty->settings.c_cc[VERASE]) && (tty->settings.c_lflag & ICANON)) {
        flags |= (CHAR_DISCARD + CHAR_DEL);
        if (tty->line_buffer_end>=0) {
            tty->line_buffer_end--;
        }
        else {
            /*
             * Do not echo ERASE if the line is empty
             */
            flags &= ~CHAR_ECHO;
        }
        /*
         * Clear echo flag if ECHOE is not set
         */
        if (!(tty->settings.c_lflag & ECHOE))
            flags &= ~CHAR_ECHO;
    }
    /*
     * KILL (Ctrl-U by default)
     */
    if ((*c==tty->settings.c_cc[VKILL]) && (tty->settings.c_lflag & ICANON)) {
            flags |= (CHAR_DISCARD+CHAR_KILL+CHAR_ECHO);
            if (!(tty->settings.c_lflag & ECHOK))
                flags &= ~CHAR_ECHO;
    }
    /*
     * INTR (Ctrl-C by default)
     */
    if ((*c==tty->settings.c_cc[VINTR]) && (tty->settings.c_lflag & ISIG)) {
        do_kill(-tty->pgrp, __KSIGINT);
        if (!(tty->settings.c_lflag & NOFLSH))
            tty_ld_flush(tty);
        flags |= CHAR_DISCARD;
        flags &= ~CHAR_EOL;
    }
    /*
     * SUSP (Ctrl-Z by default)
     */
    if ((*c==tty->settings.c_cc[VSUSP]) && (tty->settings.c_lflag & ISIG)) {
        do_kill(-tty->pgrp, __KSIGTSTP);
        if (!(tty->settings.c_lflag & NOFLSH))
            tty_ld_flush(tty);
        flags |= CHAR_DISCARD;
        flags &= ~CHAR_EOL;
    }
    /*
     * If ICANON and ECHONL are set, echo NL even if ECHO is not set
     */
    if ((*c=='\n') && ((tty->settings.c_lflag & (ICANON + ECHONL))==(ICANON+ECHONL)))
        flags |= CHAR_ECHO;
    return flags;
}



/*
 * Add the characters received to the line buffer. If the end-of-line has been reached,
 * transfer the entire buffer to the read buffer of the terminal. If either the line buffer
 * or the read buffer are not able to hold the additional characters, discard input and return
 * Parameters:
 * @tty - the tty on which we operate (we assume that we hold the lock on this tty)
 * @input - the characters to add
 * @nbytes - the number of bytes
 * Return value:
 * 1 if data has been copied to the read buffer
 * 0 if no data has been copied
 */
int tty_ld_put(tty_t* tty, unsigned char* input, size_t nbytes) {
    int i;
    int action_flags;
    int data_available = 0;
    int old_line_end = tty->line_buffer_end;
    unsigned char tmp[MAX_INPUT];
    unsigned char echo[2];
    unsigned char del;
    /*
     * Save a copy of the old line buffer to be able to revert the buffer to
     * its previous state
     */
    memcpy(tmp, tty->line_buffer, MAX_INPUT);
    if (MAX_INPUT-tty->line_buffer_end-1 < nbytes) {
        return 0;
    }
    for (i=0;i<nbytes;i++) {
        /*
         * Determine action and handle special characters
         */
        action_flags = handle_character(tty, &input[i]);
        /*
         * Add byte to current line if DISCARD flag is not set and echo it
         * if ECHO flag is set
         */
        if (!(action_flags & CHAR_DISCARD))
            tty->line_buffer[++tty->line_buffer_end]=input[i];
        if (action_flags & CHAR_ECHO) {
            /*
             * If the character is a control character and ECHOCTL is set,
             * echo 0x40 + the character, unless it is tab or NL
             */
            if ((input[i]<32) && (tty->settings.c_lflag & ECHOCTL) && (input[i]!='\n') && (input[i]!='\t')
                    && (0==(action_flags & CHAR_DISCARD))) {
                echo[0] = 0x40 + input[i];
                tty_ld_write(tty, echo, 1);
            }
            else if (0==(action_flags & CHAR_DISCARD))
                tty_ld_write(tty, input+i, 1);
        }
        /*
         * If KILL flag is set and echo is requested, send appropriate number of DELs to console
         */
        if ((action_flags & (CHAR_KILL+CHAR_ECHO)) == CHAR_KILL+CHAR_ECHO) {
            del = 127;
            for (i=0;i<tty->line_buffer_end+1;i++)
                tty_ld_write(tty, &del, 1);
        }
        /*
         * Clear line buffer if KILL is requested
         */
        if (action_flags & CHAR_KILL) {
            tty->line_buffer_end = -1;
        }
        /*
         * If DEL flag is set and echo is requested, send DEL to console
         */
        if ((action_flags & (CHAR_DEL+CHAR_ECHO))==CHAR_DEL+CHAR_ECHO) {
            del = 127;
            tty_ld_write(tty, &del, 1);
        }
        /*
         * If line is complete, transfer it to read buffer
         */
        if (action_flags & CHAR_EOL) {
            /*
             * Check whether the read line has enough capacity left to hold the entire
             * current line
             */
            if (MAX_INPUT-tty->read_buffer_end-1 < tty->line_buffer_end+1) {
                /*
                 * Discard what we have just placed in the line buffer and return.
                 */
                memcpy(tty->line_buffer, tmp, MAX_INPUT);
                tty->line_buffer_end = old_line_end;
                return 0;
            }
            else {
                /*
                 * Copy current line to read buffer and reset line buffer
                 */
                memcpy(tty->read_buffer+tty->read_buffer_end+1, tty->line_buffer, tty->line_buffer_end+1);
                tty->read_buffer_end += tty->line_buffer_end+1;
                tty->line_buffer_end = -1;
                data_available = 1;
            }
        }
    }
    return data_available;
}

/*
 * Read data from read buffer
 * Parameters:
 * @tty - the tty structure to be used
 * @data - the buffer to which we copy the data
 * @nbytes - number of bytes to be copied
 * Return value:
 * number of bytes which have been read
 */
ssize_t tty_ld_read(tty_t* tty, unsigned char* data, size_t nbytes) {
    size_t chars_read = 0;
    size_t chars_processed = 0;
    int i;
    /*
     * Start to transfer data until either nbytes have been transfered
     * or the read buffer is empty or we hit upon an end-of-line
     */
    for (i = 0; (i < tty->read_buffer_end+1) && (i < nbytes); i++) {
        /*
         * In canonical mode, skip an EOF byte
         */
        if (((tty->settings.c_lflag & ICANON)==0) || (tty->read_buffer[i]!=tty->settings.c_cc[VEOF])) {
            ((char*) data)[i] = tty->read_buffer[i];
            chars_read++;
        }
        chars_processed++;
        if (IS_EOL(tty->read_buffer[i], tty->settings) && (tty->settings.c_lflag & ICANON)) {
            break;
        }
    }
    /*
     * Move remaining data to start of buffer
     */
    for (i = chars_processed; i < tty->read_buffer_end+1; i++) {
        tty->read_buffer[i - chars_read] = tty->read_buffer[i];
    }
    tty->read_buffer_end -= chars_processed;
    return chars_read;
}

/*
 * Flush a tty structure, i.e. discard all data
 */
void tty_ld_flush(tty_t* tty) {
    tty->line_buffer_end = -1;
    tty->read_buffer_end = -1;
}


/*
 * Write data to a tty
 */
ssize_t tty_ld_write(tty_t* tty,unsigned char* buffer, size_t size) {
    size_t i;
    int j;
    unsigned char c;
    for (i = 0; i < size; i++) {
        c = buffer[i];
        if (c == tty->settings.c_cc[VKILL]) {
            for (j=0;j<tty->line_buffer_end+1;j++) {
                kputchar(127);
            }
        }
        else if (c == tty->settings.c_cc[VERASE]) {
            kputchar(127);
        }
        else {
            kputchar(((char*) buffer)[i]);
        }
    }
    return size;
}
