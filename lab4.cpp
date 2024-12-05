#include <iostream>
#include <windows.h>
#include <vector>
#include <random>
#include <chrono>
#include <thread>

class SharedMemory {
private:
    HANDLE hMapFile;
    int* pMemory;     
    int blockSize;
    int successfulReads = 0;
    int unsuccessfulReads = 0;
    int successfulWrites = 0;
    int unsuccessfulWrites = 0;
    int blockCount;

public:
    SharedMemory(int totalSize, int blockSize) : blockSize(blockSize) {
        blockCount = totalSize / blockSize;

        // Создаем объект общей памяти
        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,   
            NULL,                   
            PAGE_READWRITE,        
            0,                     
            totalSize,         
            TEXT("SharedMemory"));  

        if (hMapFile == NULL) {
            std::cerr << "Не удалось создать объект общей памяти." << std::endl;
            exit(1);
        }

        pMemory = static_cast<int*>(MapViewOfFile(
            hMapFile,              
            FILE_MAP_ALL_ACCESS,   
            0,                      
            0,                      
            totalSize));           

        if (pMemory == NULL) {
            std::cerr << "Не удалось отобразить общую память." << std::endl;
            CloseHandle(hMapFile);
            exit(1);
        }
    }

    ~SharedMemory() {
        UnmapViewOfFile(pMemory);
        CloseHandle(hMapFile);
    }

    int getBlockCount() const {
        return blockCount;
    }

    void readBlock(int blockIndex, int readerId, HANDLE coutMutex) {
        WaitForSingleObject(coutMutex, INFINITE);
        std::cout << "Читатель " << readerId << " читает блок " << blockIndex << ": ";
        bool blockEmpty = true;
        for (int i = blockIndex * blockSize; i < (blockIndex + 1) * blockSize; ++i) {
            std::cout << pMemory[i] << " ";
            if (pMemory[i] != 0) {
                blockEmpty = false;
            }
        }
        std::cout << std::endl;

        if (blockEmpty) {
            unsuccessfulReads++;
        }
        else {
            successfulReads++;
        }

        ReleaseMutex(coutMutex);  
    }

    void writeBlock(int blockIndex, int writerId, HANDLE coutMutex) {
        WaitForSingleObject(coutMutex, INFINITE); 
        int value = rand() % 1000;
        std::cout << "Писатель " << writerId << " записывает в блок " << blockIndex << ": " << value << std::endl;

        for (int i = blockIndex * blockSize; i < (blockIndex + 1) * blockSize; ++i) {
            pMemory[i] = value;
        }

        successfulWrites++;
        ReleaseMutex(coutMutex);  
    }

    void printStatistics() {
        std::cout << "Статистика работы:" << std::endl;
        std::cout << "Успешных чтений: " << successfulReads << std::endl;
        std::cout << "Неуспешных чтений: " << unsuccessfulReads << std::endl;
        std::cout << "Успешных записей: " << successfulWrites << std::endl;
        std::cout << "Неуспешных записей: " << unsuccessfulWrites << std::endl;
    }
};

DWORD WINAPI readerTask(LPVOID lpParam) {
    auto params = static_cast<std::tuple<SharedMemory*, int, HANDLE>*>(lpParam);
    SharedMemory* sharedMemory = std::get<0>(*params);
    int readerId = std::get<1>(*params);
    HANDLE coutMutex = std::get<2>(*params);

    int blockCount = sharedMemory->getBlockCount();
    for (int i = 0; i < 5; ++i) {
        int blockIndex = rand() % blockCount;
        sharedMemory->readBlock(blockIndex, readerId, coutMutex);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
    }

    return 0;
}

DWORD WINAPI writerTask(LPVOID lpParam) {
    auto params = static_cast<std::tuple<SharedMemory*, int, HANDLE>*>(lpParam);
    SharedMemory* sharedMemory = std::get<0>(*params);
    int writerId = std::get<1>(*params);
    HANDLE coutMutex = std::get<2>(*params);

    int blockCount = sharedMemory->getBlockCount();
    for (int i = 0; i < 5; ++i) {
        int blockIndex = rand() % blockCount;
        sharedMemory->writeBlock(blockIndex, writerId, coutMutex);
        std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
    }

    return 0;
}

int main() {
    int memorySize = 20;
    int blockSize = 5;

    int readersCount = 3;
    int writersCount = 2;

    HANDLE coutMutex = CreateMutex(NULL, FALSE, NULL);
    if (coutMutex == NULL) {
        std::cerr << "Ошибка создания мьютекса для вывода" << std::endl;
        return 1;
    }

    SharedMemory sharedMemory(memorySize, blockSize);

    std::vector<HANDLE> readers;
    std::vector<HANDLE> writers;

    for (int i = 0; i < readersCount; ++i) {
        auto params = new std::tuple<SharedMemory*, int, HANDLE>(&sharedMemory, i + 1, coutMutex);
        HANDLE reader = CreateThread(NULL, 0, readerTask, params, 0, NULL);
        readers.push_back(reader);
    }

    for (int i = 0; i < writersCount; ++i) {
        auto params = new std::tuple<SharedMemory*, int, HANDLE>(&sharedMemory, i + 1, coutMutex);
        HANDLE writer = CreateThread(NULL, 0, writerTask, params, 0, NULL);
        writers.push_back(writer);
    }

    for (HANDLE reader : readers) {
        WaitForSingleObject(reader, INFINITE);
        CloseHandle(reader);
    }

    for (HANDLE writer : writers) {
        WaitForSingleObject(writer, INFINITE);
        CloseHandle(writer);
    }

    CloseHandle(coutMutex);

    sharedMemory.printStatistics();

    return 0;
}
