#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define INPUT_HOLDER_SIZE 512

#define STATUS_SUCCESS 0
#define STATUS_FAIL -1
#define STATUS_FAIL_LSEEK (off_t)-1
#define STATUS_TIMEOUT -2
#define STATUS_NO_NUMCONV -3
#define MAX_STRING_LENGTH 1000
#define OWNER_READ_WRITE 0600
#define TIMEOUT_SEC 5
#define TIMEOUT_USEC 0
#define STDIN 0
#define STDOUT 1
#define STOPNUMBER 0
#define MAX_FILEDESC_NUMBER 1
#define NOTSTOPNUMBER 1

extern int errno;

int lastWorkWithData(int fileDescriptorIn){
    int closeStatus = close(fileDescriptorIn);
    if(closeStatus == STATUS_FAIL){
        perror("There are also problems with closing file");
        return STATUS_FAIL;
    }
    return STATUS_SUCCESS;
}

// Функция, печающая весь файл (нововведение)
int printAllFile(int fileDescriptorIn){
    char stringHolder[MAX_STRING_LENGTH];
    int readSymbols;
    int writeSymbols;

    int status = lseek(fileDescriptorIn, 0, SEEK_SET);
    if(status == STATUS_FAIL_LSEEK) {
        perror("printAllFile. There are problems while printing string by number, exactly with setting position in file");
        return STATUS_FAIL;
    }

    do {
        readSymbols = read(fileDescriptorIn, stringHolder, MAX_STRING_LENGTH);
        if(readSymbols == STATUS_FAIL){
            perror("printAllFile. Problems with reading file");
            return STATUS_FAIL;
        }
        writeSymbols = write(STDOUT, stringHolder, readSymbols);
        if(writeSymbols == STATUS_FAIL){
            perror("printAllFile. Problems with writing data in file");
            return STATUS_FAIL;
        }
    } while(readSymbols != 0);
    printf("\n");

    return STATUS_SUCCESS;
}

long getStringNumber(int stringsCount, int fileDescriptorIn){
    fd_set rfds;
    struct timeval tv;
    int selectStatus;

    // Сбрасываем биты для множества файловых дескрипторов, которые нужны для чтения (нововведение)
    FD_ZERO(&rfds);

    // Устанавливаем бит для стандартного потока ввода во множестве файловых дескрипторов, которые нужны для чтения (нововведение)
    FD_SET(STDIN, &rfds);

    // Устанавливаем допустимое время ожидания изменения статуса наборов дескрипторов (нововведение)
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;

    char *endptr = NULL;

    printf("There are %d strings.\nEnter number of line, which you want to see (You have 5 seconds): ", stringsCount);
    int status = fflush(stdout);
    if(status != STATUS_SUCCESS){
        perror("getStringNumber. There are problems while getting your number, exactly with fflush command");
        return STATUS_FAIL;
    }

    // Используем функцию select, для того, чтобы отслеживать статус указанных дескрипторов для чтения
    selectStatus = select(MAX_FILEDESC_NUMBER, &rfds, NULL, NULL, &tv);
    if(selectStatus == 0){
        fprintf(stderr, "getStringNumber. Time is over!\n");
        printAllFile(fileDescriptorIn);
        return STATUS_TIMEOUT;
    }

    char numberHolder[INPUT_HOLDER_SIZE];

    int readSymbols = read(STDIN, numberHolder, INPUT_HOLDER_SIZE);
    if(readSymbols == STATUS_FAIL){
        perror("getStringNumber. There are problems while getting your number, exactly with reading file");
        return STATUS_FAIL;
    }

    long stringNumber = strtol(numberHolder, &endptr, 10);
    if( (stringNumber == LLONG_MAX || stringNumber == LLONG_MIN) && errno == ERANGE){
        perror("getStringNumber. There are problems while getting your number, exactly with converting string to long");
        return STATUS_FAIL;
    }

    if(numberHolder == endptr || *endptr != '\n'){
        return STATUS_NO_NUMCONV;
    }

    return stringNumber;
}

int fillTable(long* offsetsFileTable, long* stringsLengthsFileTable, int fileDescriptorIn){

    char inputHolder[INPUT_HOLDER_SIZE];

    size_t indexInInputHolder = 0;
    size_t currentStringLength = 0;
    size_t indexInTable = 0;
    int currentPosition = 0;

    int readSymbols = read(fileDescriptorIn, inputHolder, INPUT_HOLDER_SIZE);

    if( readSymbols == STATUS_FAIL){
        perror("fillTable. There are problems while filling table, exactly with reading file");
        return STATUS_FAIL;
    }

    while (readSymbols > 0){
        while(indexInInputHolder < readSymbols){
            currentStringLength++;
            if(inputHolder[indexInInputHolder] == '\n'){

                offsetsFileTable[indexInTable] = currentPosition + 1 - currentStringLength;
                stringsLengthsFileTable[indexInTable++] = currentStringLength;

                currentStringLength = 0;
            }
            indexInInputHolder++;
            currentPosition++;
        }
        readSymbols = read(fileDescriptorIn, inputHolder, INPUT_HOLDER_SIZE);
        indexInInputHolder = 0;
        if( readSymbols == STATUS_FAIL){
            perror("fillTable. There are problems while filling table, exactly with reading file");
            return STATUS_FAIL;
        }
    }

    return indexInTable;
}

int printStringByNumber(int fileDescriptorIn, long* offsetFileTable, const long* stringsLengthsFileTable, int stringsCount) {
    long currentBufferSize = INPUT_HOLDER_SIZE;
    int readSymbols;
    int lseekStatus;
    long stringNumber;
    int writeStatus;

    do {
        // Получаем число строки, которую хочет видеть пользователь
        stringNumber = getStringNumber(stringsCount, fileDescriptorIn);

        if (stringNumber < 0 || stringNumber > stringsCount) {
            if (stringNumber == STATUS_FAIL) {
                return STATUS_FAIL;
            } else if (stringNumber == STATUS_TIMEOUT) {
                // Если время вышло, то завершаем печать строк по их номеру
                return STATUS_SUCCESS;
            }
            fprintf(stderr, "printStringByNumber. Invalid string number! Try again.\n");
            continue;
        }

        if (stringNumber == 0) {
            break;
        }

        lseekStatus = lseek(fileDescriptorIn, offsetFileTable[stringNumber - 1], SEEK_SET);
        if (lseekStatus == STATUS_FAIL_LSEEK) {
            perror("printStringByNumber. There are problems while printing string by number, exactly with setting position in file");
            return STATUS_FAIL;
        }

        currentBufferSize = stringsLengthsFileTable[stringNumber - 1];
        char stringHolder[currentBufferSize - 1]; // without \n

        readSymbols = read(fileDescriptorIn, stringHolder, currentBufferSize - 1);
        if (readSymbols == STATUS_FAIL) {
            perror("printStringByNumber. String number is invalid");
            return STATUS_FAIL;
        }

        writeStatus = write(STDOUT, stringHolder, currentBufferSize - 1);
        if(writeStatus == STATUS_FAIL){
            perror("printStringByNumber. There are problems with writing data");
            return STATUS_FAIL;
        }
        printf("\n");

    } while (NOTSTOPNUMBER);

    printf("Stop number!\n");
    return STATUS_SUCCESS;
}

int main(int argc, char* argv[]){

    int fileDescriptorIn;

    long offsetsFileTable[256];
    long stringsLengthsFileTable[256];
    int status;

    if(argc < 2){
        fprintf(stderr, "Not enough arguments entered.\nusage: progname input_file\n");
        exit(EXIT_FAILURE);
    }

    fileDescriptorIn = open(argv[1], O_RDONLY, OWNER_READ_WRITE);
    if(fileDescriptorIn == STATUS_FAIL){
        perror("There are problems while reading file");
        exit(EXIT_FAILURE);
    }

    int stringsCount = fillTable(offsetsFileTable, stringsLengthsFileTable, fileDescriptorIn);
    if(stringsCount == STATUS_FAIL){
        lastWorkWithData(fileDescriptorIn);
        return EXIT_FAILURE;
    }

    status = printStringByNumber(fileDescriptorIn, offsetsFileTable, stringsLengthsFileTable, stringsCount);
    if(status == STATUS_FAIL) {
        lastWorkWithData(fileDescriptorIn);
        return EXIT_FAILURE;
    }

    status = lastWorkWithData(fileDescriptorIn);
    if(status == STATUS_FAIL){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}