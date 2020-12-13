#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>

#define ERR_MMAP 2
#define ERR_MALLOC 3
#define ERR_REALLOC 4
#define ERR_READ 5
#define ERR_WRITE 6
#define ERR_OPEN 7
#define ERR_CLOSE 8
#define ERR_PIPE 9
#define ERR_FORK 10
#define ERR_WAIT 11

#define ERR_DIV_0 20
#define ERR_INV_INT 21
#define ERR_NO_STATUS 22
#define ERR_SIZE 23

#define CHILD_BLOCK 24
#define PAR_BLOCK 25

#define PAGESIZE 2048

/* 
 * Пользователь вводит команды вида: «число число число<endline>». Далее эти числа передаются от родительского процесса в дочерний. 
 * Дочерний процесс производит деление первого числа, на последующие, а результат выводит в файл. 
 * Если происходит деление на 0, то тогда дочерний и родительский процесс завершают свою работу. 
 * Проверка деления на 0 должна осуществляться на стороне дочернего процесса. Числа имеют тип int.
*/

int get_int_length(int num) { // Считает длину числа
    int length = 0;
    if (num < 0) {
        ++length;
    }
    do {
        num /= 10;
        ++length;
    } while (num != 0);
    return length;
}

void int_to_string(char* s, int x) { // Преобразует число в строку
    bool is_negative = x < 0;
    if (is_negative) {
        x *= -1;
    }

    bool is_first = true;
    char* p;
    do {
        if (is_first) {
            p = s;
            is_first = false;
        } else {
            ++p;
        }
        *p = '0' + (x % 10);
        x /= 10;
    } while (x != 0);

    if (is_negative) {
        ++p;
        *p = '-';
    }

    while (s < p) {
        char tmp = *s;
        *s = *p;
        *p = tmp;
        ++s;
        --p;
    }
}

void read_strings(int fd, char*** ptr_buffer, int* res_size) { // Считывает строки через пробел до конца ввода
    char c; // Текущий считываемый символ
    int str_idx = 0; // Индекс текущего слова
    size_t buffer_size = 1; // Кол-во строк
    char** buffer = malloc(sizeof(char*) * buffer_size);
    if (!buffer) {
        perror("Malloc error");
        exit(ERR_MALLOC);
    }
    int* lengths = malloc(sizeof(int) * buffer_size);
    if (!lengths) {
        perror("Malloc error");
        exit(ERR_MALLOC);
    }

    lengths[0] = 0;
    int str_size = 1; // Длина текущей строки
    buffer[0] = malloc(sizeof(char) * str_size);
    if (!buffer[0]) {
        perror("Malloc error");
        exit(ERR_MALLOC);
    }
    size_t read_bytes = 0;
    while ((read_bytes = read(fd, &c, sizeof(char))) > 0) {
        if ((read_bytes != sizeof(char)) && (read_bytes != 0)) {
            perror("Read error");
            exit(ERR_READ);
        }

        if (c == '\n') { // Конец ввода слов
            break;
        }
        if (c == ' ') { // Следующее слово
            ++str_idx;
            str_size = 1;
            continue;
        }

        // Увеличиваем буфер строк и длин
        if (str_idx >= buffer_size) {
            ++buffer_size;
            str_size = 1;

            buffer = realloc(buffer, sizeof(char*) * buffer_size);
            if (!buffer) {
                perror("Realloc error");
                exit(ERR_REALLOC);
            }

            buffer[buffer_size - 1] = malloc(sizeof(char) * str_size);
            if (!buffer[buffer_size - 1]) {
                perror("Malloc error");
                exit(ERR_MALLOC);
            }
            for (size_t i = 0; i < str_size; ++i) {
                buffer[buffer_size - 1][i] = 0;
            }

            lengths = realloc(lengths, sizeof(int) * buffer_size);
            if (!lengths) {
                perror("Realloc error");
                exit(ERR_REALLOC);
            }
            lengths[buffer_size - 1] = 0;
        }

        // Увеличиваем длину строки
        if (lengths[str_idx] == str_size) {
            str_size += 16;
            buffer[str_idx] = realloc(buffer[str_idx], sizeof(char) * str_size);
            if (!buffer[str_idx]) {
                perror("Realloc error");
                exit(ERR_REALLOC);
            }
        }
        
        buffer[str_idx][lengths[str_idx]] = c;
        ++lengths[str_idx];
    }

    for (int i = 0; i < buffer_size; ++i) {
        buffer[i] = realloc(buffer[i], sizeof(char) * lengths[i]);
        if (!buffer[i]) {
            perror("Realloc error");
            exit(ERR_REALLOC);
        }
        buffer[i][lengths[i]] = '\0';
    }
    *res_size = buffer_size;
    *ptr_buffer = buffer;
}

// str[4] = get_string();
void read_string(int fd, char** str, size_t* size) { // Считывает одну строку
    char c; // Текущий считываемый символ
    size_t str_len = 0;
    size_t str_size = 1; // Длина текущей строки
    char* buffer = malloc(sizeof(char) * str_size);
    if (!buffer) {
        perror("Malloc error");
        exit(ERR_MALLOC);
    }

    size_t read_bytes = 0;
    while ((read_bytes = read(fd, &c, sizeof(char))) > 0) {
        if ((read_bytes != sizeof(char)) && (read_bytes != 0)) {
            perror("Read error");
            exit(ERR_READ);
        }

        if (c == '\n') { // Конец ввода слов
            break;
        }

        // Увеличиваем длину строки
        if (str_len == str_size) {
            str_size += 16;
            buffer = realloc(buffer, sizeof(char) * str_size);
            if (!buffer) {
                perror("Realloc error");
                exit(ERR_REALLOC);
            }
        }
        
        buffer[str_len] = c;
        ++str_len;
    }

    buffer = realloc(buffer, sizeof(char) * str_len);
    if (!buffer) {
        perror("Realloc error");
        exit(ERR_REALLOC);
    }
    buffer[str_len] = '\0';

    *size = str_len;
    *str = buffer;
}

void str_array_to_int(char** strs, int nums[], size_t n) { // Преобразует массив строк в массив int'ов
    for (size_t i = 0; i < n; ++i) {
        ///nums[i] = atoi(strs[i]);
        char* p = strs[i];
        bool is_negative = false;
        int result = 0;
        while(*p != '\0') {
            if (*p == '-' || *p == '+') {
                if (p != strs[i]) {
                    perror("Invalid int");
                    exit(ERR_INV_INT);
                }
                if (*p == '-') {
                    is_negative = true;
                }
            } else if ('0' <= *p && *p <= '9') {
                result = result * 10 + (*p - '0');
            } else {
                perror("Invalid int");
                exit(ERR_INV_INT);
            }
            ++p;
        }
        if (is_negative) {
            result *= -1;
        }
        nums[i] = result;
    }
}

void write_to_fd(int fd, const void *buf, size_t nbytes) {
    if (write(fd, buf, nbytes) != nbytes) {
        perror("Write error");
        exit(ERR_WRITE);
    }
}

void read_from_fd(int fd, void *buf, size_t nbytes) {
    if (read(fd, buf, nbytes) != nbytes) {
        perror("Read error");
        exit(ERR_READ);
    }
}

int main() {
    int* m_file = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (m_file == MAP_FAILED) {
        perror("mmap error");
        exit(ERR_MMAP);
    }

    int* sig = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sig == MAP_FAILED) {
        perror("mmap error");
        exit(ERR_MMAP);
    }
    *sig = CHILD_BLOCK;

    printf("Enter path to file:\n");
    char* path;
    size_t path_size = 0;
    read_string(0, &path, &path_size); // Читает имя выходного файла

    int status;
    int id = fork();

    if (id == -1) {
        perror("fork() error\n");
        exit(ERR_FORK);
    } else if (id == 0) {
        // Ребенок
        while (*sig == CHILD_BLOCK) {}
        
        size_t size = m_file[0];
        int divident = m_file[1];
        int divisor = 1;

        // Читаем числа
        for (int i = 2; i < size + 1; ++i) {
            divisor *= m_file[i];
        }

        if (divisor == 0) {
            exit(ERR_DIV_0);
        }

        int res = divident / divisor;
        int length = get_int_length(res);
        char s[length+1];
        int_to_string(s, res);

        int fp = open(path, O_CREAT | O_WRONLY); // создаем, если не сущ., только для записи
        if (fp == -1) { // Если не получилось открыть файл
            free(path);
            close(fp);
            exit(ERR_OPEN);
        }
        write_to_fd(fp, s, sizeof(char) * length);
        close(fp);
        *sig = CHILD_BLOCK;
        exit(0);
    } else {
        // Родитель, id = child_id
        char** strs = NULL;
        int size = 0;

        printf("[%d] Reading...\n", getpid()); fflush(stdout);
        read_strings(0, &strs, &size); // Читаем числа до \n в массив char* и переводим в массив int'ов

        if (size*sizeof(int) > PAGESIZE ) {
            perror("size is to big");
            exit(ERR_SIZE);
        }

        int nums[size];
        str_array_to_int(strs, nums, size);
        printf("[%d] Red all values.\n", getpid()); fflush(stdout);
        for (size_t i = 0; i < size; ++i) {
            printf("nums[%zu] = %d\n", i, nums[i]);
        }

        printf("[%d] Writing...\n", getpid()); fflush(stdout);
        m_file[0] = size;
        for (size_t i = 1; i < size + 1; ++i) {
            m_file[i] = nums[i - 1];
        }
        printf("[%d] Wrote all values.\n", getpid());
        for (size_t i = 0; i < size + 1; ++i) {
            printf("m_file[%zu] %d\n", i, m_file[i]);
        }
        free(strs);

        *sig = PAR_BLOCK;
        if (waitpid(id, &status, 0) == -1) {
            perror("wait() error\n");
            exit(ERR_WAIT);
        }

        if (WIFEXITED(status)) { // Если ребенок завершился со статусом завершения
            int exit_code = WEXITSTATUS(status);
            switch (exit_code) {
            case ERR_DIV_0:
                perror("error: you cant divide by zer0\n");
                exit(exit_code);
                break;
            case ERR_OPEN:
                perror("error: can't open file\n");
                exit(exit_code);
                break;
            case ERR_CLOSE:
                perror("error: can't close file\n");
                exit(exit_code);
                break;
            case ERR_WRITE:
                perror("error: can't write to file\n");
                exit(exit_code);
                break;
            case ERR_READ:
                perror("error: can't read from file\n");
                exit(exit_code);
                break;
            case ERR_REALLOC:
                perror("error: realloc error\n");
                exit(exit_code);
                break;
            
            default:
                printf("[%d] Child ended with status: %d\n", getpid(), status); fflush(stdout);
                break;
            }
        } else {
            perror("error: child exit with no status\n");
            exit(ERR_NO_STATUS);
        }
    }
    free(path);
    if (munmap(m_file, PAGESIZE) == -1) {
        perror("munmap error");
        return ERR_MMAP;
    }
    return 0;
}
