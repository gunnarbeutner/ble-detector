/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/

#include <sys/types.h>

#if !defined(SOCKET_ERROR)
    /** error in socket operation */
    #define SOCKET_ERROR -1
#endif

#define INVALID_SOCKET SOCKET_ERROR
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static int Socket_error(char* aString, int sock)
{
    if (errno != EINTR && errno != EAGAIN && errno != EINPROGRESS && errno != EWOULDBLOCK)
    {
        if (strcmp(aString, "shutdown") != 0 || (errno != ENOTCONN && errno != ECONNRESET))
        {
            int orig_errno = errno;
            char* errmsg = strerror(errno);

            printf("Socket error %d (%s) in %s for socket %d\n", orig_errno, errmsg, aString, sock);
        }
    }
    return errno;
}

int transport_sendPacketBuffer(int mysock, char* host, int port, unsigned char* buf, int buflen)
{
    struct sockaddr_in cliaddr;
    int rc = 0;

    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = inet_addr(host);
    cliaddr.sin_port = htons(port);

    if ((rc = sendto(mysock, buf, buflen, 0, (const struct sockaddr*)&cliaddr, sizeof(cliaddr))) == SOCKET_ERROR)
        Socket_error("sendto", mysock);
    else
        rc = 0;

    return rc;
}

int transport_getdata(int mysock, unsigned char* buf, int count)
{
    int rc = recvfrom(mysock, buf, count, 0, NULL, NULL);
    //printf("received %d bytes count %d\n", rc, (int)count);
    return rc;
}

/**
return >=0 for a socket descriptor, <0 for an error code
*/
int transport_open(void)
{
    int mysock = socket(AF_INET, SOCK_DGRAM, 0);

    if (mysock == INVALID_SOCKET)
        return Socket_error("socket", mysock);

    return mysock;
}

int transport_close(int mysock)
{
    int rc;

    rc = shutdown(mysock, SHUT_WR);
    rc = close(mysock);

    return rc;
}
