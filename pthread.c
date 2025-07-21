#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

pthread_cond_t hasData = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; 

void *addr = NULL;
int frame_size = 64;
uint16_t ue_id_min = 1;
uint16_t ue_id_max = 100;

void write_big_endian16(uint8_t* buf, uint16_t value) {
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
}

void write_big_endian32(uint8_t* buf, uint32_t value) {
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8)  & 0xFF;
    buf[3] = (value)       & 0xFF;
}

uint16_t generate_random(uint16_t min, uint16_t max) {
    return min + (uint16_t)(random() % (max - min + 1));
}

uint8_t* generate_frame(int generate_frame_size) {
    uint16_t ue_id = generate_random(ue_id_min, ue_id_max);
    uint16_t drb_id = generate_random((uint16_t)1, (uint16_t)4);
    uint32_t length = generate_frame_size;
    int payload_size = generate_frame_size - 8;

    uint8_t *frame = (uint8_t*)malloc(generate_frame_size);

    write_big_endian16(frame, ue_id);
    write_big_endian16(frame + 2, drb_id);
    write_big_endian32(frame + 4, length);
    for (int i = 0; i < payload_size; i++) {
        frame[8 + i] = generate_random(0, 255);
    }
    return frame;
}

void *mmap_create() {
    int fd = open("rlc_data.txt", O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (fd < 0) {
        printf("open failed !\n");
        return NULL;
    }
    ftruncate(fd, 1600); // 扩展文件大小！
    addr = mmap(NULL, 1600, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if(addr == MAP_FAILED){
        perror("mmap");
        return NULL;
    }
    return addr;
}

void *wr_handle(void *arg) {
    int ret_detach = pthread_detach(pthread_self());
    if (ret_detach != 0) {
        printf("wr_handle pthread_detach failed!\n");
        return NULL;
    }

    while(1) {
        pthread_mutex_lock(&lock);
        uint8_t *frameAddr = generate_frame(frame_size);
        memcpy(addr, frameAddr, frame_size);
        pthread_cond_signal(&hasData);
        pthread_mutex_unlock(&lock);
        usleep(1000);
    }
    pthread_exit(NULL);
    return NULL;
}

void *rd_handle(void *arg) {
    int ret_detach = pthread_detach(pthread_self());
    if (ret_detach != 0) {
        printf("rd_handle pthread_detach failed!\n");
        return NULL;
    }

    while(1) {
        pthread_mutex_lock(&lock);
        pthread_cond_wait(&hasData, &lock);
        printf("frame data : ");
        //sizeof() is correct? what 's the meaning of %02x
        printf("\n");
        for (int i = 0; i < 64; i++) {
            printf("%02x", ((uint8_t*)addr)[i]);
        }
        printf("\n");
        pthread_mutex_unlock(&lock);
    }
    pthread_exit(NULL);
    return NULL;
}

void pthread_create_function() {
    pthread_t wr_pthread, rd_pthread;
    int ret = pthread_create(&wr_pthread, NULL, wr_handle, NULL);
    if (ret != 0) {
            printf("wr_pthrad failed!\n");
            return ;
    }

    ret = pthread_create(&rd_pthread, NULL, rd_handle, NULL);
    if (ret != 0) {
            printf("wr_pthrad failed!\n");
            return ; 
    }
}

int main(){
    srand(time(NULL));
    addr = mmap_create();
    if (!addr) return -1;  
    pthread_create_function();
      
    while (1) {
        sleep(1);
    }
}

