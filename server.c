#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define STDIN 0
#define MAX_SALAS 100


fd_set master, read_fds;
struct sockaddr_in myaddr, remoteaddr;
int fdmax, newfd, nbytes, yes=1, addrlen;
char buf[256];

typedef struct {
    int cliente_sd;
    char nome[256];
    int ativo;
} cliente;

typedef struct {
    fd_set sala_fd;
    int limite;
    int quantidade;
    int ativo;
    cliente *clientes;
} sala;

sala salas[MAX_SALAS];


void
envia_msg (int sd, int sala_id, int server_sd) {
    printf("Enviando mensagem do file descriptor %d na sala %d\n", sd, sala_id);
    // Para cada file descriptor
    for (int j = 0; j <= fdmax; j++)
        // checa se ele esta no cesto do master
        if (FD_ISSET(j, &salas[sala_id].sala_fd))
            // e checa se o valor nao e o descritor de si mesmo
            if (j != sd && j != server_sd)
                // por fim envia a mensagem para aquele socket descritor
                send(j, buf, nbytes, 0);
}


void
executa_comando (int sd) {
    char resp_buf[256];

    // Se o recv retornar 0 ou a mensagem foi de sair
    // retira o socket descriptor do cesto
    if (strncmp(buf+1, "sair", 4) == 0) {
        printf("Desconectando descritor %d\n", sd);
        strcpy(resp_buf, "Cliente Desconectado\n");
        send(sd, resp_buf, strlen(resp_buf), 0);
        close(sd);
        FD_CLR(sd, &master);
    }
}


void
inicia_servidor () {
    for (int i = 0; i < MAX_SALAS; i++) {
        FD_ZERO(&salas[i].sala_fd);
        salas[i].limite = 0;
        salas[i].ativo = 0;
    }
}


int
cria_sala (int limite) {
    int sala;
    for (sala = 0; sala < MAX_SALAS; sala++)
        if (salas[sala].ativo == 0)
            break;

    salas[sala].ativo = 1;
    salas[sala].limite = limite;
    salas[sala].clientes = malloc(limite * sizeof(cliente));
    for (int i = 0; i < limite; i++)
        salas[sala].clientes[i].ativo = 0;

    return sala;
}


void
inserir_na_sala(int sd, int sala, char nome[], int tam_nome) {
    salas[sala].quantidade++;
    FD_SET(sd, &salas[sala].sala_fd);
    for (int i = 0; i < salas[sala].limite; i++) {
        if (salas[sala].clientes[i].ativo == 0) {
            salas[sala].clientes[i].ativo = 1;
            salas[sala].clientes[i].cliente_sd = sd;
            strncpy(salas[sala].clientes[i].nome, nome, tam_nome);
        }
    }
}


int
main (int argc, char *argv[]) {
    
    if (argc < 3) {
        printf("Digite IP e porta para o servidor\n");
        exit(1);
    }

    // Limpeza dos sets master e das salas
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    inicia_servidor();

    // Configuracao de socket
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = inet_addr(argv[1]);
    myaddr.sin_port = htons(atoi(argv[2]));
    memset(&(myaddr.sin_zero), 0, 8);

    // Bind e listen nesse socket descriptor
    bind(sd, (struct sockaddr *)&myaddr, sizeof(myaddr));
    listen(sd, 10);

    // Adiciona os file descriptors no set master
    FD_SET(sd, &master);
    FD_SET(STDIN, &master);

    // fdmax -> maior file descriptor (socket descriptor)
    fdmax = sd;
    // addrlen -> tamnaho da struct addrlen
    addrlen = sizeof(remoteaddr);

    int sala;

    for ( ; ; ) {
        // Informa que o master ira receber descritores de leitura e realiza o select
        read_fds = master;
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        for (int i = 0; i <= fdmax; i++) {
            // Testa para ver se o file descriptor esta no cesto
            if (FD_ISSET(i, &read_fds)) {
                // Checa se o file descriptor e o socket
                if (i == sd) {
                    // Se for, aceita a conexao e adiciona o socket descriptor no cesto
                    newfd = accept(sd, (struct sockaddr *)&remoteaddr, &addrlen);
                    FD_SET(newfd, &master);

                    int limite, tam_nome;
                    char nome[256];
                    // send(i, salas, sizeof(sala) * MAX_SALAS, 0);
                    tam_nome = recv(newfd, nome, sizeof(char) * 256, 0);
                    recv(newfd, &sala, sizeof(int), 0);
                    printf("%d\n", sala);
                    
                    if (sala == -1) {
                        recv(newfd, &limite, sizeof(int), 0);
                        sala = cria_sala(limite);
                    }
                    inserir_na_sala(newfd, sala, nome, tam_nome);

                    // Se o valor do sd for maior que o atual (mais coisas no cesto)
                    // atualiza esse valor para as proximas iteracoes do loop
                    if (newfd > fdmax)
                        fdmax = newfd;
                }
                else {
                    // Se nao for o descritor do socket, cria um buffer, recebe a mensagem
                    // e a retransmite por todos os sockets conectados
                    recv(i, &sala, sizeof(int), 0);
                    memset(&buf, 0, sizeof(buf));
                    nbytes = recv(i, buf, sizeof(buf), 0);

                    // Desconexao forcada
                    if (nbytes == 0) {
                        printf("Desconectando forcadamente o descritor %d\n", i);
                        FD_CLR(i, &master);
                    }

                    if (buf[0] == '/')
                        executa_comando(i);
                    else
                        envia_msg(i, sala, sd);
                }
            }
        }
    }

    return 0;
}
