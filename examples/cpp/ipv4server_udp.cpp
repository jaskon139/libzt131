/*
 * ZeroTier SDK - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include <stdio.h>
#include <string.h>
#include <string>
#include <inttypes.h>

#if defined(_WIN32)
#include <WinSock2.h>
#include <stdint.h>
#else
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "ZeroTierSockets.h"


bool nodeReady = false;
bool networkReady = false;

// Example callbacks
void myZeroTierEventCallback(void *msgPtr)
{
	struct zts_callback_msg *msg = (struct zts_callback_msg *)msgPtr;

	// Node events
	if (msg->eventCode == ZTS_EVENT_NODE_ONLINE) {
		//printf("ZTS_EVENT_NODE_ONLINE --- This node's ID is %llx\n", msg->node->address);
		nodeReady = true;
	}
	if (msg->eventCode == ZTS_EVENT_NODE_OFFLINE) {
		//printf("ZTS_EVENT_NODE_OFFLINE --- Check your physical Internet connection, router, firewall, etc. What ports are you blocking?\n");
		nodeReady = false;
	}
	if (msg->eventCode == ZTS_EVENT_NODE_NORMAL_TERMINATION) {
		//printf("ZTS_EVENT_NODE_NORMAL_TERMINATION\n");
	}

	// Virtual network events
	if (msg->eventCode == ZTS_EVENT_NETWORK_NOT_FOUND) {
		//printf("ZTS_EVENT_NETWORK_NOT_FOUND --- Are you sure %llx is a valid network?\n",
			// msg->network->nwid);
	}
	if (msg->eventCode == ZTS_EVENT_NETWORK_REQ_CONFIG) {
		//printf("ZTS_EVENT_NETWORK_REQ_CONFIG --- Requesting config for network %llx, please wait a few seconds...\n", msg->network->nwid);
	} 
	if (msg->eventCode == ZTS_EVENT_NETWORK_ACCESS_DENIED) {
		//printf("ZTS_EVENT_NETWORK_ACCESS_DENIED --- Access to virtual network %llx has been denied. Did you authorize the node yet?\n",
			// msg->network->nwid);
	}
	if (msg->eventCode == ZTS_EVENT_NETWORK_READY_IP4) {
		//printf("ZTS_EVENT_NETWORK_READY_IP4 --- Network config received. IPv4 traffic can now be sent over network %llx\n",
			// msg->network->nwid);
		networkReady = true;
	}
	if (msg->eventCode == ZTS_EVENT_NETWORK_READY_IP6) {
		//printf("ZTS_EVENT_NETWORK_READY_IP6 --- Network config received. IPv6 traffic can now be sent over network %llx\n",
			// msg->network->nwid);
		networkReady = true;
	}
	if (msg->eventCode == ZTS_EVENT_NETWORK_DOWN) {
		//printf("ZTS_EVENT_NETWORK_DOWN --- %llx\n", msg->network->nwid);
	}

	// Address events
	if (msg->eventCode == ZTS_EVENT_ADDR_ADDED_IP4) {
		char ipstr[ZTS_INET_ADDRSTRLEN];
		struct zts_sockaddr_in *in4 = (struct zts_sockaddr_in*)&(msg->addr->addr);
		zts_inet_ntop(ZTS_AF_INET, &(in4->sin_addr), ipstr, ZTS_INET_ADDRSTRLEN);
		//printf("ZTS_EVENT_ADDR_NEW_IP4 --- This node's virtual address on network %llx is %s\n", msg->addr->nwid, ipstr);
	}
	if (msg->eventCode == ZTS_EVENT_ADDR_ADDED_IP6) {
		char ipstr[ZTS_INET6_ADDRSTRLEN];
		struct zts_sockaddr_in6 *in6 = (struct zts_sockaddr_in6*)&(msg->addr->addr);
		zts_inet_ntop(ZTS_AF_INET6, &(in6->sin6_addr), ipstr, ZTS_INET6_ADDRSTRLEN);
		//printf("ZTS_EVENT_ADDR_NEW_IP6 --- This node's virtual address on network %llx is %s\n", msg->addr->nwid, ipstr);
	}
	if (msg->eventCode == ZTS_EVENT_ADDR_REMOVED_IP4) {
		char ipstr[ZTS_INET_ADDRSTRLEN];
		struct zts_sockaddr_in *in4 = (struct zts_sockaddr_in*)&(msg->addr->addr);
		zts_inet_ntop(ZTS_AF_INET, &(in4->sin_addr), ipstr, ZTS_INET_ADDRSTRLEN);
		//printf("ZTS_EVENT_ADDR_REMOVED_IP4 --- The virtual address %s for this node on network %llx has been removed.\n", ipstr, msg->addr->nwid);
	}
	if (msg->eventCode == ZTS_EVENT_ADDR_REMOVED_IP6) {
		char ipstr[ZTS_INET6_ADDRSTRLEN];
		struct zts_sockaddr_in6 *in6 = (struct zts_sockaddr_in6*)&(msg->addr->addr);
		zts_inet_ntop(ZTS_AF_INET6, &(in6->sin6_addr), ipstr, ZTS_INET6_ADDRSTRLEN);
		//printf("ZTS_EVENT_ADDR_REMOVED_IP6 --- The virtual address %s for this node on network %llx has been removed.\n", ipstr, msg->addr->nwid);
	}
	// Peer events
	if (msg->eventCode == ZTS_EVENT_PEER_DIRECT) {
		//printf("ZTS_EVENT_PEER_DIRECT --- node=%llx\n", msg->peer->address);
		// A direct path is known for nodeId
	}
	if (msg->eventCode == ZTS_EVENT_PEER_RELAY) {
		//printf("ZTS_EVENT_PEER_RELAY --- node=%llx\n", msg->peer->address);
		// No direct path is known for nodeId
	}
}

int localupdsock; 
int ztoudpsock;
struct sockaddr_in clientAddr;
struct zts_sockaddr acc_in4;

void *thread1(void *)
{
    struct sockaddr_in addr;
    addr.sin_family     = AF_INET;
    addr.sin_port       = htons(9876);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

	clientAddr.sin_family     = AF_INET;
    clientAddr.sin_port       = htons(3824);
    clientAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    char buff_recv[65535] = {0};
    
    int n;
    socklen_t len = sizeof(clientAddr);

    printf("Welcome! This is a UDP server.\n");

    if ((localupdsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket error.\n");
        exit(1);
    }

    if (bind(localupdsock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        printf("bind error.\n");
        exit(1);
    }

    while (1)
    {
        n = recvfrom(localupdsock, buff_recv, sizeof(buff_recv), 0, (struct sockaddr *) &clientAddr, &len);
        if (n > 0)
        {
            buff_recv[n] = 0;
            // printf("recv data from client:%s %u says: %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buff_recv);

            n = zts_sendto(ztoudpsock, buff_recv, n, 0, (struct zts_sockaddr *) &acc_in4, sizeof(acc_in4));
            if (n < 0)
            {
                printf("sendto error.\n");
                // break;
            }
        }
        else 
        {
            printf("recv error.\n");
            // break;
        }
    }
}

void thread_create(void)   //创建两个线程
{
        int temp;
		pthread_t thread =0;        
        /*创建线程*/
        if((temp = pthread_create(&thread, NULL, thread1, NULL)) != 0)  //comment2     
                printf("线程1创建失败!/n");
        else
                printf("线程1被创建/n");
}

size_t get_executable_path( char* processdir,char* processname, size_t len)
{
    char* path_end;
    if(readlink("/proc/self/exe", processdir,len) <=0)
    {
          return -1;
    }
	printf("get current path is :%s\n", processdir);
    path_end = strrchr(processdir,  '/');
    if(path_end == NULL)
    {
          return -1;
    }
    ++path_end;
    strcpy(processname, path_end);
    *path_end = '\0';
 
    return (size_t)(path_end - processdir);
}

int main(int argc, char **argv)
{
	thread_create();
	char binpath[256];
	char process[256];
 
	get_executable_path(binpath, process, sizeof(binpath));
	printf("---------->>> current path is : %s\n", binpath);
	printf("---------->>> current process is : %s\n", process);

	// 	if (argc != 5) {
	// 	printf("\nlibzt example server\n");
	// 	printf("server <config_file_path> <nwid> <serverBindPort> <ztServicePort>\n");
	// 	exit(0);
	// }
	uint64_t nwid = 0x17d709436c911e4fl; // Network ID to join
	int serverBindPort = 9999;  //atoi(argv[3]); // Port the application should bind to
	int ztServicePort = 9555; //atoi(argv[4]); // Port ZT uses to send encrypted UDP packets to peers (try something like 9994)
	
	struct zts_sockaddr_in in4;	
	in4.sin_port = zts_htons(serverBindPort);
#if defined(_WIN32)
	in4.sin_addr.S_addr = ZTS_INADDR_ANY;
#else
	in4.sin_addr.s_addr = ZTS_INADDR_ANY;
#endif
	in4.sin_family = ZTS_AF_INET;

	// Bring up ZeroTier service and join network

	int  accfd=0;
	int err = ZTS_ERR_OK;

	if((err = zts_start(binpath, &myZeroTierEventCallback, ztServicePort)) != ZTS_ERR_OK) {
		printf("Unable to start service, error = %d. Exiting.\n", err);
		exit(1);
	}
	printf("Waiting for node to come online...\n");
	while (!nodeReady) { zts_delay_ms(50); }
	printf("This node's identity is stored in %s\n", argv[1]);

	if((err = zts_join(nwid)) != ZTS_ERR_OK) {
		printf("Unable to join network, error = %d. Exiting.\n", err);
		exit(1);
	}
	printf("Joining network %llx\n", nwid);
	printf("Don't forget to authorize this device in my.zerotier.com or the web API!\n");
	while (!networkReady) { zts_delay_ms(50); }

	// Socket-like API example

	printf("Creating socket...\n");	
	if ((ztoudpsock = zts_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("error creating ZeroTier socket\n");
	}

	printf("Binding...\n");	
	if ((err = zts_bind(ztoudpsock, (struct zts_sockaddr *)&in4, sizeof(struct zts_sockaddr_in)) < 0)) {
		printf("error binding to interface (%d)\n", err);
	}
	
	int bytes=0, flags=0, r=0;
	char recvBuf[65536];
	memset(recvBuf, 0, sizeof(recvBuf));

	printf("reading from client...\n");
	socklen_t addrlen = sizeof(acc_in4);
	memset(&acc_in4, 0, sizeof acc_in4);
	memset(&recvBuf, 0, sizeof recvBuf);

	while(true) {
		// memset(&recvBuf, 0, sizeof recvBuf);
		r = zts_recvfrom(ztoudpsock, recvBuf, sizeof(recvBuf), flags, &acc_in4, &addrlen);
		if (r >= 0) {
			// char *ip = inet_ntoa(acc_in4.sa_data);
			// printf("Received : r=%d, %s -- from: %s : %d\n", r, recvBuf, "222", acc_in4.sa_family);

			r = sendto(localupdsock, recvBuf, r, 0, (struct sockaddr *) &clientAddr, sizeof(clientAddr));
            if (r < 0)
            {
                printf("sendto error.\n");                
            }
		}
	}

/*
	printf("sending to client...\n");
	w = zts_write(accfd, rbuf, strlen(rbuf));

*/

	err = zts_close(ztoudpsock);

	return err;
}
