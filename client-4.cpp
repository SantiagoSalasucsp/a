/* Client code in C */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream> // std::cout
#include <thread>   // std::thread, std::this_thread::sleep_for
#include <chrono>
#include <string>
#include <atomic>
#include <limits>
#include <fstream> // Para manejo de archivos
#include <vector>

std::string nickname;
std::atomic<bool> envLista(false);
std::atomic<bool> enPartida(false);
std::atomic<bool> esEspectador(false);
std::atomic<char> simbolo{'\0'};
int socketGlobal;

void pedirYEnviarP(int sock, char simbolo);

// calcular hash (suma de bytes mod 100000)
int calcularHash(const char *datos, long tamano)
{
  int hash = 0;
  for (long i = 0; i < tamano; i++)
  {
    hash = (hash + (unsigned char)datos[i]) % 100000;
  }
  return hash;
}

void leerSocket(int socketCliente)
{
  char *buffer = (char *)malloc(10);

  int n, tamano;
  do
  {
    n = read(socketCliente, buffer, 5);
    if (n <= 0)
      break;
    buffer[n] = '\0';
    tamano = atoi(buffer);

    buffer = (char *)realloc(buffer, tamano + 1);

    n = read(socketCliente, buffer, 1);
    if (n <= 0)
      break;
    buffer[n] = '\0';

    // Mensaje l
    if (buffer[0] == 'l')
    {
      n = read(socketCliente, buffer, tamano - 1);
      buffer[n] = '\0';
      printf("\nUsuarios conectados: %s\n", buffer);
      envLista = true;
    }
    // Mensaje m
    else if (buffer[0] == 'm')
    {
      std::string usuarioOrigen;
      std::string mensaje;
      n = read(socketCliente, buffer, 5);
      buffer[n] = '\0';
      int tamanoMensaje = atoi(buffer);

      n = read(socketCliente, buffer, tamanoMensaje);
      buffer[n] = '\0';
      mensaje = buffer;

      n = read(socketCliente, buffer, 5);
      buffer[n] = '\0';
      int tamanoUsuario = atoi(buffer);

      n = read(socketCliente, buffer, tamanoUsuario);
      buffer[n] = '\0';
      usuarioOrigen = buffer;

      printf("\n\n%s: %s\n", usuarioOrigen.c_str(), mensaje.c_str());

      if (usuarioOrigen == "Servidor" || usuarioOrigen == "servidor" || usuarioOrigen == "server" || usuarioOrigen == "Server")
      {
        if (mensaje == "do you want to see?" || mensaje == "Do you want to see?" || mensaje == "Do you want to see" || mensaje == "Desea ver?" || mensaje == "do you want to see")
        {
          int opcion;
          std::cout << "\n¿Deseas ver la partida?\n 1. Sí\n 2. No\n> ";
          std::cin >> opcion;

          if (opcion == 1)
          { // el usuario acepta ser espectador
            esEspectador = true;
            write(socketCliente, "00001V", 6); // mensaje V al servidor
          }
          else
          { // rechaza
            enPartida = false;
          }
          continue;
        }
      }
    }
    // Mensaje b
    else if (buffer[0] == 'b')
    {
      std::string usuarioOrigen;
      std::string mensaje;
      n = read(socketCliente, buffer, 5);
      buffer[n] = '\0';
      int tamanoMensaje = atoi(buffer);

      n = read(socketCliente, buffer, tamanoMensaje);
      buffer[n] = '\0';
      mensaje = buffer;

      n = read(socketCliente, buffer, 5);
      buffer[n] = '\0';
      int tamanoUsuario = atoi(buffer);

      n = read(socketCliente, buffer, tamanoUsuario);
      buffer[n] = '\0';
      usuarioOrigen = buffer;

      printf("\n\n[broadcast] %s: %s\n", usuarioOrigen.c_str(), mensaje.c_str());
    }
    // Mensaje f
    else if (buffer[0] == 'f')
    {
      std::string emisorArchivo;
      std::string nombreArchivo;
      // char contenidoArchivo[500000]; // buffer contenido del archivo

      char hashRecibido[6];

      // 5B tamaño emisor
      n = read(socketCliente, buffer, 5);
      buffer[n] = '\0';
      int tamanoEmisor = atoi(buffer);

      // nickname emisor
      n = read(socketCliente, buffer, tamanoEmisor);
      buffer[n] = '\0';
      emisorArchivo = buffer;

      // 100B tamaño nombre del archivo
      n = read(socketCliente, buffer, 100);
      buffer[100] = '\0';
      long tamanoNombreArchivo = strtol(buffer, NULL, 10);

      // nombre del archivo
      char *nombreArchivoC = new char[tamanoNombreArchivo + 1];
      n = read(socketCliente, nombreArchivoC, tamanoNombreArchivo);
      nombreArchivoC[n] = '\0';
      nombreArchivo = std::string(nombreArchivoC, tamanoNombreArchivo);
      delete[] nombreArchivoC;

      // 18B tamaño del archivo
      n = read(socketCliente, buffer, 18);
      buffer[18] = '\0';
      long tamanoArchivo = atol(buffer);
      char *contenidoArchivo = new char[tamanoArchivo + 1];
      // contenido del archivo
      long totalLeido = 0;
      while (totalLeido < tamanoArchivo)
      {
        n = read(socketCliente, contenidoArchivo + totalLeido, tamanoArchivo - totalLeido);
        if (n <= 0)
          break;
        totalLeido += n;
      }

      // 5B hash
      n = read(socketCliente, hashRecibido, 5);
      hashRecibido[5] = '\0';

      // calcular hash
      int hashLocal = calcularHash(contenidoArchivo, tamanoArchivo);
      char hashCalculado[6];
      sprintf(hashCalculado, "%05d", hashLocal);

      // compara hash
      if (strcmp(hashCalculado, hashRecibido) == 0)
        printf("\n[archivo] %s: %s (Hash OK)\n", emisorArchivo.c_str(), nombreArchivo.c_str());
      else
        printf("\n[archivo] %s: %s (Hash INCORRECTO: calculado %s, recibido %s)\n",
               emisorArchivo.c_str(), nombreArchivo.c_str(), hashCalculado, hashRecibido);

      // guardar archivo
      if (strcmp(hashCalculado, hashRecibido) == 0)
      {
        std::ofstream archivoSalida(nombreArchivo, std::ios::binary);
        if (!archivoSalida.is_open())
        {
          std::cerr << "No se pudo crear el archivo " << nombreArchivo << " en disco.\n";
        }
        else
        {
          archivoSalida.write(contenidoArchivo, tamanoArchivo);
          archivoSalida.close();
          std::cout << "Archivo " << nombreArchivo << " guardado en la carpeta actual.\n";
        }
      }
    }

    // MENSAJE X
    else if (buffer[0] == 'x' || buffer[0] == 'X')
    {
      n = read(socketCliente, buffer, 9);
      buffer[9] = '\0';
      printf("\nTABLERO\n");
      for (int i = 0; i < 9; i++)
      {
        printf("%c", buffer[i]);
        if (i % 3 == 2)
          printf("\n");
        else
          printf("|");
      }
    }

    // MENSAJE T
    else if (buffer[0] == 't' || buffer[0] == 'T')
    {
      read(socketCliente, &simbolo, 1); // guarda simbolo
      pedirYEnviarP(socketCliente, simbolo);
    }

    // MENSAJE E
    else if (buffer[0] == 'e' || buffer[0] == 'E')
    {
      char codigo;
      read(socketCliente, &codigo, 1);
      char len[6];
      read(socketCliente, len, 5);
      int tam = atoi(len);
      buffer = (char *)realloc(buffer, tam + 1);
      n = read(socketCliente, buffer, tam);
      buffer[n] = '\0';

      std::cout << "\nERROR " << codigo << ": " << buffer << "\n";

      if (codigo == '6')
      { // posición ocupada
        // vuelve a pedir p
        pedirYEnviarP(socketCliente, simbolo);
      }
    }

    // MENSAJE O
    else if (buffer[0] == 'o' || buffer[0] == 'O')
    {
      char res;
      read(socketCliente, &res, 1);
      if (!esEspectador)
      {
        if (res == 'W')
          puts("\n*** ¡Ganaste! ***");
        else if (res == 'L')
          puts("\n*** Perdiste ***");
        else
          puts("\n*** Empate ***");
      }
      else
      {
        puts("\n*** La partida termino ***");
      }
      enPartida = false;
      esEspectador = false;
    }

  } while (strncmp(buffer, "exit", 4) != 0);
}

void pedirYEnviarP(int sock, char simbolo)
{
    // Permitir al usuario elegir entre hacer una acción o jugar
    char opcion;
    std::cout << "\nTu turno (" << simbolo << ").\n(L/M/B/F/J) o (1-9): ";
    std::cin >> opcion;
    
    // Si es una letra (acción)
    if (opcion == 'L' || opcion == 'l') {
        // Mensaje L - Lista de usuarios
        char buffer[7];
        strcpy(buffer, "00001L");
        buffer[6] = '\0';
        write(sock, buffer, 6);
        while (!envLista)
            ;
        envLista = false;
        
        // Después de la acción, volvemos a pedir la posición
        pedirYEnviarP(sock, simbolo);
        return;
    } 
    else if (opcion == 'M' || opcion == 'm') {
        // Mensaje M - Enviar mensaje
        std::string destino;
        std::string mensaje;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Destinatario: ";
        std::getline(std::cin, destino);
        std::cout << "Mensaje: ";
        std::getline(std::cin, mensaje);
        
        char* buffer = (char *)malloc((int)destino.length() + (int)mensaje.length() + 16 + 1);
        sprintf(buffer, "%05dM%05d%s%05d%s", (int)(destino.length() + mensaje.length() + 1 + 5 + 5),
                (int)mensaje.length(), mensaje.c_str(), (int)destino.length(), destino.c_str());
        write(sock, buffer, mensaje.length() + destino.length() + 16); // 16 = 5+5+5+1
        free(buffer);
        
        // Después de la acción, volvemos a pedir la posición
        pedirYEnviarP(sock, simbolo);
        return;
    }
    else if (opcion == 'B' || opcion == 'b') {
        // Mensaje B - Broadcast
        std::string mensaje;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Mensaje B: ";
        std::getline(std::cin, mensaje);
        
        char* buffer = (char *)malloc((int)mensaje.length() + 11 + 1);
        sprintf(buffer, "%05dB%05d%s", (int)(mensaje.length() + 1) + 5, (int)mensaje.length(), mensaje.c_str());
        write(sock, buffer, mensaje.length() + 11); // 11 = 5+1+5
        free(buffer);
        
        // Después de la acción, volvemos a pedir la posición
        pedirYEnviarP(sock, simbolo);
        return;
    }
    else if (opcion == 'F' || opcion == 'f') {
        // Mensaje F - Enviar archivo
        std::string destino;
        std::string rutaArchivo;
        std::string nombreArchivo;

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Nombre Destinatario: ";
        std::getline(std::cin, destino);
        std::cout << "Ruta del archivo a enviar: ";
        std::getline(std::cin, rutaArchivo);

        // nombre del archivo sin ruta
        size_t posBarra = rutaArchivo.find_last_of("/\\");
        if (posBarra != std::string::npos)
        {
            nombreArchivo = rutaArchivo.substr(posBarra + 1);
        }
        else
        {
            nombreArchivo = rutaArchivo;
        }

        // abre el archivo en modo binario y se lee
        std::ifstream archivoEntrada(rutaArchivo, std::ios::in | std::ios::binary);
        if (!archivoEntrada.is_open())
        {
            std::cerr << "No se pudo abrir el archivo " << rutaArchivo << "\n";
            pedirYEnviarP(sock, simbolo);
            return;
        }
        std::vector<char> datosArchivo((std::istreambuf_iterator<char>(archivoEntrada)),
                                     std::istreambuf_iterator<char>());
        long tamanoArchivo = datosArchivo.size();
        archivoEntrada.close();

        // hash del archivo
        int valorHash = calcularHash(datosArchivo.data(), tamanoArchivo);
        char hashStr[6];
        sprintf(hashStr, "%05d", valorHash);

        // campo tamnombre archivo
        long tamanoNomArchivo = nombreArchivo.size();
        char campoTamanoNombre[101];
        sprintf(campoTamanoNombre, "%0100ld", tamanoNomArchivo);

        // tamaños
        long tamanoDestino = destino.size();
        long dataLen = 1 + 5 + tamanoDestino + 100 + tamanoNomArchivo + 18 + tamanoArchivo + 5;
        long tamanoMensajeTotal = 5 + dataLen;
        char *bufferEnvio = new char[tamanoMensajeTotal];
        memset(bufferEnvio, 0, tamanoMensajeTotal);

        // 5B dataLen
        sprintf(bufferEnvio, "%05ld", dataLen);
        int posEnvio = 5;

        // 1B tipo F
        bufferEnvio[posEnvio++] = 'F';

        // 5B tamaño nickaname destinatario
        {
            char tmp[10];
            sprintf(tmp, "%05ld", tamanoDestino);
            memcpy(bufferEnvio + posEnvio, tmp, 5);
            posEnvio += 5;
        }

        // nickname destinatario
        memcpy(bufferEnvio + posEnvio, destino.c_str(), tamanoDestino);
        posEnvio += tamanoDestino;

        // 100B longitud rchivo
        memcpy(bufferEnvio + posEnvio, campoTamanoNombre, 100);
        posEnvio += 100;

        // nombre archivo
        memcpy(bufferEnvio + posEnvio, nombreArchivo.c_str(), tamanoNomArchivo);
        posEnvio += tamanoNomArchivo;

        // 18B tamaño archivo
        {
            char tmp[19];
            sprintf(tmp, "%018ld", tamanoArchivo);
            memcpy(bufferEnvio + posEnvio, tmp, 18);
            posEnvio += 18;
        }

        // Contenido archivo
        memcpy(bufferEnvio + posEnvio, datosArchivo.data(), tamanoArchivo);
        posEnvio += tamanoArchivo;

        // 5B hash
        memcpy(bufferEnvio + posEnvio, hashStr, 5);
        posEnvio += 5;

        // envio
        write(sock, bufferEnvio, tamanoMensajeTotal);
        std::cout << "Archivo enviado.\n";
        delete[] bufferEnvio;
        
        // Después de la acción, volvemos a pedir la posición
        pedirYEnviarP(sock, simbolo);
        return;
    }
    else if (opcion == 'Q' || opcion == 'q') {
        // Mensaje Q - Salir
        char buffer[7];
        strcpy(buffer, "00001Q");
        buffer[6] = '\0';
        write(sock, buffer, 6);
        enPartida = false;
        return;
    }
    else if (opcion >= '1' && opcion <= '9') {
        // Es un número, procesamos como posición de juego
        int pos = opcion - '0';
        /* 5 B tamaño (3) + 'P' + pos + símbolo  = 8 bytes totales */
        char pkt[9];
        sprintf(pkt, "00003P%c%c", char('0' + pos), simbolo);
        write(sock, pkt, 8);
    }
    else {
        // Opción inválida, pedimos de nuevo
        std::cout << "invalidoo\n";
        pedirYEnviarP(sock, simbolo);
    }
}


int main(void)
{
  struct sockaddr_in direccion;
  int resultado;
  int socketCliente = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  int n;

  if (-1 == socketCliente)
  {
    perror("No se pudo crear el socket");
    exit(EXIT_FAILURE);
  }

  memset(&direccion, 0, sizeof(struct sockaddr_in));

  direccion.sin_family = AF_INET;
  direccion.sin_port = htons(45000);
  resultado = inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

  if (0 > resultado)
  {
    perror("error: primer parámetro no es una familia de direcciones válida");
    close(socketCliente);
    exit(EXIT_FAILURE);
  }
  else if (0 == resultado)
  {
    perror("la cadena (segundo parámetro) no contiene una dirección IP válida");
    close(socketCliente);
    exit(EXIT_FAILURE);
  }

  if (-1 == connect(socketCliente, (const struct sockaddr *)&direccion, sizeof(struct sockaddr_in)))
  {
    perror("falló el connect");
    close(socketCliente);
    exit(EXIT_FAILURE);
  }

  socketGlobal = socketCliente;
  // Mensaje N para registro

  std::cout << "Ingresa tu nombre: ";
  std::getline(std::cin, nickname);
  char *buffer = (char *)malloc((int)nickname.length() + 5 + 1 + 1);
  sprintf(buffer, "%05dN%s\0", (int)nickname.length() + 1, nickname.c_str());
  write(socketCliente, buffer, nickname.length() + 1 + 5);
  std::thread(leerSocket, socketCliente).detach();

  bool salir = false;
  while (!salir)
  {
    // Menú con letras en lugar de números
    char opcion;
    std::cout << "\nmenu" << std::endl;
    std::cout << "L. Ver lista de usuarios conectados" << std::endl;
    std::cout << "M. Mandar mensaje" << std::endl;
    std::cout << "B. Mandar mensaje broadcast" << std::endl;
    std::cout << "F. Mandar archivo" << std::endl;
    if (!enPartida)
    {
      std::cout << "J. Unirse a tic tac toe"<< std::endl;
    }
    std::cout << "Q. Salir" << std::endl;
    std::cout << "Seleccione una opción: ";
    std::cin >> opcion;

    // Mensaje L
    if (opcion == 'L' || opcion == 'l')
    {
      buffer = (char *)realloc(buffer, 7 + 1);
      strcpy(buffer, "00001L");
      buffer[6] = '\0';
      write(socketCliente, buffer, 6);
      while (!envLista)
        ;
      envLista = false;
    }
    // Mensaje M
    else if (opcion == 'M' || opcion == 'm')
    {
      std::string destino;
      std::string mensaje;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "\nNombre Destinatario: ";
      std::getline(std::cin, destino);
      std::cout << "\nMensaje: ";
      std::getline(std::cin, mensaje);
      buffer = (char *)realloc(buffer, (int)destino.length() + (int)mensaje.length() + 16 + 1);

      sprintf(buffer, "%05dM%05d%s%05d%s", (int)(destino.length() + mensaje.length() + 1 + 5 + 5),
              (int)mensaje.length(), mensaje.c_str(), (int)destino.length(), destino.c_str());
      write(socketCliente, buffer, mensaje.length() + destino.length() + 16); // 16 = 5+5+5+1
    }
    // Mensaje Q
    else if (opcion == 'Q' || opcion == 'q')
    {
      buffer = (char *)realloc(buffer, 7 + 1);
      strcpy(buffer, "00001Q");
      buffer[6] = '\0';
      write(socketCliente, buffer, 6);
      salir = true;
    }
    // Mensaje B
    else if (opcion == 'B' || opcion == 'b')
    {
      std::string mensaje;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "\nMensaje: ";
      std::getline(std::cin, mensaje);
      buffer = (char *)realloc(buffer, (int)mensaje.length() + 11 + 1);
      sprintf(buffer, "%05dB%05d%s", (int)(mensaje.length() + 1) + 5, (int)mensaje.length(), mensaje.c_str());
      // printf("Mensaje B a enviar: %s\n", buffer);
      write(socketCliente, buffer, mensaje.length() + 11); // 11 = 5+1+5
    }
    // Mensaje F
    else if (opcion == 'F' || opcion == 'f')
    {
      std::string destino;
      std::string rutaArchivo;
      std::string nombreArchivo;

      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "\nNombre Destinatario: ";
      std::getline(std::cin, destino);
      std::cout << "Ruta del archivo a enviar: ";
      std::getline(std::cin, rutaArchivo);

      // nombre del archivo sin ruta
      size_t posBarra = rutaArchivo.find_last_of("/\\");
      if (posBarra != std::string::npos)
      {
        nombreArchivo = rutaArchivo.substr(posBarra + 1);
      }
      else
      {
        nombreArchivo = rutaArchivo;
      }

      // abre el archivo en modo binario y se lee
      std::ifstream archivoEntrada(rutaArchivo, std::ios::in | std::ios::binary);
      if (!archivoEntrada.is_open())
      {
        std::cerr << "No se pudo abrir el archivo " << rutaArchivo << "\n";
        continue;
      }
      std::vector<char> datosArchivo((std::istreambuf_iterator<char>(archivoEntrada)),
                                     std::istreambuf_iterator<char>());
      long tamanoArchivo = datosArchivo.size();
      archivoEntrada.close();

      // hash del archivo
      int valorHash = calcularHash(datosArchivo.data(), tamanoArchivo);
      char hashStr[6];
      sprintf(hashStr, "%05d", valorHash);

      // campo tamnombre archivo
      long tamanoNomArchivo = nombreArchivo.size();
      char campoTamanoNombre[101];
      sprintf(campoTamanoNombre, "%0100ld", tamanoNomArchivo);

      // tamaños
      long tamanoDestino = destino.size();
      long dataLen = 1 + 5 + tamanoDestino + 100 + tamanoNomArchivo + 18 + tamanoArchivo + 5;
      long tamanoMensajeTotal = 5 + dataLen;
      char *bufferEnvio = new char[tamanoMensajeTotal];
      memset(bufferEnvio, 0, tamanoMensajeTotal);

      // 5B dataLen
      sprintf(bufferEnvio, "%05ld", dataLen);
      int posEnvio = 5;

      // 1B tipo F
      bufferEnvio[posEnvio++] = 'F';

      // 5B tamaño nickaname destinatario
      {
        char tmp[10];
        sprintf(tmp, "%05ld", tamanoDestino);
        memcpy(bufferEnvio + posEnvio, tmp, 5);
        posEnvio += 5;
      }

      // nickname destinatario
      memcpy(bufferEnvio + posEnvio, destino.c_str(), tamanoDestino);
      posEnvio += tamanoDestino;

      // 100B longitud rchivo
      memcpy(bufferEnvio + posEnvio, campoTamanoNombre, 100);
      posEnvio += 100;

      // nombre archivo
      memcpy(bufferEnvio + posEnvio, nombreArchivo.c_str(), tamanoNomArchivo);
      posEnvio += tamanoNomArchivo;

      // 18B tamaño archivo
      {
        char tmp[19];
        sprintf(tmp, "%018ld", tamanoArchivo);
        memcpy(bufferEnvio + posEnvio, tmp, 18);
        posEnvio += 18;
      }

      // Contenido archivo
      memcpy(bufferEnvio + posEnvio, datosArchivo.data(), tamanoArchivo);
      posEnvio += tamanoArchivo;

      // 5B hash
      memcpy(bufferEnvio + posEnvio, hashStr, 5);
      posEnvio += 5;

      // envio
      write(socketCliente, bufferEnvio, tamanoMensajeTotal);
      std::cout << "Archivo enviado.\n";
      delete[] bufferEnvio;
    }
    // Mensaje J
    else if (opcion == 'J' || opcion == 'j')
    {
      buffer = (char *)realloc(buffer, 7 + 1);
      strcpy(buffer, "00001J");
      buffer[6] = '\0';
      write(socketCliente, buffer, 6);
      enPartida = true;
      // El cliente espera mientras está en partida
      while (enPartida)
        ;
    }
    else
    {
      std::cout << "Opción no válida." << std::endl;
    }
  }

  shutdown(socketCliente, SHUT_RDWR);
  close(socketCliente);
  return 0;
}