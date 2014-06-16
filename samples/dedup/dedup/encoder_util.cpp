#include <stdio.h>
#include "dedup_debug.h"
#include "util.h"



int openInputFile(config* conf){
    struct stat filestat;
    if (stat(conf->infile, &filestat) < 0) {
        EXIT_TRACE("input file not found: %s\n", conf->infile);
    }

    if (!(filestat.st_mode & S_IFREG)) {
        EXIT_TRACE("not a normal file: %s\n", conf->infile);
    }

    int fd = OPEN(conf->infile, O_RDONLY | O_LARGEFILE, S_IREAD);
    /* src file open */
    if( fd < 0) {
        EXIT_TRACE("%s file open error %s\n", conf->infile, strerror(errno));
    }
    return fd;
}


int writeHeader(config* conf){
    int fd = OPEN(conf->outfile, O_CREAT|O_WRONLY|O_TRUNC, S_IREAD | S_IWRITE);

    if (fd < 0) {
        perror("SendBlock open");
        EXIT_TRACE1("opening output file fails\n");
        return 0;
    }
    u_char type = TYPE_HEAD;
    if (xwrite(fd, &type, sizeof(u_char)) < 0){
        perror("xwrite:");
        EXIT_TRACE1("xwrite type fails\n");
        return 0;
    }
    int checkbit = CHECKBIT;
    if (xwrite(fd, &checkbit, sizeof(int)) < 0){
        EXIT_TRACE1("xwrite head fails\n");
    }

    send_head head;
    memset(&head.filename[0],0,LEN_FILENAME);
    strcpy(&head.filename[0],conf->infile);
    head.fid = 1;
    if (xwrite(fd, &head, sizeof(send_head)) < 0){
        EXIT_TRACE1("xwrite head fails\n");
    }
    return fd;
}

int write_file(int fd, u_char type, int seq_count, ulong len, u_char * content) {
    if (xwrite(fd, &type, sizeof(type)) < 0){
        perror("xwrite:");
        EXIT_TRACE1("xwrite type fails\n");
        return -1;
    }
    if (xwrite(fd, &seq_count, sizeof(seq_count)) < 0){
        EXIT_TRACE1("xwrite content fails\n");
    }
    if (xwrite(fd, &len, sizeof(len)) < 0){
       EXIT_TRACE1("xwrite content fails\n");
    }
    if (xwrite(fd, content, len) < 0){
       EXIT_TRACE1("xwrite content fails\n");
    }
    return 0;
}

void writeFinish(int fd){
    u_char type = TYPE_FINISH;
    if (xwrite(fd, &type, sizeof(type)) < 0){
        perror("xwrite:");
        EXIT_TRACE1("xwrite type fails\n");
        return;
    }
    CLOSE(fd);
}



