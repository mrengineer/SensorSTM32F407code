#ifndef _SD_OPS
#define _SD_OPS

typedef enum {LOG, DATA, SETTINGS, DOWNLOAD} FileType;

#define SD_WRITE_BUFFER         1024*16      //����� ������ ��� ������ 
#define MAX_FILE_SIZE           SD_WRITE_BUFFER+27000       //������ ����� actual.dat ����� �������� ��� �������� 
                                                            //�� �������� �� ������, � ������ �. �. ����� SD_WRITE_BUFFER
#define WRITE_SAFE_BLOCK        512                         //���� ������ ����� 512 ����, ������ �����

void NVIC_Configuration(void);

int save_buffer         (FileType FT, char *buff);
int save                (FileType FT, char const* fmt, ... );
void read_all           (char * filename);
void flush_to_sd        (void);
void save_value         (char Axis, float Filtered, float a, float b);
void settings_to_sd     (void);
void save_raw_values    (int X, int Y, int Z);

void mount              (void);

#endif