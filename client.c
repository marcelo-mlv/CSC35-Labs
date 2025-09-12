/*
 * Cliente que pode solicitar um arquivo ou o último acesso do servidor.
 * Este programa roda diretamente sobre o nível de transporte (TCP).
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 10002       /* arbitrário, mas cliente e servidor devem combinar */
#define BUF_SIZE 4096           /* tamanho do bloco de transferência */

/* Opcoes do protocolo */
#define OP_GET 0x01
#define OP_LAST_ACCESS 0x02

/* Status do servidor */
#define STATUS_OK 0x00
#define STATUS_ERROR 0x01

void fatal(char *string)
{
    printf("%s\n", string);
    exit(1);
}


int main(int argc, char **argv)
{
    int s, bytes;
    char buf[BUF_SIZE];         /* buffer para arquivo de entrada */
    struct hostent *h;          /* informações sobre servidor */
    struct sockaddr_in channel; /* mantém endereço IP */

    if (argc < 3) {
        printf("Uso: client server-name MyGet [file-name]\n");
        printf("   ou: client server-name MyLastAccess\n");
        exit(1);
    }

    h = gethostbyname(argv[1]);    /* pesquisa endereço IP do host */
    if (!h) fatal("gethostbyname failed");

    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  /* IPv4 + TCP */
    if (s < 0) fatal("socket");

    memset(&channel, 0, sizeof(channel));  /* inicializa estrutura */
    channel.sin_family = AF_INET;          /* IPv4 */
    memcpy(&channel.sin_addr.s_addr, h->h_addr_list[0], h->h_length); /* copia IP do servidor */
    channel.sin_port = htons(SERVER_PORT); /* define porta do servidor */

    if (connect(s, (struct sockaddr *)&channel, sizeof(channel)) < 0) {
        fatal("connect failed");
    }

    /* Processa a requisição MyGet */
    if (strcmp(argv[2], "MyGet") == 0) {
        if (argc != 4) fatal("Uso: client server-name MyGet [file-name]");

        // Envia o opcode MyGet
        char opcode = OP_GET;
        write(s, &opcode, sizeof(opcode));

        // Envia o nome do arquivo com byte 0 no final
        write(s, argv[3], strlen(argv[3]) + 1);

        // Captura a resposta do servidor
        read(s, buf, 1); // Lê o byte de status

        if (buf[0] == STATUS_OK) {
            // Se o status for OK, lê o tamanho do arquivo
            int file_size;
            read(s, &file_size, sizeof(file_size));
            file_size = ntohl(file_size);

            printf("Arquivo recebido (%d bytes):\n", file_size);

            // Lê o arquivo e escreve na saída padrão
            while (file_size > 0) {
                bytes = read(s, buf, BUF_SIZE);
                if (bytes <= 0) break;
                write(1, buf, bytes);
                file_size -= bytes;
            }
        } else if (buf[0] == STATUS_ERROR) {
            // Se o status for erro, lê a mensagem de erro
            printf("Erro do servidor: ");
            while ((bytes = read(s, buf, 1)) > 0 && buf[0] != '\0') {
                write(1, buf, 1);
            }
            printf("\n");
        } else {
            printf("Resposta do servidor desconhecida.\n");
        }
    }
    /* Processa a requisição MyLastAccess */
    else if (strcmp(argv[2], "MyLastAccess") == 0) {
        if (argc != 3) fatal("Uso: client server-name MyLastAccess");

        // Envia o opcode MyLastAccess
        char opcode = OP_LAST_ACCESS;
        write(s, &opcode, sizeof(opcode));

        // Captura a resposta do servidor
        read(s, buf, 1); // Lê o byte de status

        if (buf[0] == STATUS_OK) {
            // Lê a mensagem de data e hora
            while ((bytes = read(s, buf, 1)) > 0 && buf[0] != '\0') {
                write(1, buf, 1);
            }
            printf("\n");
        } else {
            printf("Resposta do servidor desconhecida.\n");
        }
    } else {
        printf("Requisição inválida. Use 'MyGet' ou 'MyLastAccess'.\n");
    }

    close(s);
    return 0;
}