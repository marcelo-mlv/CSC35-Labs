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
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define SERVER_PORT 10002       /* arbitrário, mas cliente e servidor devem combinar */
#define BUF_SIZE 4096           /* tamanho do bloco de transferência */
#define QUEUE_SIZE 10           /* tamanho da fila */

/* Opcoes do protocolo */
#define OP_GET 0x01
#define OP_LAST_ACCESS 0x02

/* Status do servidor */
#define STATUS_OK 0x00
#define STATUS_ERROR 0x01

/* Estrutura para manter o estado de cada cliente */
#define MAX_CLIENTS 100
typedef struct {
    char client_ip[INET_ADDRSTRLEN];
    time_t last_access_time;
    int is_active;
} ClientState;

ClientState client_states[MAX_CLIENTS];
pthread_mutex_t states_mutex = PTHREAD_MUTEX_INITIALIZER;

void fatal(char *string)
{
    printf("%s\n", string);
    exit(1);
}

// Encontra ou cria uma entrada para o cliente na tabela de estados
ClientState* get_client_state(const char* ip) {
    pthread_mutex_lock(&states_mutex);

    // Procura por uma entrada existente
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_states[i].is_active && strcmp(client_states[i].client_ip, ip) == 0) {
            pthread_mutex_unlock(&states_mutex);
            return &client_states[i];
        }
    }

    // Se nao encontrar, cria uma nova entrada
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!client_states[i].is_active) {
            strncpy(client_states[i].client_ip, ip, INET_ADDRSTRLEN - 1);
            client_states[i].is_active = 1;
            client_states[i].last_access_time = 0;
            pthread_mutex_unlock(&states_mutex);
            return &client_states[i];
        }
    }

    pthread_mutex_unlock(&states_mutex);
    return NULL; // Tabela cheia
}

// Funcao que o thread ira executar para cada cliente
void* handle_client(void* arg) {
    int sa = *((int*)arg);
    free(arg); // Libera a memoria alocada para o socket

    char buf[BUF_SIZE];
    char opcode;
    int bytes_read;

    // Obtem o endereco do cliente para rastrear o estado
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(sa, (struct sockaddr*)&client_addr, &addr_len);
    char client_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip_str, INET_ADDRSTRLEN);

    printf("Conexao aceita de %s\n", client_ip_str);

    // Le o opcode da requisicao
    if (read(sa, &opcode, 1) <= 0) {
        close(sa);
        return NULL;
    }

    // Processa a requisicao MyGet
    if (opcode == OP_GET) {
        // Le o nome do arquivo
        int fd, total_bytes_read = 0;
        while ((bytes_read = read(sa, buf + total_bytes_read, 1)) > 0 && buf[total_bytes_read] != '\0') {
            total_bytes_read += bytes_read;
        }

        printf("Requisicao MyGet para arquivo: %s\n", buf);

        fd = open(buf, O_RDONLY);
        if (fd < 0) {
            // Arquivo nao encontrado ou erro de permissao
            char status = STATUS_ERROR;
            write(sa, &status, 1);
            const char *error_msg = "Arquivo nao encontrado ou erro de permissao";
            write(sa, error_msg, strlen(error_msg) + 1);
            printf("Erro: Arquivo nao pode ser aberto.\n");
        } else {
            // Arquivo encontrado, envia resposta de sucesso
            char status = STATUS_OK;
            write(sa, &status, 1);

            // Envia o tamanho do arquivo
            struct stat file_stat;
            fstat(fd, &file_stat);
            int file_size = htonl(file_stat.st_size);
            write(sa, &file_size, sizeof(file_size));

            // Envia o conteudo do arquivo
            while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
                write(sa, buf, bytes_read);
            }
            close(fd);

            // Registra o tempo de acesso
            ClientState* state = get_client_state(client_ip_str);
            if (state) {
                state->last_access_time = time(NULL);
            }
            printf("Arquivo enviado com sucesso.\n");
        }
    }
    // Processa a requisicao MyLastAccess
    else if (opcode == OP_LAST_ACCESS) {
        printf("Requisicao MyLastAccess\n");
        char status = STATUS_OK;
        write(sa, &status, 1);

        ClientState* state = get_client_state(client_ip_str);
        if (state && state->last_access_time != 0) {
            char time_str[100];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&state->last_access_time));
            write(sa, time_str, strlen(time_str) + 1);
        } else {
            const char *null_access = "Last Access = Null";
            write(sa, null_access, strlen(null_access) + 1);
        }
        printf("Informacao de ultimo acesso enviada.\n");
    }

    close(sa);
    return NULL;
}


int main(int argc, char *argv[])
{
    int s, b, l, sa, on = 1;
    struct sockaddr_in channel; /* mantém endereço IP */

    /* Inicializa a tabela de estados */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_states[i].is_active = 0;
    }

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

    printf("Servidor iniciado e aguardando conexoes na porta %d...\n", SERVER_PORT);

    /* O soquete agora está preparado e vinculado. Espera conexão e a processa */
    while (1) {
        sa = accept(s, NULL, NULL);   /* bloqueia solicitação de conexão */
        if (sa < 0) fatal("accept failed");

        // Aloca memória para passar o socket para a thread
        int* pclient_sock = malloc(sizeof(int));
        *pclient_sock = sa;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)pclient_sock);
        pthread_detach(thread_id); // Deixa a thread rodar de forma independente
    }
}