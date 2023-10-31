#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>
#define BLOCK_SIZE 512
#define FILE_SIZE 1000000
#define OPTION_BLOCKSIZE "blksize"
#define OPTION_BLOCKSIZE_VALUE "512"

struct TFTP_REQUEST{
  uint16_t opcode;
  char filename[BLOCK_SIZE];
  char mode[BLOCK_SIZE];
  char options[BLOCK_SIZE];
};

struct TFTP{
    uint16_t opcode;
    uint16_t packet_info;
    char data[FILE_SIZE];
};


// Funkce pro odeslání TFTP DATA packetu
void send_data_packet(int sockfd, struct sockaddr_in *server_addr, int port,int block_num, char *data, int data_size) {
    struct TFTP packet;
    packet.opcode = htons(3);  // Opcode pro DATA packet
    packet.packet_info = htons(block_num);

    // Kopírování dat do packetu
    memcpy(packet.data, data, data_size);
    
    // Odeslání DATA packetu serveru
    sendto(sockfd, &packet, 4 + data_size, 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in));
}

int send_rrq_packet(int sockfd, struct sockaddr_in *server_addr, const char *filename, const char *mode) {
    struct TFTP_REQUEST packet;
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    char option_blocksize[BLOCK_SIZE] = OPTION_BLOCKSIZE;
    // Nastavení opcode pro RRQ
    packet.opcode = htons(1); // 1 pro RRQ

    // Nastavení jména souboru
    strcpy(packet.filename, filename);
    // Nastavení režimu
    strcpy(packet.mode, mode);
    int i = 0;
    int j = 0;
    // Kontrola zda je spravne nastavena blksize Option
    if(option_blocksize != "\0" && atoi(OPTION_BLOCKSIZE_VALUE) >= 8 &&  atoi(OPTION_BLOCKSIZE_VALUE) <= 65464 && strcmp(OPTION_BLOCKSIZE,"blksize") == 0){
      // Option
      while(OPTION_BLOCKSIZE[i] != '\0'){
        packet.options[i] = OPTION_BLOCKSIZE[i];
        i++;
      }
      packet.options[i] = '\0';
      i++;
      j = 0;
      // Hodnota option
      while(OPTION_BLOCKSIZE_VALUE[j] != '\0'){
        packet.options[i] = OPTION_BLOCKSIZE_VALUE[j];
        i++;
        j++;
      }
      packet.options[i] = '\0';
      i++;
    }
    // Odeslání RRQ packetu na server
    ssize_t send_len = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)server_addr, server_addr_len);

    if (send_len < 0) {
        perror("Chyba při odesílání RRQ packetu");
        return -1;
    }

    return 0;
}

int send_wrq_packet(int sockfd, struct sockaddr_in *server_addr, char *filename, const char *mode) {
    struct TFTP_REQUEST packet;
    char option_blocksize[BLOCK_SIZE] = OPTION_BLOCKSIZE;
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    // Opcode pro WRQ packet
    packet.opcode = htons(2);  
    // Nastavení jména souboru
    strcpy(packet.filename, filename);
    // Nastavení režimu
    strcpy(packet.mode, mode);
    int i = 0;
    int j = 0;
    // Kontrola zda je spravne nastavena blksize Option
    if(option_blocksize != "\0" && atoi(OPTION_BLOCKSIZE_VALUE) >= 8 &&  atoi(OPTION_BLOCKSIZE_VALUE) <= 65464 && strcmp(OPTION_BLOCKSIZE,"blksize") == 0){
      // Option
      while(OPTION_BLOCKSIZE[i] != '\0'){
        packet.options[i] = OPTION_BLOCKSIZE[i];
        i++;
      }
      packet.options[i] = '\0';
      i++;
      j = 0;
      // Hodnota option
      while(OPTION_BLOCKSIZE_VALUE[j] != '\0'){
        packet.options[i] = OPTION_BLOCKSIZE_VALUE[j];
        i++;
        j++;
      }
      packet.options[i] = '\0';
      i++;
    }
    // Odeslání WRQ packetu na server
    ssize_t send_len = sendto(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)server_addr, server_addr_len);
    if (send_len < 0) {
        perror("Chyba při odesílání WRQ packetu");
        return -1;
    }
    return 0;
}

// Funkce pro odeslání TFTP ACK packetu
void send_ack_packet(int sockfd, struct sockaddr_in *server_addr, int block_num) {
    struct TFTP packet;
    packet.opcode = htons(4);  // Opcode pro ACK packet
    packet.packet_info = htons(block_num);
    // Odeslání ACK packetu serveru
    sendto(sockfd, &packet, 4, 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in));
}

void receive_err_packet(struct TFTP packet, struct sockaddr_in *server_addr, int port){
  fprintf(stderr,"%s\n",packet.data);
  fprintf(stderr,"ERROR {%s}:{%d}:{%d} {%d} \"{%s}\"\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), port, htons(packet.packet_info),packet.data);
  return;
}
int receive_oack_packet(struct TFTP packet,struct sockaddr_in *server_addr){
  int i = 1;
  int size = 1;
  int j = 0;
  char buffer[BLOCK_SIZE];
  int blksize;
  // BLKSIZE  OPTION
  // Delka OPTIONS
    while(i < BLOCK_SIZE){
        if(packet.data[i] == '\0' && packet.data[i-1] == '\0'){
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
        buffer[j] = packet.data[i];
        i++;
        j++;
        if(packet.data[i] == '\0'){
            buffer[j] = '\0';
            i++;
            j=0;
            // Byl zadan option BLOCKSIZE
            if(strcmp(buffer,OPTION_BLOCKSIZE) == 0 && word % 2 == 0){
                // ziskani OPTION VALUE
                memset(&buffer[0], 0 , sizeof(buffer));
                while(packet.data[i] != '\0'){
                    buffer[j] = packet.data[i];
                    i++;
                    j++;
                }
                j++;
                // Kontrola zda je zadana hodnota stejna kterou jsme poslali
                if(atoi(buffer) >= 8 &&  atoi(buffer) <= 65464){
                    blksize_option = 1;
                    if(atoi(buffer) != atoi(OPTION_BLOCKSIZE_VALUE)){
                      return -1;
                    }
                    blksize = atoi(buffer);
                }
            }
            // Vynulovani bufferu
            memset(&buffer[0], 0 , sizeof(buffer));
            j = 0;
            word ++;
        }
    }
    fprintf(stderr,"OACK {%s}:{%d} {%s}={%d}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), OPTION_BLOCKSIZE,blksize);
    }
  return 1;
}

// Funkce pro přijetí TFTP ACK packetu
int receive_ack_packet(int sockfd, struct sockaddr_in *server_addr, int block_num, int port) {
    struct TFTP packet;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)server_addr, &addr_len);

    if (recv_len < 0) {
        perror("Chyba při příjmu ACK packetu");
        return -1;
    }
    if (ntohs(packet.opcode) == 5){
          receive_err_packet(packet,server_addr, port);
          close(sockfd);
          return -1;
        }
    // OACK
    if(ntohs(packet.opcode) == 6 && block_num == 0){
      return receive_oack_packet(packet, server_addr);
    }else{
      // Ověření, že přijatý packet je ACK
      if (ntohs(packet.opcode) != 4) {
          fprintf(stderr, "Chybný opcode v ACK packetu.\n");
          return -1;
      }
      if (ntohs(packet.packet_info) != block_num) {
          fprintf(stderr, "Neočekávaný ACK. Očekáváno: %d, Přijato: %d\n", ntohs(packet.packet_info), block_num);
          // Zde můžete poslat ERROR packet klientovi
          return -1;
      }
      fprintf(stderr,"ACK {%s}:{%d} {%d}\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port), block_num);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char* server_hostname = NULL,*file_path=NULL,*dest_file_path;
    int port=69,opt,sock;
    int block_num;
    int download = 0;
    int blksize = BLOCK_SIZE;
    struct sockaddr_in server_addr;
    if(argc < 5 || argc > 9){
      fprintf(stderr, "usage: %s -h <host> -p <port> -m <mode>\n", argv[0]);
      exit(EXIT_FAILURE);
    }
    while (opt != -1)
  {
    switch (opt = getopt(argc, argv, ":h:p:f:t:"))
    {
    case 'h':
      server_hostname = optarg;
      continue;
    case 'p':
      port = atoi(optarg);
      continue;
    case 'f':
      download = 1;
      file_path = optarg;
      continue;
    case 't':
      dest_file_path = optarg;
      continue;
    default:
      fprintf(stderr, "usage: %s -h <host> -p <port> -m <mode>\n", argv[0]);
      exit(EXIT_FAILURE);
      break;
    case -1:
      break;
    }
  }
  if(server_hostname == NULL){
    printf("Nebyl zadan host\n}");
    fprintf(stderr, "usage: %s -h <host> -p <port> -m <mode>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  // Vytvoření socketu
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Chyba při vytváření socketu");
        exit(1);
    }

    // Nastavení adresy serveru
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Nastavení IP adresy serveru (změňte na reálnou adresu serveru)
    if (inet_pton(AF_INET, server_hostname, &server_addr.sin_addr) <= 0) {
        perror("Chyba při nastavení IP adresy serveru");
        close(sock);
        exit(1);
    }
    // BLKSIZE  OPTION
    if(atoi(OPTION_BLOCKSIZE_VALUE) >= 8 &&  atoi(OPTION_BLOCKSIZE_VALUE) <= 65464 && strcmp(OPTION_BLOCKSIZE,"blksize") == 0)
      blksize = atoi(OPTION_BLOCKSIZE_VALUE);
    // RRQ
    if(file_path != NULL){
   // Odešlete RRQ packet na server
    if (send_rrq_packet(sock, &server_addr, file_path, "netascii") == -1) {
        // Chyba při odesílání RRQ packetu
        close(sock);
        exit(1);
    }

    block_num = 1;
    
    //soubor pro zapis
    FILE *file = fopen(dest_file_path, "wb");
        if (file == NULL) {
            fprintf(stderr,"Chyba při otevírání souboru");
            close(sock);
            exit(EXIT_FAILURE);
        }
    struct TFTP packet;
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    while (1) {
        // Přijmutí DATA packetu od serveru
        ssize_t recv_len  = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&server_addr, &server_addr_len);
        if (recv_len < 0) {
            perror("Chyba při příjmu DATA packetu");
            // Zde můžete poslat ERROR packet serveru
            continue;
        }
        // Error packet
        if (ntohs(packet.opcode) == 5){
          receive_err_packet(packet,&server_addr,port);
          close(sock);
          exit(1);
        }
        
        // OACK, prvni prijaty packet
        if (ntohs(packet.opcode) == 6 && (block_num - 1) == 0 ){
          if(receive_oack_packet(packet,&server_addr) == -1){
            close(sock);
            exit(1);
          }
          send_ack_packet(sock, &server_addr, block_num - 1);
        }
        // Packet data
        else{
          if(ntohs(packet.opcode) != 3){
            fprintf(stderr, "Chybný opcode v DATA packetu.\n");
            // Zde můžete poslat ERROR packet serveru
            continue;
          }
          if(ntohs(packet.packet_info) != block_num){
            fprintf(stderr,"Neocekavany blok dat");
            close(sock);
            exit(1);
        }
        fprintf(stderr,"DATA {%s}:{%d}:{%d} {%d}\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port), port, block_num);
        // Ulozeni dat do souboru
        fwrite(packet.data, 1, recv_len - 4, file);
        // Odeslání ACK packetu serveru pro potvrzení
        send_ack_packet(sock, &server_addr, block_num);
        // Ukonceni
        if(recv_len - 4 < blksize){
          break;
        }
        block_num++;
        }
    }
    printf("Prijat soubor: %s\n",file_path);
    // WRQ
    }else{
      block_num = 0;
      if (send_wrq_packet(sock, &server_addr, dest_file_path, "netascii") == -1) {
        // Chyba při odesílání WRQ packetu
        close(sock);
        exit(1);
    }
    // Prijat ACK nebo OACK
    if(receive_ack_packet(sock, &server_addr, block_num, port) == -1){
      close(sock);
      return 0;
    }
    block_num++;
    char buffer[FILE_SIZE];
    int data_size = fread(buffer, 1, FILE_SIZE, stdin);
    while (1) {
        char data[blksize];
        memcpy(data, buffer + (blksize * (block_num-1)), blksize);
        // Pokud je data_size 0, dosáhli jsme konce stdin
        
        // Příjem ACK packetu od serveru
        if(block_num * blksize <= data_size)
          send_data_packet(sock, &server_addr, port,block_num, data, blksize);
        else
          send_data_packet(sock, &server_addr, port,block_num, data, data_size - ((block_num-1)* blksize));
        // Odeslání datového bloku serveru
        if (receive_ack_packet(sock, &server_addr, block_num, port) == -1) {
            // Chyba při příjmu ACK packetu
            // Zde můžete poslat ERROR packet serveru
            close(sock);
            return 0;
        }
        if (block_num * blksize >= data_size) {
            break;
        }
        // Zvýšení blokového čísla pro další blok
        block_num++;
        
    }
    }
    close(sock);
  return 0;
}