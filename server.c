#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>


#define STDIN 0


fd_set master, read_fds, write_fds;
struct sockaddr_in myaddr, remoteaddr;
int fdmax, sd, newfd, i, j, nbytes, yes=1, addrlen;
char buf[256];


void
envia_msg () {
    printf("Enviando mensagem do file descriptor %d\n", i);
    // Para cada file descriptor
    for (j = 0; j <= fdmax; j++)
        // checa se ele esta no cesto do master
        if (FD_ISSET(j, &master))
            // e checa se o valor nao e o descritor de si mesmo
            if (j != i && j != sd)
                // por fim envia a mensagem para aquele socket descritor
                send(j, buf, nbytes, 0);
}


void
executa_comando () {
    char resp_buf[256];

    // Se o recv retornar 0 ou a mensagem foi de sair
    // retira o socket descriptor do cesto
    if (strncmp(buf+1, "sair", 4) == 0) {
        printf("Desconectando descritor %d\n", i);
        strcpy(resp_buf, "Cliente Desconectado\n");
        send(i, resp_buf, strlen(resp_buf), 0);
        close(i);
        FD_CLR(i, &master);
    }
}


int
main (int argc, char *argv[]) {
    
    if (argc < 3) {
        printf("Digite IP e porta para o servidor\n");
        exit(1);
    }

    // Limpeza dos sets master e read_fds
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Configuracao de socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
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

    for ( ; ; ) {
        // Informa que o master ira receber descritores de leitura e realiza o select
        read_fds = master;
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        for (i = 0; i <= fdmax; i++) {
            // Testa para ver se o file descriptor esta no cesto
            if (FD_ISSET(i, &read_fds)) {
                // Checa se o file descriptor e o socket
                if (i == sd) {
                    // Se for, aceita a conexao e adiciona o socket descriptor no cesto
                    newfd = accept(sd, (struct sockaddr *)&remoteaddr, &addrlen);
                    FD_SET(newfd, &master);
                    // Se o valor do sd for maior que o atual (mais coisas no cesto)
                    // atualiza esse valor para as proximas iteracoes do loop
                    if (newfd > fdmax)
                        fdmax = newfd;
                }
                else {
                    // Se nao for o descritor do socket, cria um buffer, recebe a mensagem
                    // e a retransmite por todos os sockets conectados
                    memset(&buf, 0, sizeof(buf));
                    nbytes = recv(i, buf, sizeof(buf), 0);

                    // Desconexao forcada
                    if (nbytes == 0) {
                        printf("Desconectando forcadamente o descritor %d\n", i);
                        FD_CLR(i, &master);
                    }

                    if (buf[0] == '/')
                        executa_comando();
                    else
                        envia_msg();
                }
            }
        }
    }

    return 0;
}
