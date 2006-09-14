#include "arp.h"

//int main() {
//	arp_set("192.168.2.64","00:00:00:64:64:64");
//	arp_del("192.168.2.64");
//	exit(0);
//}

int arp_set(char* ip, char* mac) {
	int sockfd;
	struct arpreq req;
	struct ether_addr* mac_addr;
	struct hostent* hp;
	
	//get enough memory for struct
	memset((char*) &req,0,sizeof(req));
	
	//fill in ip address
	hp = gethostbyname(ip);
	req.arp_pa.sa_family=AF_INET;
	memcpy(&(req.arp_pa.sa_data[2]),hp->h_addr, hp->h_length);

	//fill in mac address
	req.arp_ha.sa_family=ARPHRD_ETHER;
	mac_addr = ether_aton((const char*)mac);
	memcpy(&(req.arp_ha.sa_data[0]),mac_addr->ether_addr_octet,ETHER_ADDR_LEN);

	//set needed flags (commit and add permanently)
	req.arp_flags = ATF_PERM | ATF_COM;
	
	//set correct device
	strcpy(req.arp_dev, "");	//alternatively: "eth0"

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	/* Call the kernel. */
	if (ioctl(sockfd, SIOCSARP, &req) < 0) {
		perror("SIOCSARP");
		close(sockfd);
		return (0);
	}
	close(sockfd);
	return (1);
}

int arp_del(char* ip) {
        int sockfd;
        struct arpreq req;
        struct hostent* hp;

        //get enough memory for struct
        memset((char*) &req,0,sizeof(req));

        //fill in ip address
        hp = gethostbyname(ip);
        req.arp_pa.sa_family=AF_INET;
        memcpy(&(req.arp_pa.sa_data[2]),hp->h_addr, hp->h_length);

        //set needed flags (commit and add permanently)
        req.arp_flags = ATF_COM;

        //set correct device
        strcpy(req.arp_dev, "");        //alternatively: "eth0"

        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        /* Call the kernel. */
        if (ioctl(sockfd, SIOCDARP, &req) < 0) {
                perror("SIOCDARP");
                close(sockfd);
                return (0);
        }
        close(sockfd);
        return (1);
}

