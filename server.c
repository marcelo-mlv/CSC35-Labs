/* Este é o código do servidor */

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 10002       /* arbitrário, mas cliente e servidor devem combinar */
#define BUF_SIZE 4096           /* tamanho do bloco de transferência */
#define QUEUE_SIZE 10           /* tamanho da fila */

void fatal(char *string)
{
    printf("%s\n", string);
    exit(1);
}


int main(int argc, char *argv[])
{
    int s, b, l, fd, sa, bytes, on = 1;
    char buf[BUF_SIZE];         /* buffer para arquivo de saída */
    struct sockaddr_in channel; /* mantém endereço IP */

    /* Monta estrutura de endereços para vincular ao soquete */
    memset(&channel, 0, sizeof(channel));  /* canal zero */
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);

    /* Abertura passiva. Espera a conexão. */
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  /* cria soquete */
    if (s < 0) fatal("socket failed");

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)); /* opções */
    b = bind(s, (struct sockaddr *)&channel, sizeof(channel)); /* associa socket */
    if (b < 0) fatal("bind failed");

    l = listen(s, QUEUE_SIZE);  /* pronto para conexões */
    if (l < 0) fatal("listen failed");

    /* O soquete agora está preparado e vinculado. Espera conexão e a processa */
    while (1) {
        sa = accept(s, 0, 0);   /* bloqueia solicitação de conexão */
        if (sa < 0) fatal("accept failed");

        read(sa, buf, BUF_SIZE); /* lê nome do arquivo do soquete */

        /* Captura e retorna o arquivo */
        fd = open(buf, O_RDONLY);
        if (fd < 0) fatal("open failed");

        while (1) {
            bytes = read(fd, buf, BUF_SIZE); /* lê do arquivo */
            if (bytes <= 0) break;           /* verifica final do arquivo */
            write(sa, buf, bytes);           /* grava bytes no soquete */
        }
        close(fd); /* fecha arquivo */
        close(sa); /* fecha conexão */
    }
}