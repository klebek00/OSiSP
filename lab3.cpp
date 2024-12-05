#include <windows.h>
#include <iostream>
#include <string>

#define BUFFER_COUNT 3
#define BUFFER_SIZE 256
#define SHARED_MEMORY_NAME L"Local\\SharedMemoryExample"
#define MUTEX_NAME L"Local\\BufferAccessMutex"
#define SEMAPHORE_NAME L"Local\\BufferSemaphore"

using namespace std;

struct SharedBuffer {
    char data[BUFFER_SIZE];
    bool inUse;  
};

struct SharedMemory {
    SharedBuffer buffers[BUFFER_COUNT];
};

void producer(SharedMemory* sharedMemory, HANDLE hSemaphore, HANDLE hMutex) {
    while (true) {
        WaitForSingleObject(hSemaphore, INFINITE);

        WaitForSingleObject(hMutex, INFINITE);  
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            if (!sharedMemory->buffers[i].inUse) {
                cout << "Введите данные для буфера " << i + 1 << ": ";
                string input;
                cin >> input;
                strncpy_s(sharedMemory->buffers[i].data, BUFFER_SIZE, input.c_str(), BUFFER_SIZE - 1);
                sharedMemory->buffers[i].inUse = true; 
                cout << "Производитель: Буфер " << i + 1 << " заполнен.\n";
                break;
            }
        }
        ReleaseMutex(hMutex);  
    }
}

void consumer(SharedMemory* sharedMemory, HANDLE hSemaphore, HANDLE hMutex) {
    while (true) {
        WaitForSingleObject(hSemaphore, INFINITE);

        WaitForSingleObject(hMutex, INFINITE);  
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            if (sharedMemory->buffers[i].inUse) {
                cout << "Потребитель: Чтение данных из буфера " << i + 1 << ": " << sharedMemory->buffers[i].data << endl;
                sharedMemory->buffers[i].inUse = false;  
                break;
            }
        }
        ReleaseMutex(hMutex);  
    }
}

int main() {
    setlocale(LC_ALL, "Russian");

    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedMemory),
        SHARED_MEMORY_NAME
    );

    if (!hMapFile) {
        std::cerr << "Не удалось создать/открыть разделяемую память: " << GetLastError() << "\n";
        return 1;
    }

    SharedMemory* sharedMemory = (SharedMemory*)MapViewOfFile(
        hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory)
    );

    if (!sharedMemory) {
        std::cerr << "Не удалось мапировать разделяемую память: " << GetLastError() << "\n";
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hMutex = CreateMutex(nullptr, FALSE, MUTEX_NAME);
    if (!hMutex) {
        std::cerr << "Не удалось создать/открыть мьютекс: " << GetLastError() << "\n";
        UnmapViewOfFile(sharedMemory);
        CloseHandle(hMapFile);
        return 1;
    }

    HANDLE hSemaphore = CreateSemaphore(nullptr, BUFFER_COUNT, BUFFER_COUNT, SEMAPHORE_NAME);
    if (!hSemaphore) {
        std::cerr << "Не удалось создать/открыть семафор: " << GetLastError() << "\n";
        CloseHandle(hMutex);
        UnmapViewOfFile(sharedMemory);
        CloseHandle(hMapFile);
        return 1;
    }

    WaitForSingleObject(hMutex, INFINITE);
    static bool isInitialized = true;
    if (isInitialized) {
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            sharedMemory->buffers[i].inUse = false;
            memset(sharedMemory->buffers[i].data, 0, BUFFER_SIZE);
        }
        isInitialized = false;  
    }
    ReleaseMutex(hMutex);

    DWORD dwThreadId;
    HANDLE hProducerThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)producer, sharedMemory, 0, &dwThreadId);
    HANDLE hConsumerThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)consumer, sharedMemory, 0, &dwThreadId);

    if (hProducerThread == nullptr || hConsumerThread == nullptr) {
        std::cerr << "Не удалось создать потоки.\n";
        return 1;
    }

    WaitForSingleObject(hProducerThread, INFINITE);
    WaitForSingleObject(hConsumerThread, INFINITE);

    CloseHandle(hProducerThread);
    CloseHandle(hConsumerThread);
    CloseHandle(hSemaphore);
    CloseHandle(hMutex);
    UnmapViewOfFile(sharedMemory);
    CloseHandle(hMapFile);

    return 0;
}
