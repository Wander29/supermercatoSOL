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