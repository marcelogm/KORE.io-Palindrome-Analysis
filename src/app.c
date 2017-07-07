#include <kore/kore.h>
#include <kore/http.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#define THREAD_NUMBER 8
#define FILE_DIR "bible.txt"
#define USE_LENGTH_ANALYSIS true
#define MINIMAL_LENGTH 2
#define SPLIT_DELIM " :,;.-\n\t\r#"

struct file_information {
    size_t size;
    char * content;
};
typedef struct file_information f_info;

struct mutex_package {
    int * target;
    pthread_mutex_t * mutex;
};
typedef struct mutex_package m_pack;

struct thread_package {
    size_t id;
    size_t work_size;
    f_info * file;
    m_pack * palindrome;
};
typedef struct thread_package t_pack;

void    mutex_package_init(m_pack * newest, void * target);
void    thread_package_init(t_pack * newest, f_info * file, size_t id, size_t work_size, m_pack * palindrome);
f_info* get_file_content(char * dir);
bool    palindrome(char * start, size_t size);
char*   lower_case(char * input, size_t length);
void*   thread_callback(void * arg);
int		page(struct http_request *);

void 
mutex_package_init(m_pack * newest, 
                    void * target) {
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    newest->mutex = &mutex;
    newest->target = target;
}

void 
thread_package_init(t_pack * newest, 
                    f_info * file, 
                    size_t id, 
                    size_t work_size, 
                    m_pack * palindrome) {
    newest->file = file;
    newest->id = id;
    newest->work_size = work_size;
    newest->palindrome = palindrome;
}

f_info * 
get_file_content(char * dir) {
    FILE * f_ptr = fopen(dir, "r");
    if (f_ptr) {
        fseek(f_ptr, 0, SEEK_END);
        f_info * file = (f_info *) malloc(sizeof (f_info));
        file->size = ftell(f_ptr);
        file->content = (char *) malloc(sizeof (char) * file->size);
        fseek(f_ptr, 0, SEEK_SET);
        fread(file->content, file->size, 1, f_ptr);
        fclose(f_ptr);
        return file;
    }
    return NULL;
}

bool 
palindrome(char * start, size_t size) {
    size_t middle = size / 2;
    char * end = start + (size - 1);
    while (middle > 0) {
        if (*start != *end) {
            return false;
        }
        start++;
        end--;
        middle--;
    }
    return true;
}

char * 
lower_case(char * input, size_t length) {
    size_t i = 0;
    for (i = length; i > 0; i--) {
        if (*(input + i) >= 65 && *(input + i) <= 90) {
            *(input + i) += 32;
        }
    }
    return input;
}

void * 
thread_callback(void * arg) {
    t_pack * param = arg;
    size_t from = param->work_size * param->id;
    char * backup = NULL;
    char * buffer = NULL;
    int acc = 0;
    buffer = __strtok_r(param->file->content + from, SPLIT_DELIM, (char **) &backup);
    while (buffer != NULL) {
        size_t length = strlen(buffer);
        #ifdef USE_LENGTH_ANALYSIS
            if (palindrome(lower_case(buffer, length), length) && length > MINIMAL_LENGTH) {
        #else
            if (palindrome(lower_case(buffer, length), length)) {
        #endif
            acc++;
        }
        buffer = __strtok_r(NULL, SPLIT_DELIM, &backup);
    }
    pthread_mutex_lock(param->palindrome->mutex);
    *param->palindrome->target += acc;
    pthread_mutex_unlock(param->palindrome->mutex);
    pthread_exit(NULL);
}

int page(struct http_request * req) {
    f_info * file = get_file_content(FILE_DIR);
    if(file == NULL) {
        const char * error = "File not found";
        http_response(req, 404, error, strlen(error));
        return (KORE_RESULT_ERROR);
    }
    const size_t work_per_thread = file->size / THREAD_NUMBER;
    size_t i = 0; 
    size_t palindrome = 0;
    m_pack m_palindrome;
    mutex_package_init(&m_palindrome, &palindrome);
    pthread_t * t = (pthread_t *) malloc(sizeof (pthread_t) * THREAD_NUMBER);
    t_pack * per_thread = (t_pack *) malloc(sizeof (t_pack) * THREAD_NUMBER);
    for (i = 0; i < THREAD_NUMBER; i++) {
        thread_package_init(per_thread + i, file, i, work_per_thread, &m_palindrome);
        pthread_create(t + i, NULL, thread_callback, per_thread + i);
    }
    for (i = 0; i < THREAD_NUMBER; i++) {
        pthread_join(*(t + i), NULL);
    }
    char buffer[255];
    sprintf(buffer, "Biblical palindromes: %u \n\nKORE.io multithread test :D \n\nSee https://kore.io \nAlso https://github.com/marcelogm", (unsigned int) palindrome);
    const char * send = strtok(buffer, "\0");
    http_response(req, 200, send, strlen(send));
    return (KORE_RESULT_OK);
}
