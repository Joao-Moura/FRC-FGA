# Chat usando Select e Sockets
## Iniciando o servidor

```
# Primeiro compile o código
$ gcc -o server server.c

# Segundo inicie o server
$ ./server [ip do server] [porta do server]
```

## Conectando clientes (telnet/netcat)

```
# Conecte-se ao server
$ telnet [ip do server] [porta do server]

# Insira seu nome
$ Joao

# Insira a sala desejada (-1 caso queira criar uma nova)
$ -1

# Se estiver criando insira o limite da sala)
$ 10

# Converse à vontade com os integrantes da mesma sala
$ Ola pessoal
```

## API de comandos
O server possui implementado alguns comandos, sendo eles:

```
# Listagem dos integrantes
$ /listar

# Ex. de resposta
$ ===== Clientes Conectados Na Sala =====
$ [Joao]
$ Pedro
$ Brenno

# Sair de uma sala
$ /sair

# Trocar de sala (logo depois forneça o número da sala)
$ /trocar_sala
$ 4
```
