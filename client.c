/* Esta página contém um programa cliente que pode solicitar um arquivo do*/
/* programa servidor na próxima página. O servidor responde enviando o arquivo inteiro.*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 12345       /* arbitrário, mas cliente e servidor devem combinar */
#define BUF_SIZE 4096           /* tamanho do bloco de transferência */

int main(int argc, char **argv)
{
    int c, s, bytes;
    char buf[BUF_SIZE];         /* buffer para arquivo de entrada */
    struct hostent *h;          /* informações sobre servidor */
    struct sockaddr_in channel; /* mantém endereço IP */

    if (argc != 3) fatal("Usage: client server-name file-name");
    h = gethostbyname(argv[1]);    /* pesquisa endereço IP do host */
    if (!h) fatal("gethostbyname failed");

    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  /* IPv4 + TCP */
    if (s < 0) fatal("socket");

    memset(&channel, 0, sizeof(channel));  /* inicializa estrutura */
    channel.sin_family = AF_INET;          /* IPv4 */
    memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length); /* copia IP do servidor */
    channel.sin_port = htons(SERVER_PORT); /* define porta do servidor */

    c = connect(s, (struct sockaddr *)&channel, sizeof(channel)); /* inicia conexão */
    if (c < 0) fatal("connect failed");

    /* Conexão agora estabelecida. Envia nome do arquivo com byte 0 no final. */
    write(s, argv[2], strlen(argv[2]) + 1);

    /* Captura o arquivo e o escreve na saída padrão. */
    while (1) {
        bytes = read(s, buf, BUF_SIZE);   /* lê do soquete */
        if (bytes <= 0) exit(0);          /* verifica final de arquivo */
        write(1, buf, bytes);             /* escreve na saída padrão */
    }
}

fatal(char *string)
{
    printf("%s\n", string);
    exit(1);
}
