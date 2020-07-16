#include <myutils.h>

/* tratto da “Advanced Programming In the UNIX Environment”
 * by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley
 *
 * if we encounter an error and have previously read or written
 * any data, we return the amount of data transferred instead
 * of the error.
 * Similarly, if we reach the end of file while reading,
 * we return the number of bytes copied to the caller’s buffer
 * if we already read some data successfully and have not yet
 * satisfied the amount requested.
 */

ssize_t readn(int fd, void *ptr, size_t n) {
    size_t   nleft;
    ssize_t  nread;

    nleft = n;
    while (nleft > 0) {
        if((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */
        nleft -= nread;
        ptr   = (char *)ptr + nread;
    }
    return(n - nleft); /* return >= 0 */
}

ssize_t writen(int fd, void *ptr, size_t n) {
    size_t   nleft;
    ssize_t  nwritten;

    nleft = n;
    while (nleft > 0) {
        if((nwritten = write(fd, ptr, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount written so far */
        } else if (nwritten == 0) break;
        nleft -= nwritten;
        ptr   = (char *)ptr + nwritten;
    }
    return(n - nleft); /* return >= 0 */
}

void strip_spaces(char* str) {
    int i = 0, j = 0;
    while (str[j] != '\0') {
        while (str[j] != '\0' && isspace(str[j])) { j++; }
        str[i++] = str[j++];
    }
    str[i] = '\0';
}

int millitimespec(struct timespec *ts, const int timeout_ms) {
    if(timeout_ms <= 0) {
        fprintf(stderr, "MILLISLEEP: timeout_ms must be > 0\n");
        errno = EINVAL;
        return -1;
    }
    ts->tv_sec = timeout_ms / 1000;
    ts->tv_nsec = (timeout_ms % 1000) * msTOnsMULT;

    return 0;
}


int millisleep(const int ms) {
    int err;
    struct timespec towait = {0, 0},
                    remaining = {0, 0};

    if(ms <= 0) {
        fprintf(stderr, "MILLISLEEP: ms must be > 0\n");
        errno = EINVAL;
        return -1;
    }

    towait.tv_sec = ms / 1000;
    towait.tv_nsec = (ms % 1000) * msTOnsMULT;

    do {
        err = nanosleep(&towait, &remaining);
        towait = remaining;
    } while(err != 0);

    return 0;
}


/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */
/*
 * @FROM
 *  https://www.gnu.org/software/libc/manual/html_node/Calculating-Elapsed-Time.html
 */
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}


