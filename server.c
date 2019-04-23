#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 20
#define TRUE 1
#define FALSE 0
#define MAX_BUFFER 1024
#define COMMANDLEN 16
#define BUFFSIZE 1024
#define ALIASLEN 16


// MAX_CLIENTS sabitiyle tanımlanan sayıda client için MAX_CLIENTS adet thread için 
// pthread_t yapısı iceren ve karşılık gelen threadlerin kullanımda
// olup olmadığını gösteren MAX_CLIENTS tane bayrak ve erişim için mutex değişkeni ve 
// dolu olup olmadığı ile ilgili condition değişkeni içeren thread_pool_t yapısı

typedef struct {
	pthread_t threads[MAX_CLIENTS];
	int is_allocated[MAX_CLIENTS];
	int sockets[MAX_CLIENTS];
	int thread_count;
	char* alias[MAX_CLIENTS];
	pthread_mutex_t mutex;
	pthread_cond_t not_full_cond;
} thread_pool_t;

struct packet {
        char command[COMMANDLEN]; // komut
        char alias[ALIASLEN]; // client's alias
        char buff[BUFFSIZE]; // payload
};

typedef struct {
	int thread_no;
	int socket_no;
} client_handler_data;



// threade atanan fonksiyona gonderilen thread havuzundaki hangi threadin tahsis edildiğini belirten thread_no
// ve threadin ele alacağı clientın soket numarasını içeren socket_no dan oluşan client_handler_data yapısı

thread_pool_t thread_pool;

// thread_pool_t yapısını başlatan (en başta threadlerinin hiçbirinin kullanımda olmadığını belirten
// ve mutex ve condition değişkenini başlatan fonksiyon

void init_thread_pool()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		thread_pool.is_allocated[i] = FALSE;
		thread_pool.sockets[i] = -1;
		thread_pool.alias[i] = NULL;
	}

	pthread_mutex_init(&(thread_pool.mutex), NULL);
	pthread_cond_init(&(thread_pool.not_full_cond), NULL);
	thread_pool.thread_count = 0;
}

void delete_from_thread_pool(int thread_no)
{
	pthread_mutex_lock(&thread_pool.mutex);
	if (thread_pool.is_allocated[thread_no] == TRUE)
	{
		free(thread_pool.alias[thread_no]);

		if (thread_pool.thread_count == MAX_CLIENTS)
		{
			thread_pool.thread_count--;
			thread_pool.is_allocated[thread_no] = FALSE;
			pthread_cond_signal(&thread_pool.not_full_cond);
		}
		else
		{
			thread_pool.thread_count--;
			thread_pool.is_allocated[thread_no] = FALSE;
		}
	}
	pthread_mutex_unlock(&thread_pool.mutex);
}



void* client_handler(void* data)
{
	struct packet p;
	char user_alias[ALIASLEN];

	memset(&p, 0, sizeof(struct packet));

	client_handler_data* d = (client_handler_data *)data;
	int n;

	while ((n = recv(d->socket_no, (void *)&p, sizeof(struct packet), 0)) > 0)
	{
		if (strcmp(p.command, "alias") == 0)
		{
			pthread_mutex_lock(&thread_pool.mutex);
			int i;
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				if (thread_pool.is_allocated[i] && (i != d->thread_no) && (strcmp(thread_pool.alias[i], p.alias) == 0))
					break;
			}
			if (i == MAX_CLIENTS)
			{
				strcpy(user_alias, p.alias);
				thread_pool.alias[d->thread_no] = (char *)malloc(sizeof(char) * (strlen(p.alias) + 1));
				strcpy(thread_pool.alias[d->thread_no], user_alias);
				strcpy(p.command, "ALIAS_OK");
				send(d->socket_no, (void* )&p, sizeof(struct packet), 0);
				pthread_mutex_unlock(&thread_pool.mutex);
				break;
			}
			else
			{
				strcpy(p.command, "ALIAS_IN_USE");
				send(d->socket_no, (void *)&p, sizeof(struct packet), 0);
			}
			pthread_mutex_unlock(&thread_pool.mutex);
			
		}
	}

	if (n == 0)
	{
		delete_from_thread_pool(d->thread_no);
		pthread_exit((void *)0);
	}
	else if (n < 0)
	{
		printf("Thread [%d] beklenmeyen bir hata olustu...\nHata kodu:%d\n", d->thread_no, n);
		delete_from_thread_pool(d->thread_no);
		pthread_exit((void *)1);
	}

	printf("[%d] soket numarali [%s] isimli client giris yapti.\n", d->socket_no, user_alias);

	
	


	while ((n = recv(d->socket_no, (void *)&p, sizeof(struct packet), 0)) > 0)
	{

		// client "LOGOUT" yazarsa thread sonlandırılır ve thread havuzunda karşılık gelen tahsis bayrağı
		// FALSE yapılır ve client ile bağlantı yapılan soket kapatılır.
		// global veri olan thread havuzuna erişirken mutex kullandığımıza dikkat!
		
		if (strcmp(p.command, "LOGOUT") == 0)
		{
			printf("%s isimli kullanici cikis yapti.\n", user_alias);
			delete_from_thread_pool(d->thread_no);

			int socket = d->socket_no;
			free(data);
			close(socket);
			pthread_exit((void *)0);
		}
		else if (strcmp(p.command, "SEND") == 0)
		{
			if (strcmp(p.alias, user_alias) == 0);

			else
			{
				int i;
				pthread_mutex_lock(&thread_pool.mutex);
				for (i = 0; i < MAX_CLIENTS; i++)
				{
					if (thread_pool.is_allocated[i] && strcmp(thread_pool.alias[i], p.alias) == 0)
					{
						strcpy(p.alias, user_alias);
						send(thread_pool.sockets[i], (void *)&p, sizeof(struct packet), 0);
						break;
					}
				}
				if (i == MAX_CLIENTS)
				{
					strcpy(p.command, "NOT_FOUND");
					if (send(d->socket_no, (void *)&p, sizeof(struct packet), 0) < 0)
					{
						delete_from_thread_pool(d->thread_no);
						pthread_mutex_unlock(&thread_pool.mutex);
						int socket = d->socket_no;
						free(data);
						close(socket);
						pthread_exit((void *)1);
					}
				}
				else
				{
					strcpy(p.command, "SEND_OK");
					if (send(d->socket_no, (void *)&p, sizeof(struct packet), 0) < 0)
					{
						delete_from_thread_pool(d->thread_no);
						pthread_mutex_unlock(&thread_pool.mutex);
						int socket = d->socket_no;
						free(data);
						close(socket);
						pthread_exit((void *)1);
					}
				}
				pthread_mutex_unlock(&thread_pool.mutex);
			}
		}

		else if (strcmp(p.command, "LIST_USERS") == 0)
		{
			memset(p.buff, 0, sizeof(p.buff));

			pthread_mutex_lock(&thread_pool.mutex);
			if (thread_pool.thread_count == 1)
				strcpy(p.buff, "SUAN CEVRIMICI KULLANICI YOK!\n");
			else
			{
				int i;
				for(i = 0; i < MAX_CLIENTS; i++)
				{
					if (thread_pool.is_allocated[i] && (i != d->thread_no))
					{
						strcat(p.buff, thread_pool.alias[i]);
						strcat(p.buff, "\n");
					}
				}
				p.buff[strlen(p.buff)] = '\0';
			}
			pthread_mutex_unlock(&thread_pool.mutex);

			if (send(d->socket_no, (void *)&p, sizeof(struct packet), 0) < 0)
			{
				delete_from_thread_pool(d->thread_no);
				int socket = d->socket_no;
				free(data);
				close(socket);
				pthread_exit((void *)1);
			}
		}
		else
		{
			strcpy(p.command, "INVALID_COMD");
			if (send(d->socket_no, (void *)&p, sizeof(struct packet), 0) < 0)
			{
				delete_from_thread_pool(d->thread_no);
				int socket = d->socket_no;
				free(data);
				close(socket);
				pthread_exit((void *)1);
			}
		}

		memset((void *)&p, 0, sizeof(struct packet));
		n = 0;
	}

	
	// recv sonucu dönen veri 0 ise client bağlantıyı kapatmıştır.
	// threadi sonlandır ve thread havuzunda karşılık gelen tahsis bayrağını
	// FALSE yap ve soketi kapat.

	if (n == 0)
	{
		printf("[%d] soket numarali [%s] isimli kullanici cevrimdisi...\n", d->socket_no, user_alias);
		delete_from_thread_pool(d->thread_no);
		int socket = d->socket_no;
		free(data);
		close(socket);
		pthread_exit((void *)0);

	}
	// recv çağrısının negatif sonucu hata belirtir..
	else if (n < 0)
	{
		printf("Thread [%d] beklenmeyen bir hata olustu...\nHata kodu:%d\n", d->thread_no, n);

		delete_from_thread_pool(d->thread_no);
		int socket = d->socket_no;
		free(data);
		close(socket);
		pthread_exit((void *)1);
	}



}


int main(int argc, char *argv[])
{
	int server_sock, client_socket, portno, client_len, n;

	struct sockaddr_in server_addr, client_addr;

	if(argc != 3)
	{
		printf("Kullanim: %s port_no ip_adresi\n", argv[0]);
		exit(1);
	}

	server_sock = socket(AF_INET, SOCK_STREAM, 0);

	if(server_sock < 0)
	{
		printf("HATA: soket acilamadi!\n");
		exit(1);
	}

	portno = atoi(argv[1]);

	memset((char *)&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	if (strcmp(argv[2], "localhost") == 0)
		server_addr.sin_addr.s_addr = INADDR_ANY;
	else
		server_addr.sin_addr.s_addr = inet_addr(argv[2]);

	if( bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
	{
		printf("HATA: soket bind edilemedi!\n");
		exit(1);
	}

	listen(server_sock, 5);
	
	init_thread_pool();


	


	while (client_socket = accept(server_sock, (struct sockaddr *)&client_addr, &client_len))
	{
		printf("%d soket numarali client baglandi.\n", client_socket);
		pthread_mutex_lock(&thread_pool.mutex);

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (thread_pool.is_allocated[i] == FALSE)
			{
				client_handler_data* d = (client_handler_data *)malloc(sizeof(client_handler_data));
				d->socket_no = client_socket;
				d->thread_no = i;

				int status = -1;
				status = pthread_create(&thread_pool.threads[i], (void *)NULL, client_handler, (void *)d);

				if (status < 0)
				{
					close(client_socket);
					printf("HATA: thread olusturulamadi.\n");
					close(server_sock);
					pthread_mutex_unlock(&thread_pool.mutex);
					exit(1);
				}

				thread_pool.is_allocated[i] = TRUE;
				thread_pool.sockets[i] = client_socket;
				thread_pool.thread_count++;
				break;
			}
		}
		// eğer thread havuzusunu belirten thread_pool yapısındaki kullanımdaki thread sayısını tutan thread_count değeri
		// MAX_CLIENT sayısına eşitse threadlerden biri sonlanırken sinyal gönderene kadar bekle...
		if (thread_pool.thread_count == MAX_CLIENTS)
			pthread_cond_wait(&thread_pool.not_full_cond, &thread_pool.mutex);

		pthread_mutex_unlock(&thread_pool.mutex);
	}

	pthread_mutex_destroy(&thread_pool.mutex);
	pthread_cond_destroy(&thread_pool.not_full_cond);
	for(int i = 0; i < MAX_CLIENTS; i++)
		free(thread_pool.alias[i]);

	printf("Sunucu kapatiliyor...\n");
	pthread_exit((void *)0);
}