
extern int openInputFile(config* conf);
extern int writeHeader(config* conf);
extern int write_file(int fd, u_char type, int seq_count, ulong len, u_char * content);
extern void writeFinish(int fd);

