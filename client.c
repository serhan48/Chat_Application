#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define TRUE 1
#define FALSE 1

#define COMMANDLEN 16
#define BUFFSIZE 1024
#define ALIASLEN 16

struct packet {
        char command[COMMANDLEN]; // komut
        char alias[ALIASLEN]; // client aliası
        char buff[BUFFSIZE]; // mesaj
};




void* message_receiver(void* data)
{
	int socket = *(int *)data;

	int n = 0;
	struct packet p;
	memset((void *)&p, 0, sizeof(struct packet));

	while ((n = recv(socket, (void *)&p, sizeof(struct packet), 0)) > 0)
	{
		if (strcmp(p.command, "NOT_FOUND") == 0)
		{
			printf("[SUNUCU]:%s KULLANICISI BULUNAMADI.\n", p.alias);
		}
		else if (strcmp(p.command, "LIST_USERS") == 0)
		{
			printf("CEVRIMICI KULLANICILAR:\n");
			printf("%s", p.buff);
		}
		else if (strcmp(p.command, "SEND") == 0)
		{
			printf("[%s]:%s", p.alias, p.buff);
		}
	}
	if (n == 0)
	{
		printf("SUNUCU ILE BAGLANTI KESILDI.\n");
	}
	else
	{
		printf("SUNUCU ILE BAGLANTI KURULURKEN HATA OLUSTU.\n");
		exit(1);
	}
}





void print_commands()
{
	printf("----------------------------------------------------------------------------\n");
	printf("Programdan cikmak icin LOGOUT yaziniz.\n");
	printf("Cevrimici kullanicileri listemek icin LISTUSERS yaziniz\n");
	printf("Kullaniciya mesaj atmak için K_ADI/MESAJ örneğin:SALIH/merhaba nasilsin?\n");
	printf("----------------------------------------------------------------------------\n");
}



int main(int argc, char* argv[])
{
	int socket_descriptor, portno, n;

	char user_input[1200];

	struct packet p;

	struct sockaddr_in server_addr;

	if(argc != 3)
	{
		printf("Kullanim: %s port_no ip_adresi\n", argv[0]);
		exit(1);
	}

	portno = atoi(argv[1]);

	socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
	
	if (socket_descriptor < 0)
	{
		printf("HATA: soket acilamadi!\n");
		exit(1);
	}


	memset((char *)&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	if (strcmp(argv[2], "localhost") == 0)
		server_addr.sin_addr.s_addr = INADDR_ANY;
	else
		server_addr.sin_addr.s_addr = inet_addr(argv[2]);

	if (connect(socket_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		printf("HATA: sunucuya baglanilamadi!\n");
		exit(1);
	}

	do
	{
		n = 0;
		memset((void *)&p, 0, sizeof(struct packet));
		printf("Lutfen isim seciniz (en fazla 15 karakter):");
		scanf("%s", user_input);
		strcpy(p.command, "alias");
		user_input[ALIASLEN - 1] = '\0';
		strcpy(p.alias, user_input);
		send(socket_descriptor, (void *)&p, sizeof(struct packet), 0);
		n = recv(socket_descriptor, (void *)&p, sizeof(struct packet), 0);
		if (n == 0)
		{
			printf("Server ile baglanti kesildi!\n");
			exit(1);
		}
		else if (n < 0)
		{
			printf("Server ile baglanti kurulurken hata olustu!\n");
			exit(1);
		}

	} while (strcmp(p.command, "ALIAS_IN_USE") == 0);

	printf("Hosgeldiniz:%s\n", p.alias);
	print_commands();


	pthread_t message_receiver_thread;

	if (pthread_create(&message_receiver_thread, (void *)NULL, message_receiver, (void *)&socket_descriptor) < 0)
	{
		printf("HATA: Message receiver threadi olusturalamadi!\n");
		close(socket_descriptor);
		exit(1);
	}


	getchar();

	while (TRUE)
	{
		char str[16];
		fgets(user_input, sizeof(user_input), stdin);

		if (strncmp(user_input, "LOGOUT", 6) == 0)
		{
			strcpy(p.command, "LOGOUT");
			send(socket_descriptor, (void *)&p, sizeof(struct packet), 0);
			exit(0);
		}
		else if (strncmp(user_input, "LISTUSERS", 9) == 0)
		{
			strcpy(p.command, "LIST_USERS");
			send(socket_descriptor, (void *)&p, sizeof(struct packet), 0);
		}
		else
		{
			int i;
			for(i = 0; user_input[i] != '/'; i++)
				str[i] = user_input[i];
			
			str[i] = '\0';
			str[ALIASLEN - 1] = '\0';
			if (i > ALIASLEN - 1)
			{
				print_commands();
			}
			else
			{
				strcpy(p.command, "SEND");
				strcpy(p.alias, str);
				strcpy(p.buff, &user_input[i + 1]);
				send(socket_descriptor, (void *)&p, sizeof(struct packet), 0);
			}
		}
		memset((void *)&p, 0, sizeof(struct packet));
		user_input[0] = '\0';	
	}
	close(socket_descriptor);

	return 0;
}