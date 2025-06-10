#ifndef UDP_H
#define UDP_H

#include <vector>
#include <cstring>
#include <string>
#include <cstdio>
#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>

class udp_rcv
{
private:
    char buffer[500];
    char *tcp_ptr = nullptr; // posicion acutal de leer
    char *tcp = nullptr;     // tcp reensamblado
    int tcp_len = 0;         // tam tcp
    std::vector<char *> vc;  // fragmentos recibidos

    socklen_t addr_len = sizeof(from_addr);
    int sock;

    int total_pkts = 0;
    int total_tcp_size = 0;

public:
    struct sockaddr_in from_addr;
    udp_rcv(int socket)
    {
        sock = socket;
    }

    ~udp_rcv()
    {
        // Liberar memoria dinámica de los fragmentos
        for (char *frag : vc)
            delete[] frag;

        if (tcp)
        {
            delete[] tcp;
            tcp = nullptr;
            tcp_ptr = nullptr;
            tcp_len = 0;
        }
    }

    udp_rcv(const udp_rcv &other)
    {
        sock = other.sock;
        total_pkts = other.total_pkts;
        total_tcp_size = other.total_tcp_size;
        tcp_len = other.tcp_len;

        if (other.tcp && tcp_len > 0)
        {
            // copia de tcp
            tcp = new char[tcp_len];
            std::memcpy(tcp, other.tcp, tcp_len);

            // tcp_ptr pos relativa
            ptrdiff_t offset = other.tcp_ptr - other.tcp;
            tcp_ptr = tcp + offset;
        }
        else
        {
            tcp = nullptr;
            tcp_ptr = nullptr;
        }
    }

    void recibir()
    {
        // reset variables
        total_pkts = 0;
        total_tcp_size = 0;

        for (char *frag : vc)
            delete[] frag;
        vc.clear();

        if (tcp)
        {
            delete[] tcp;
            tcp = nullptr;
            tcp_ptr = nullptr;
            tcp_len = 0;
        }

        while (true)
        {
            int n = recvfrom(sock, buffer, 500, 0,
                             (struct sockaddr *)&from_addr, &addr_len);
            if (n != 500)
                continue; // solo paquetes de 500

            // campos udp
            char seq_str[6] = {}, tot_str[6] = {}, size_str[6] = {};
            std::memcpy(seq_str, buffer, 5);
            std::memcpy(tot_str, buffer + 5, 5);
            std::memcpy(size_str, buffer + 10, 5);

            int seq = std::stoi(seq_str);
            int tot = std::stoi(tot_str);
            int tcp_size = std::stoi(size_str);

            //std::cout << "Recibiendo datagrama " << seq << " de " << tot << '\n';

            // iniciar vc
            if (vc.empty())
            {
                vc.resize(tot, nullptr);
                total_pkts = tot;
            }

            // indice
            int idx = (seq == 0) ? tot - 1 : seq - 1;
            if (idx < 0 || idx >= tot || vc[idx] != nullptr)
                continue; // fuera de rango

            // guardar fragmento
            char *frag = new char[500];
            std::memcpy(frag, buffer, 500);
            vc[idx] = frag;

            // sumar tamaño fragmento tcp
            total_tcp_size += tcp_size;

            // comprobar si estan completos
            bool completo = true;
            for (int i = 0; i<vc.size();i++)
            {
                if (vc[i] == nullptr)
                {
                    //std::cout << "Falta fragmento: " << i+1 << '\n';
                    completo = false;
                    break;
                }
            }

            if (completo){
                break;
            }
                
        }
        //std::cout << "Finalizando ordenamiento" << "\n";
        reensamblar();
    }

    void reensamblar()
    {
        tcp = new char[total_tcp_size];
        tcp_ptr = tcp;
        tcp_len = total_tcp_size;

        int offset = 0;

        for (int i = 0; i < total_pkts; ++i)
        {
            char *frag = vc[i];
            if (!frag)
                continue;

            if (i == total_pkts - 1)
            {
                int last_size = std::stoi(std::string(frag + 10, 5));
                std::memcpy(tcp + offset, frag + 15, last_size);
                offset += last_size;
            }
            else
            {
                std::memcpy(tcp + offset, frag + 15, 485); // 500 - 15
                offset += 485;
            }
        }
    }

    // funcion para que tcp lea
    int leer(char *buffer, int cantidad)
    {
        // error punteros o cantidad negativa
        if (!tcp_ptr || !tcp || cantidad <= 0)
            return 0;

        int bytes_disponibles = tcp + tcp_len - tcp_ptr;
        int bytes_a_copiar = std::min(cantidad, bytes_disponibles);

        // no hay mas bytes
        if (bytes_a_copiar <= 0)
            return 0;

        std::memcpy(buffer, tcp_ptr, bytes_a_copiar);
        tcp_ptr += bytes_a_copiar; // adelantar puntero
        return bytes_a_copiar;
    }
};

class udp_snd
{
private:
    std::vector<char *> fragmentos;
    struct sockaddr_in addr;
    int sock;

public:
    udp_snd(int socket, struct sockaddr_in addr_)
    {
        sock = socket;
        addr = addr_;
    }

    std::string format_number(int num)
    {
        char temp[6];
        std::snprintf(temp, sizeof(temp), "%05d", num);
        return std::string(temp);
    }

    void enviar(const char *msg, int totalSize)
    {
        fragmentos.clear();
        fragmentar(msg, totalSize);

        // enviar con sendto cada fragmento
        for (auto frag : fragmentos)
        {

            // Mostrar la secuencia y el total (primeros 10 bytes)
            char seq[6] = {}, tot[6] = {};
            std::memcpy(seq, frag,      5);
            std::memcpy(tot, frag + 5,  5);
            std::printf("Enviando mensaje %s de %s\n", seq, tot);

            // udp con #
            // std::fwrite(frag, 1, 500, stdout), 
            // std::printf("\n");

            // udp sin #
            int payload = std::stoi(std::string(frag + 10, 5));
            std::fwrite(frag, 1, payload+15, stdout);
            std::printf("\n");

            // tcp
            // int payload = std::stoi(std::string(frag + 10, 5));
            // std::fwrite(frag + 15, 1, payload, stdout);
            // std::printf("\n");
            sleep(0.2);
            sendto(sock, frag, 500, 0,
                   (struct sockaddr *)&addr, sizeof(struct sockaddr));
        }

        for (auto frag : fragmentos)
            delete[] frag;
        fragmentos.clear();
    }

    void fragmentar(const char *msg, int totalSize)
    {
        const int payload_size = 485;
        int total_fragments = (totalSize + payload_size - 1) / payload_size;

        for (int i = 0; i < total_fragments; ++i)
        {
            // cantidad real de datos para este fragmento
            int offset = i * payload_size;
            int frag_tcp_size = std::min(payload_size, totalSize - offset);

            // buffer con #
            char *buffer = new char[500];
            std::memset(buffer, '#', 500); // padding

            // secuencia
            std::string seq_str = (i == total_fragments - 1) ? "00000" : format_number(i + 1);
            std::memcpy(buffer, seq_str.c_str(), 5);

            // total de fragmentos
            std::string tot_str = format_number(total_fragments);
            std::memcpy(buffer + 5, tot_str.c_str(), 5);

            // tamaño tcp
            std::string size_str = format_number(frag_tcp_size);
            std::memcpy(buffer + 10, size_str.c_str(), 5);

            // contenido TCP
            std::memcpy(buffer + 15, msg + offset, frag_tcp_size);

            // guardar fragmento
            fragmentos.push_back(buffer);
        }
    }
};

#endif