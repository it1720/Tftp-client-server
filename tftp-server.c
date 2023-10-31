#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <arpa/inet.h>
#define BLOCK_SIZE 512
#define OPTION_BLOCKSIZE "blksize"
#define FILE_SIZE 1000000
struct TFTP{
    uint16_t opcode;
    uint16_t packet_info;
    char data[FILE_SIZE];
};

struct TFTP_REQUEST{
  uint16_t opcode;
  char filename[BLOCK_SIZE];
  char mode[BLOCK_SIZE];
  char options[BLOCK_SIZE];
};

struct ClientData {
    int client_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char root_dirpath[BLOCK_SIZE]; 
};
int timeout(int socket)
{
	// Setup timeval variable
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	// Setup fd_set structure
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(socket, &fds);
	// >0: data ready to be read
	return select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
}

int send_oack(int socket, struct sockaddr_in* client_addr,int blksize, int optionValue) {
    struct TFTP packet;
    packet.opcode = htons(6);
    char value[BLOCK_SIZE];
    sprintf(value, "%d", optionValue);
    int i = 0;
    int j = 0;
    // Sestaveni OACK
    if(blksize){
    while(OPTION_BLOCKSIZE[i] != '\0'){
        packet.data[i] = OPTION_BLOCKSIZE[i];
        i++;
      }
      packet.data[i] = '\0';
      i++;
      j = 0;
      while(value[j] != '\0'){
        packet.data[i] = value[j];
        i++;
        j++;
      }
      packet.data[i] = '\0';
      i++;
    }
    // Poslani OACK
    sendto(socket, &packet, 4 + strlen(OPTION_BLOCKSIZE) + strlen(value) + 1, 0, (struct sockaddr*)client_addr, sizeof(struct sockaddr_in));
    return 0;
}
// Function to send an error packet
void send_error_packet(int sock, struct sockaddr_in *client_addr, struct sockaddr_in *server_addr, uint16_t error_code, const char *error_message) {
    struct TFTP packet;
    packet.opcode = htons(5);
    packet.packet_info = htons(error_code);
    char err_msg[FILE_SIZE];
    switch (ntohs(packet.packet_info))
    {
    case 0:
        strcpy(err_msg,"Unknown error: ");
        strcat(err_msg,error_message);
        break;
    case 1:
        strcpy(err_msg,"File ");
        strcat(err_msg,error_message);
        strcat(err_msg," was not found");
        break;
    case 4:
        strcpy(err_msg,error_message);
    default:
        break;
    }
    strncpy(packet.data, err_msg, sizeof(packet.data));
    
    // Veliksot paketu =  Opcode + error_code + error_message + '\0'
    size_t packet_size = 4 + strlen(err_msg) + 1;
    
    sendto(sock, &packet, packet_size, 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
}

// Funkce pro odeslání TFTP DATA packetu
void send_data_packet(int sock, struct sockaddr_in *client_addr, struct sockaddr_in *server_addr,int block_num, char *data, int data_size) {
    struct TFTP packet;
    packet.opcode = htons(3);  // Opcode pro DATA packet
    packet.packet_info = htons(block_num);

    // Kopírování dat do packetu
    memcpy(packet.data, data, data_size);
    // Odeslání DATA packetu klientovi
    sendto(sock, &packet, 4 + data_size, 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
}

// Funkce pro odeslání TFTP ACK packetu
void send_ack_packet(int sock, struct sockaddr_in *client_addr, int block_num) {
    struct TFTP packet;
    packet.opcode = htons(4);  // Opcode pro ACK packet
    packet.packet_info = htons(block_num);
    // Odeslání ACK packetu serveru
    sendto(sock, &packet, 4, 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
}
// Funkce pro přijetí TFTP ACK packetu
int receive_ack_packet(int sock, struct sockaddr_in *client_addr, struct sockaddr_in *server_addr, int block_num) {
    struct TFTP packet;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    ssize_t recv_len = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)client_addr, &addr_len);

    if (recv_len < 0) {
        send_error_packet(sock,client_addr,server_addr,0,"Chyba pri prijmu ACK");
        return -1;
    }

    // Ověření, že přijatý packet je ACK
    if (ntohs(packet.opcode) != 4) {
        send_error_packet(sock,client_addr,server_addr, 0,"Chybny opcode v ACK");
        return -1;
    }
    // Kontrola bloku packetu
    if (ntohs(packet.packet_info) != block_num) {
        send_error_packet(sock,client_addr,server_addr,0,"Neocekavany blok ACK");
        return -1;
    }
    fprintf(stderr,"ACK {%s}:{%d} {%d}\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), block_num);
    return 0;
}

int receive_rrq_packet(struct TFTP_REQUEST packet, int sock, struct sockaddr_in *client_addr, struct sockaddr_in *server_addr,char *filename) {
    // Získání jména souboru a režimu
    char path[BLOCK_SIZE];
    char buffer[BLOCK_SIZE];
    char value[BLOCK_SIZE];
    char mode[BLOCK_SIZE];
    int blksize = BLOCK_SIZE;
    int i = 1;
    int size = 1;
    int j = 0;
    strcpy(path,filename);
    strncat(path, packet.filename, BLOCK_SIZE);
    strcpy(mode,packet.mode);
    // Delka OPTIONS
    while(i < BLOCK_SIZE){
        if(packet.options[i] == '\0' && packet.options[i-1] == '\0'){
            break;
        }
        i++;
    }
    size = i;
    i=0;
    int blksize_option = 0;
    int word = 0;
    if(size > 0){
    // OPTION BLKSIZE
    while(i <= size){
        buffer[j] = packet.options[i];
        i++;
        j++;
        if(packet.options[i] == '\0'){
            buffer[j] = '\0';
            i++;
            j=0;
            // Byl zadan option BLOCKSIZE
            if(strcmp(buffer,OPTION_BLOCKSIZE) == 0 && word % 2 == 0){
                // ziskani OPTION VALUE
                memset(&buffer[0], 0 , sizeof(buffer));
                while(packet.options[i] != '\0'){
                    buffer[j] = packet.options[i];
                    i++;
                    j++;
                }
                if(atoi(buffer) >= 8 &&  atoi(buffer) <= 65464){
                    blksize_option = 1;
                    blksize = atoi(buffer);
                }
            }
            // Vynulovani bufferu
            memset(&buffer[0], 0 , sizeof(buffer));
            j = 0;
            word ++;
        }
    }
    }
    int block_num = 1;
    if(blksize_option == 1){
        fprintf(stderr,"RRQ {%s}:{%d} \"{%s}\" {%s} {%s}={%d}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), filename, mode,OPTION_BLOCKSIZE,blksize);
    }else{
        fprintf(stderr,"RRQ {%s}:{%d} \"{%s}\" {%s}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), filename, mode);
    }
    if(blksize_option == 1){
        send_oack(sock, client_addr, blksize_option, blksize);
        if (receive_ack_packet(sock, client_addr, server_addr,block_num - 1) == -1) {
                // Chyba při příjmu ACK packetu
                send_error_packet(sock,client_addr,server_addr,0,"Chyba pri prijmu ack");
                return -1;
            }
    }
    FILE *file;
    // Otevření požadovaného souboru ve spravnem modu
    if(strcmp("octet",mode) == 0)
        file = fopen(path, "rb");
    else
        file = fopen(path, "r");
        // Soubor nelze otevrit
        if (file == NULL) {
            send_error_packet(sock,client_addr,server_addr,1,packet.filename);
            return -1;
        }
        // Čtení a odesílání souboru po blocích
        char data[blksize];
        size_t bytes_read;
        while (1) {
            bytes_read = fread(data, 1, blksize, file);
            if (bytes_read == 0) {
                // Konec souboru
                break;
            }
            send_data_packet(sock, client_addr, server_addr,block_num, data, (int)bytes_read);
            //ACK packet
            if (receive_ack_packet(sock, client_addr,server_addr, block_num) == -1) {
                // Chyba při příjmu ACK packetu
                send_error_packet(sock,client_addr,server_addr,0,"Chyba pri prijmu ack");
                return -1;
            }
            block_num++;

            if (bytes_read < blksize) {
                // Konec souboru
                break;
            }
        }
        // Uzavření souboru
        fclose(file);
        printf("Poslan soubor %s klientovi.\n", path);
        return 0;
}

// Prijem WRQ packetu
int receive_wrq_packet(struct TFTP_REQUEST packet,int sock, struct sockaddr_in *client_addr, struct sockaddr_in *server_addr,char *root_dirpath) {
    char path[BLOCK_SIZE];
    char buffer[BLOCK_SIZE];
    char mode[BLOCK_SIZE];
    // Read the filename
    strcpy(path,root_dirpath);
    strcat(path, packet.filename);
    strcpy(mode,packet.mode);
    int blksize = BLOCK_SIZE;
    int j = 0;
    int size = 1;
    int i = 1;
    // kontrola zda se jedna option, nebo value
    int word = 0;    
    // Delka OPTIONS
    while(i < BLOCK_SIZE){
        if(packet.options[i] == '\0' && packet.options[i-1] == '\0'){
            break;
        }
        i++;
    }
    int blksize_option = 0;
    size = i;
    i=0;
    if(size > 0){
    // OPTION BLKSIZE
    while(i <= size){
        buffer[j] = packet.options[i];
        i++;
        j++;
        if(packet.options[i] == '\0'){
            buffer[j] = '\0';
            i++;
            j=0;
            // Byl zadan option BLOCKSIZE
            if(strcmp(buffer,OPTION_BLOCKSIZE) == 0 && word % 2 == 0){
                // ziskani OPTION VALUE
                memset(&buffer[0], 0 , sizeof(buffer));
                while(packet.options[i] != '\0'){
                    buffer[j] = packet.options[i];
                    i++;
                    j++;
                }
                blksize_option = 1;
                blksize = atoi(buffer);
            }
            // Vynulovani bufferu
            memset(&buffer[0], 0 , sizeof(buffer));
            j = 0;
            word++;
        }
    }
    }
    int block_num = 0;
    FILE *file;
    // Otevření požadovaného souboru ve spravnem modu
    if(strcmp("octet",mode) == 0)
        file = fopen(path, "wb");
    else
        file = fopen(path, "w");
    // soubor nelze otevrit
        if (file == NULL) {
            send_error_packet(sock,client_addr,server_addr,1,packet.filename);
            return -1;
        }
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    // Zaslani prvniho ACK, nebo OACK
    if(blksize_option == 1){
        fprintf(stderr,"WRQ {%s}:{%d} \"{%s}\" {%s} {%s}={%d}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), packet.filename, mode,OPTION_BLOCKSIZE,blksize);
        send_oack(sock, client_addr, blksize_option, blksize);
    }else{
        fprintf(stderr,"WRQ {%s}:{%d} \"{%s}\" {%s}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), packet.filename, mode);
        send_ack_packet(sock,client_addr,block_num);
    }
    block_num++;
    struct TFTP packet_data;
    int retry = 0;
    // 3 pokusy nez se spojeni ukonci
    while (retry <= 3) {
    int time = timeout(sock);
    ssize_t recv_len  = recvfrom(sock, &packet_data, sizeof(packet), 0, (struct sockaddr *)client_addr, &server_addr_len);
    // Neybl prijat paket
    if (recv_len < 0 && time == 0) {
            printf("Chyba při příjmu DATA packetu");
            send_ack_packet(sock,client_addr,block_num);
            // Pokus
            retry++;
    }
    else{
        // Ověření, že přijatý packet je DATA
    if (ntohs(packet_data.opcode) != 3) {
            send_error_packet(sock,client_addr,server_addr,0,"Chybný opcode v DATA packetu.\n");
            close(sock);
            exit(1);
            continue;
        }
    if(ntohs(packet_data.packet_info) != block_num){
        send_error_packet(sock,client_addr,server_addr,0,"Neocekavany blok dat.\n");
        close(sock);
        exit(1);
    }
    fprintf(stderr,"DATA {%s}:{%d}:{%d} {%d}\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), ntohs(server_addr->sin_port), block_num);
    fwrite(packet_data.data, 1, recv_len - 4, file);
    send_ack_packet(sock,client_addr,block_num);
    if(recv_len - 4 < blksize){
          break;
    }
    block_num++;
    }
    }
    printf("Přijat soubor %s.\n", packet.filename);
    fclose(file);
    return 0;
}

int receive_packet(int sock, struct sockaddr_in *client_addr,struct sockaddr_in *server_addr, char *filename){
    struct TFTP_REQUEST packet;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Přijmutí packetu od klienta
    ssize_t recv_len = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)client_addr, &client_addr_len);

    if (recv_len < 0) {
        perror("Chyba při přijímání prvního packetu");
        return -1;
    }
    // Ověření, zda je přijatý packet je RRQ
    if (ntohs(packet.opcode) == 1) {
        return receive_rrq_packet(packet, sock, client_addr, server_addr,filename);
    }
    // Ověření, zda je přijatý packet je WRQ
    else if(ntohs(packet.opcode) == 2){
        receive_wrq_packet(packet, sock, client_addr, server_addr, filename);
    }
    else{
        send_error_packet(sock,client_addr,server_addr,4,"First packet opcode must be 0 or 1");
    }
    return 1;
}

void* handle_client(void* client_data_ptr) {
    struct ClientData* client_data = (struct ClientData*)client_data_ptr;

    // Access the client_socket and server_addr fields
    int client_socket = client_data->client_socket;
    struct sockaddr_in server_addr = client_data->server_addr;
    struct sockaddr_in client_addr = client_data->client_addr;
    char filename[BLOCK_SIZE];
    strcpy(filename,client_data->root_dirpath);
    

    // Handle TFTP requests for this client
    // Implement your TFTP server logic in the handle_client function
    if (receive_packet(client_socket, &client_addr, &server_addr, filename) == -1) {
            // Chyba při příjmu RRQ packetu
            close(client_socket);
        }
    free(client_data_ptr);
    close(client_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int port=69;
    char ch;
    char *root_dirpath = NULL;
    int sock;
    socklen_t addr_len;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char mode[BLOCK_SIZE];
    if(argc > 4){
        printf("Usage: ./server [-p port] root_dirpath\n");
	    exit(EXIT_FAILURE);
    }
	while ((ch = getopt(argc, argv, "p:")) != -1) {
	    switch (ch) {
			case 'p':
				port = atoi(optarg);
				break;
	    case '?':
	      printf("Usage: ./server [-p port] root_dirpath\n");
	      exit(EXIT_FAILURE);
	    }
    if (optind < argc) {
        root_dirpath = argv[optind];
	}
    }
    if(port <= 0 || port >= 65535){
        fprintf(stderr,"Port musi byt mezi 0 az 65 535");
        exit(1);
    }
    // Vytvoření socketu
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        fprintf(stderr,"Chyba při vytváření socketu");
        exit(1);
    }
    
    // Nastavení adresy serveru
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    // Vázání socketu na zvolený port
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr,"Chyba při vázání socketu");
        close(sock);
        exit(1);
    }
    while (1) {
        if(receive_packet(sock,&client_addr,&server_addr,root_dirpath) == -1){

        }
    }

    return 0;
}