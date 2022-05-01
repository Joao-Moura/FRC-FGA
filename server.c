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
envia_msg (int sd, int server_sd, int sala_id, int cliente_id) {
    printf("Enviando mensagem do file descriptor %d na sala %d\n", sd, sala_id);
    // Para cada file descriptor
    for (int j = 0; j <= fdmax; j++)
        // checa se ele esta no cesto do master
        if (FD_ISSET(j, &salas[sala_id].sala_fd))
            // e checa se o valor nao e o descritor de si mesmo
            if (j != sd && j != server_sd) {
                // por fim envia a mensagem para aquele socket descritor
                char mensagem[500] = "[";
                strcat(mensagem, salas[sala_id].clientes[cliente_id].nome);
                strcat(mensagem, "] => ");
                strcat(mensagem, buf);
                send(j, mensagem, 500, 0);
            }
}


void
sair_da_sala (int sd, int sala_id, int cliente_id, int retirar_master) {
    printf("File descriptor %d saindo na sala %d\n", sd, sala_id);
    // Ao sair da sala, deve-se diminuir a quantidade de clientes
    // retirar o descritor da master e da sala.
    salas[sala_id].clientes[cliente_id].ativo = 0;
    salas[sala_id].quantidade--;
    if (retirar_master == 1)
        FD_CLR(sd, &master);
    FD_CLR(sd, &salas[sala_id].sala_fd);

    // E caso a quantidade fique igual 0, deve-se fechar a mesma
    // deixando-a inativa e desalocando o vetor de clientes
    if (salas[sala_id].quantidade == 0) {
        free(salas[sala_id].clientes);
        salas[sala_id].ativo = 0;
    }
}


void
inicia_servidor () {
    // Inicializacao do servidor, zerando todas as salas
    for (int i = 0; i < MAX_SALAS; i++) {
        FD_ZERO(&salas[i].sala_fd);
        salas[i].limite = 0;
        salas[i].quantidade = 0;
        salas[i].ativo = 0;
    }
}


int
cria_sala (int limite) {
    // Para criar uma sala, deve-se encontrar a primeira sala
    // vazia (ativo = 0) setar como ativa e atualizar seu limite
    int sala;
    for (sala = 0; sala < MAX_SALAS; sala++)
        if (salas[sala].ativo == 0)
            break;

    salas[sala].ativo = 1;
    salas[sala].limite = limite;
    salas[sala].clientes = malloc(limite * sizeof(cliente));

    // Apos isso, deve-se instanciar o seu vetor de clientes
    // e desativar todos os presentes. Tambem e necessario 
    // retornar o valor da sala
    for (int i = 0; i < limite; i++)
        salas[sala].clientes[i].ativo = 0;

    printf("Nova sala %d criada\n", sala);
    return sala;
}


void
inserir_na_sala(int sd, int sala_id, char nome[], int tam_nome) {
    printf("File descriptor %d entrando na sala %d\n", sd, sala_id);
    // Para inserir na sala, deve-se aumentar a quantidade, adicionar
    // o descritor no cesto da sala, encontra uma posição na sala
    // que esteja vazia (cliente.ativo = 0) e insere seus atributos
    // socket descriptor, ativo e nome
    FD_SET(sd, &salas[sala_id].sala_fd);
    salas[sala_id].quantidade++;
    for (int i = 0; i < salas[sala_id].limite; i++) {
        if (salas[sala_id].clientes[i].ativo == 0) {
            salas[sala_id].clientes[i].cliente_sd = sd;
            salas[sala_id].clientes[i].ativo = 1;
            strncpy(salas[sala_id].clientes[i].nome, nome, tam_nome);
            break;
        }
    }
}


void
executa_comando (int sd, int sala_id, int cliente_id) {
    buf[strlen(buf) - 2] = '\0';
    printf("Comando \"%s\" acionado na sala %d pelo file descriptor %d\n", buf, sd, sala_id);
    char resp_buf[256];

    // Se o recv retornar 0 ou a mensagem foi de sair
    // retira o socket descriptor do cesto
    if (strncmp(buf+1, "sair", 4) == 0) {
        printf("Desconectando descritor %d\n", sd);
        strcpy(resp_buf, "Cliente Desconectado\n");
        send(sd, resp_buf, strlen(resp_buf), 0);
        close(sd);
        sair_da_sala(sd, sala_id, cliente_id, 1);
    }

    // Caso o comando seja listar, deve-se passar por todos os
    // clientes ativos da sala e lista-los enviando-os com sends
    if (strncmp(buf+1, "listar", 6) == 0) {
        send(sd, "\n===== Clientes Conectados Na Sala =====", 40, 0);
        for (int i = 0; i < salas[sala_id].limite; i++) {
            cliente c = salas[sala_id].clientes[i];
            if (c.ativo == 1 && c.cliente_sd != sd) {
                char nome[] = "\n";
                strcat(nome, c.nome);
                send(sd, nome, strlen(nome), 0);
            }
            else if (c.ativo == 1 && c.cliente_sd == sd) { 
                char nome[] = "\n[";
                strcat(nome, c.nome);
                strcat(nome, "]");
                send(sd, nome, strlen(nome), 0);
            }
        }
        send(sd, "\n\n", 2, 0);
    }

    // Caso o cliente queira trocar de sala, deve-se executar duas
    // rotinas, a de sair de uma sala e a de inserir em um sala
    if (strncmp(buf+1, "trocar_sala", 11) == 0) {
        recv(sd, buf, 256, 0);
        int nova_sala = atoi(buf);
        char nome[256];
        strcpy(nome, salas[sala_id].clientes[cliente_id].nome);
        sair_da_sala(sd, sala_id, cliente_id, 0);
        inserir_na_sala(sd, nova_sala, nome, strlen(nome));
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

                    // Recebe nome do usuario e sala que quer entrar
                    int limite, tam_nome;
                    char nome[256];
                    tam_nome = recv(newfd, nome, 256, 0);
                    tam_nome -= 2;
                    recv(newfd, buf, 256, 0);
                    sala = atoi(buf);
                    
                    // Se a sala for -1, cria uma nova com o limite informado
                    if (sala == -1) {
                        recv(newfd, buf, 256, 0);
                        limite = atoi(buf);
                        sala = cria_sala(limite);
                    }

                    // De qualquer forma insere ele na sala nova ou existente
                    inserir_na_sala(newfd, sala, nome, tam_nome);

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

                    // Encontra a sala que o descritor do socker se encontra
                    int sala_id;
                    for (sala_id = 0; sala_id < MAX_SALAS; sala_id++)
                        if (FD_ISSET(i, &salas[sala_id].sala_fd))
                            break;

                    // Encontra o id do cliente na sala atual do mesmo
                    int cliente_id;
                    for (cliente_id = 0; cliente_id < salas[sala_id].limite; cliente_id++)
                        if (salas[sala_id].clientes[cliente_id].cliente_sd == i)
                            break;

                    // Desconexao forcada
                    if (nbytes == 0) {
                        printf("Desconectando forcadamente o descritor %d\n", i);
                        sair_da_sala(i, sala_id, cliente_id, 1);
                    }

                    // Caso o primeiro caracter da mensagem seja uma / executa comando
                    if (buf[0] == '/')
                        executa_comando(i, sala_id, cliente_id);
                    // Caso não, encaminha a mensagem na sala
                    else
                        envia_msg(i, sd, sala_id, cliente_id);
                }
            }
        }
    }

    return 0;
}
