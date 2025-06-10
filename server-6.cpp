/* Server code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <map>
#include <string>
#include <vector>

using namespace std;

std::map<string, int> mapaSockets;

struct Partida {
    char tablero[9] = {'_', '_', '_', '_', '_', '_', '_', '_', '_'};
    string jugadorO;
    string jugadorX;
    vector<string> espectadores;
    char turno = 'O';
    bool activa = false;
} partida;

string jugadorEnEspera;

/* Forward declarations */
void enviarM(int sock, const string &msg);
void enviarX_aTodos();
void enviarT(const string &nick, char simbolo);
void finalizarPartida(char resultado);
bool ganador(char s);
bool tableroLleno();

void leerSocket(int socketCliente, string nickname) {
    char *buffer = (char *)malloc(10);
    int n, tamano;
    do {
        n = read(socketCliente, buffer, 5);
        if (n <= 0) break;
        buffer[n] = '\0';
        tamano = atoi(buffer);

        n = read(socketCliente, buffer, 1);
        if (n <= 0) break;
        buffer[n] = '\0';

        buffer = (char *)realloc(buffer, tamano + 1);

        // MENSAJE L
        if (buffer[0] == 'L') {
            string mensaje;
            string coma = "";
            for (auto it = mapaSockets.cbegin(); it != mapaSockets.cend(); ++it) {
                if ((*it).first != nickname) {
                    mensaje += coma + (*it).first;
                    coma = ",";
                }
            }
            sprintf(buffer, "%05dl%s\0", (int)mensaje.length() + 1, mensaje.c_str());
            write(mapaSockets[nickname], buffer, mensaje.length() + 6);
        }
        // MENSAJE M
        else if (buffer[0] == 'M') {
            string mensaje, destino;
            n = read(socketCliente, buffer, 5);
            buffer[n] = '\0';
            int tamanoMensaje = atoi(buffer);

            char* msg = (char*)malloc(tamanoMensaje + 1);
            n = read(socketCliente, msg, tamanoMensaje);
            msg[n] = '\0';

            n = read(socketCliente, buffer, 5);
            buffer[n] = '\0';
            int tamanoDestino = atoi(buffer);

            char* dest = (char*)malloc(tamanoDestino + 1);
            n = read(socketCliente, dest, tamanoDestino);
            dest[n] = '\0';

            if (mapaSockets.find(dest) != mapaSockets.end()) {
                char respuesta[1024];
                int tamRemitente = nickname.length();
                int total = 1 + 5 + tamanoMensaje + 5 + tamRemitente;
                
                sprintf(respuesta, "%05dm%05d%s%05d%s",
                       total,
                       tamanoMensaje, msg,
                       tamRemitente, nickname.c_str());
                
                write(mapaSockets[dest], respuesta, total + 5);
            }
            free(msg);
            free(dest);
        }
        // MENSAJE B
        else if (buffer[0] == 'B') {
            n = read(socketCliente, buffer, 5);
            buffer[n] = '\0';
            int tamanoMsg = atoi(buffer);

            char* msg = (char*)malloc(tamanoMsg + 1);
            n = read(socketCliente, msg, tamanoMsg);
            msg[n] = '\0';

            char respuesta[1024];
            int tamRemitente = nickname.length();
            int total = 1 + 5 + tamanoMsg + 5 + tamRemitente;
            
            sprintf(respuesta, "%05db%05d%s%05d%s",
                   total,
                   tamanoMsg, msg,
                   tamRemitente, nickname.c_str());

            for (auto& pair : mapaSockets) {
                if (pair.first != nickname) {
                    write(pair.second, respuesta, total + 5);
                }
            }
            free(msg);
        }
        // MENSAJE F
        else if (buffer[0] == 'F') {
            n = read(socketCliente, buffer, 5);
            buffer[n] = '\0';
            int tamanoDest = atoi(buffer);

            char dest[256];
            n = read(socketCliente, dest, tamanoDest);
            dest[n] = '\0';

            n = read(socketCliente, buffer, 100);
            buffer[100] = '\0';
            long tamanoNombre = strtol(buffer, NULL, 10);

            char* nombreArchivo = new char[tamanoNombre + 1];
            n = read(socketCliente, nombreArchivo, tamanoNombre);
            nombreArchivo[n] = '\0';

            n = read(socketCliente, buffer, 18);
            buffer[18] = '\0';
            long tamanoArchivo = atol(buffer);

            char* datosArchivo = new char[tamanoArchivo];
            long totalLeido = 0;
            while (totalLeido < tamanoArchivo) {
                n = read(socketCliente, datosArchivo + totalLeido, tamanoArchivo - totalLeido);
                if (n <= 0) break;
                totalLeido += n;
            }

            char hashRecibido[6];
            n = read(socketCliente, hashRecibido, 5);
            hashRecibido[5] = '\0';

            if (mapaSockets.find(dest) != mapaSockets.end()) {
                long tamRemitente = nickname.length();
                long dataLen = 1 + 5 + tamRemitente + 100 + tamanoNombre + 18 + tamanoArchivo + 5;
                char* bufferEnvio = new char[5 + dataLen];
                
                sprintf(bufferEnvio, "%05ld", dataLen);
                int pos = 5;
                bufferEnvio[pos++] = 'f';

                sprintf(bufferEnvio + pos, "%05ld", tamRemitente);
                pos += 5;

                memcpy(bufferEnvio + pos, nickname.c_str(), tamRemitente);
                pos += tamRemitente;

                char campoTamNombre[101];
                sprintf(campoTamNombre, "%0100ld", tamanoNombre);
                memcpy(bufferEnvio + pos, campoTamNombre, 100);
                pos += 100;

                memcpy(bufferEnvio + pos, nombreArchivo, tamanoNombre);
                pos += tamanoNombre;

                char tamArchivoStr[19];
                sprintf(tamArchivoStr, "%018ld", tamanoArchivo);
                memcpy(bufferEnvio + pos, tamArchivoStr, 18);
                pos += 18;

                memcpy(bufferEnvio + pos, datosArchivo, tamanoArchivo);
                pos += tamanoArchivo;

                memcpy(bufferEnvio + pos, hashRecibido, 5);
                pos += 5;

                write(mapaSockets[dest], bufferEnvio, 5 + dataLen);
                delete[] bufferEnvio;
            }
            delete[] nombreArchivo;
            delete[] datosArchivo;
        }
        // MENSAJE Q
        else if (buffer[0] == 'Q') {
            printf("\nEl cliente %s ha salido del chat\n", nickname.c_str());
            break;
        }
        // MENSAJE J
        else if (buffer[0] == 'J') {
            if (!partida.activa && jugadorEnEspera.empty()) {
                jugadorEnEspera = nickname;
                enviarM(socketCliente, "wait for player");
            }
            else if (!partida.activa && !jugadorEnEspera.empty()) {
                partida.activa = true;
                partida.jugadorO = jugadorEnEspera;
                partida.jugadorX = nickname;
                jugadorEnEspera = "";

                enviarM(mapaSockets[partida.jugadorO], "inicio");
                enviarM(mapaSockets[partida.jugadorX], "inicio");
                enviarX_aTodos();
                enviarT(partida.jugadorO, 'O');
            }
            else {
                enviarM(socketCliente, "do you want to see");
            }
        }
        // MENSAJE V
        else if (buffer[0] == 'V') {
            if (partida.activa) {
                partida.espectadores.push_back(nickname);
                char len[6];
                sprintf(len, "00010");
                string pkt(len);
                pkt += 'X';
                pkt.append(partida.tablero, 9);
                write(socketCliente, pkt.c_str(), 15);
            }
        }
        // MENSAJE P
        else if (buffer[0] == 'P') {
            char pos, simb;
            read(socketCliente, &pos, 1);
            read(socketCliente, &simb, 1);

            int idx = pos - '1';
            if (idx < 0 || idx > 8 || partida.tablero[idx] != '_') {
                const string err = "00018E600016Posicion ocupada";
                write(socketCliente, err.c_str(), err.size());
                continue;
            }

            partida.tablero[idx] = simb;

            if (ganador(simb)) {
                enviarX_aTodos();
                finalizarPartida(simb);
            }
            else if (tableroLleno()) {
                enviarX_aTodos();
                finalizarPartida('E');
            }
            else {
                enviarX_aTodos();
                partida.turno = (partida.turno == 'O') ? 'X' : 'O';
                const string &prox = (partida.turno == 'O') ? partida.jugadorO : partida.jugadorX;
                enviarT(prox, partida.turno);
            }
        }

    } while (strncmp(buffer, "exit", 4) != 0);
    shutdown(socketCliente, SHUT_RDWR);
    close(socketCliente);
    mapaSockets.erase(nickname);
}


void enviarM(int sock, const string &msg) {
    const string remitente = "servidor";
    int lenMsg = msg.size();
    int lenEm = remitente.size();
    int dataLen = 1 + lenMsg + 5 + lenEm;

    char campoTotal[6], campoLenMsg[6], campoLenEm[6];
    sprintf(campoTotal, "%05d", dataLen);
    sprintf(campoLenMsg, "%05d", lenMsg);
    sprintf(campoLenEm, "%05d", lenEm);

    string pkt(campoTotal, 5);
    pkt += 'm';
    pkt.append(campoLenMsg, 5);
    pkt += msg;
    pkt.append(campoLenEm, 5);
    pkt += remitente;

    write(sock, pkt.c_str(), pkt.size());
}

void enviarX_aTodos() {
    char len[6];
    sprintf(len, "00010");
    string pkt(len);
    pkt += 'X';
    pkt.append(partida.tablero, 9);
    
    if (!partida.jugadorO.empty())
        write(mapaSockets[partida.jugadorO], pkt.c_str(), 15);
    if (!partida.jugadorX.empty())
        write(mapaSockets[partida.jugadorX], pkt.c_str(), 15);
    for (auto &esp : partida.espectadores)
        write(mapaSockets[esp], pkt.c_str(), 15);
}

void enviarT(const string &nick, char simbolo) {
    string pkt = "00002T";
    pkt += simbolo;
    write(mapaSockets[nick], pkt.c_str(), 7);
}

bool linea(int a, int b, int c, char s) {
    return partida.tablero[a] == s && partida.tablero[b] == s && partida.tablero[c] == s;
}

bool ganador(char s) {
    return linea(0, 1, 2, s) || linea(3, 4, 5, s) || linea(6, 7, 8, s) ||
           linea(0, 3, 6, s) || linea(1, 4, 7, s) || linea(2, 5, 8, s) ||
           linea(0, 4, 8, s) || linea(2, 4, 6, s);
}

bool tableroLleno() {
    for (char c : partida.tablero)
        if (c == '_') return false;
    return true;
}

void finalizarPartida(char resultado) {
    string msgO = "00002O";
    msgO += (resultado == 'O') ? 'W' : (resultado == 'X') ? 'L' : 'E';
    
    string msgX = "00002O";
    msgX += (resultado == 'X') ? 'W' : (resultado == 'O') ? 'L' : 'E';
    
    write(mapaSockets[partida.jugadorO], msgO.c_str(), 7);
    write(mapaSockets[partida.jugadorX], msgX.c_str(), 7);
    
    string msgE = "00002OE";
    for (auto &esp : partida.espectadores)
        write(mapaSockets[esp], msgE.c_str(), 7);
    
    partida = Partida();
}

int main(void) {
    struct sockaddr_in direccion;
    int socketServidor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    char *buffer = (char *)malloc(10);
    int n;

    if (-1 == socketServidor) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion, 0, sizeof(struct sockaddr_in));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(45000);
    direccion.sin_addr.s_addr = INADDR_ANY;

    if (-1 == bind(socketServidor, (const struct sockaddr *)&direccion, sizeof(struct sockaddr_in))) {
        perror("Error en bind");
        close(socketServidor);
        exit(EXIT_FAILURE);
    }

    if (-1 == listen(socketServidor, 10)) {
        perror("Error en listen");
        close(socketServidor);
        exit(EXIT_FAILURE);
    }

    for (;;) {
        printf("%s\n", "Esperando cliente...");
        int socketCliente = accept(socketServidor, NULL, NULL);

        if (0 > socketCliente) {
            perror("Error en accept");
            close(socketServidor);
            exit(EXIT_FAILURE);
        }

        n = read(socketCliente, buffer, 5);
        buffer[n] = '\0';
        int tamano = atoi(buffer);
        buffer = (char *)realloc(buffer, tamano + 1);

        n = read(socketCliente, buffer, 1);
        buffer[n] = '\0';
        if (buffer[0] == 'N') {
            n = read(socketCliente, buffer, tamano - 1);
            buffer[n] = '\0';
            mapaSockets[buffer] = socketCliente;
            printf("\nSe ha conectado el Cliente: %s\n", buffer);
            std::thread(leerSocket, socketCliente, std::string(buffer)).detach();
        }
        else {
            shutdown(socketCliente, SHUT_RDWR);
            close(socketCliente);
        }
    }
    free(buffer);
    close(socketServidor);
    return 0;
}