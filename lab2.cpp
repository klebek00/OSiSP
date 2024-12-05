#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>


struct AsyncIOData {
    HANDLE hFile;
    char* buffer;
    DWORD bufferSize;
    OVERLAPPED overlapped;
    char insertChar;
    DWORD insertPosition;
};

std::chrono::high_resolution_clock::time_point asyncStart, asyncEnd;

void CALLBACK AsyncReadComplete(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped) {
    AsyncIOData* ioData = reinterpret_cast<AsyncIOData*>(lpOverlapped);

    if (dwErrorCode != 0) {
        std::cerr << "Ошибка чтения файла: " << dwErrorCode << std::endl;
        return;
    }

    DWORD bytesRead = dwNumberOfBytesTransferred;

    for (DWORD i = 0; i < bytesRead; ++i) {
        if (i == ioData->insertPosition) {
            ioData->buffer[i] = ioData->insertChar;
        }
    }

    DWORD bytesWritten;
    if (!WriteFile(ioData->hFile, ioData->buffer, bytesRead, &bytesWritten, &ioData->overlapped)) {
        std::cerr << "Ошибка записи в файл." << std::endl;
        return;
    }

    asyncEnd = std::chrono::high_resolution_clock::now(); 
    std::chrono::duration<double> elapsed = asyncEnd - asyncStart;
    std::cout << "Время обработки: " << elapsed.count() << " секунд." << std::endl;
}

void AsyncFileProcessing(const char* inputFilename, const char* outputFilename, char insertChar, DWORD insertPosition) {
    HANDLE hFile = CreateFileA(inputFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Ошибка открытия файла." << std::endl;
        return;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        std::cerr << "Ошибка получения размера файла." << std::endl;
        CloseHandle(hFile);
        return;
    }

    char* buffer = new char[fileSize];
    AsyncIOData ioData = {};
    ioData.hFile = hFile;
    ioData.buffer = buffer;
    ioData.bufferSize = fileSize;
    ioData.insertChar = insertChar;
    ioData.insertPosition = insertPosition;

    ZeroMemory(&ioData.overlapped, sizeof(ioData.overlapped));
    ioData.overlapped.Offset = 0;  

    asyncStart = std::chrono::high_resolution_clock::now(); 

    if (!ReadFileEx(hFile, buffer, fileSize, &ioData.overlapped, AsyncReadComplete)) {
        std::cerr << "Ошибка при чтении файла асинхронно." << std::endl;
        delete[] buffer;
        CloseHandle(hFile);
        return;
    }

    SleepEx(1000, TRUE); 

    CloseHandle(hFile);
    delete[] buffer;
}

struct ThreadData {
    HANDLE hFile;      
    DWORD startPos;     
    DWORD endPos;       
    char* buffer;       
    char insertChar;    
    DWORD insertPosition; 
};

DWORD WINAPI ThreadProc(LPVOID param) {
    ThreadData* threadData = static_cast<ThreadData*>(param);

    DWORD bytesRead;
    SetFilePointer(threadData->hFile, threadData->startPos, NULL, FILE_BEGIN);  
    ReadFile(threadData->hFile, threadData->buffer, threadData->endPos - threadData->startPos, &bytesRead, NULL);

    for (DWORD i = 0; i < bytesRead; ++i) {
        if (threadData->startPos + i == threadData->insertPosition) {
            threadData->buffer[i] = threadData->insertChar;
        }
    }

    return 0;
}

void MultiThreadedFileProcessing(const char* inputFilename, const char* outputFilename, char insertChar, DWORD insertPosition, int numThreads) {
    HANDLE hFile = CreateFileA(inputFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Ошибка открытия файла для чтения." << std::endl;
        return;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        std::cerr << "Ошибка получения размера файла." << std::endl;
        CloseHandle(hFile);
        return;
    }

    DWORD partSize = fileSize / numThreads;  

    HANDLE* threadHandles = static_cast<HANDLE*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, numThreads * sizeof(HANDLE)));
    ThreadData* threadDataArray = static_cast<ThreadData*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, numThreads * sizeof(ThreadData)));

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Многопоточная обработка" << std::endl;

    for (int i = 0; i < numThreads; ++i) {
        DWORD startPos = i * partSize;
        DWORD endPos = (i == numThreads - 1) ? fileSize : startPos + partSize;

        char* buffer = static_cast<char*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, endPos - startPos));

        threadDataArray[i] = { hFile, startPos, endPos, buffer, insertChar, insertPosition };

        threadHandles[i] = CreateThread(NULL, 0, ThreadProc, &threadDataArray[i], 0, NULL);
        if (threadHandles[i] == NULL) {
            std::cerr << "Ошибка создания потока." << std::endl;
            HeapFree(GetProcessHeap(), 0, buffer);
            CloseHandle(hFile);
            return;
        }
    }

    WaitForMultipleObjects(numThreads, threadHandles, TRUE, INFINITE);

    HANDLE hOutputFile = CreateFileA(outputFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutputFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Ошибка открытия выходного файла." << std::endl;
        for (int i = 0; i < numThreads; ++i) {
            HeapFree(GetProcessHeap(), 0, threadDataArray[i].buffer);
        }
        CloseHandle(hFile);
        return;
    }

    DWORD bytesWritten;
    for (int i = 0; i < numThreads; ++i) {
        DWORD partSize = threadDataArray[i].endPos - threadDataArray[i].startPos;
        WriteFile(hOutputFile, threadDataArray[i].buffer, partSize, &bytesWritten, NULL);

        HeapFree(GetProcessHeap(), 0, threadDataArray[i].buffer);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Время многопоточной обработки: " << elapsed.count() << " секунд." << std::endl;

    CloseHandle(hOutputFile);
    CloseHandle(hFile);
    HeapFree(GetProcessHeap(), 0, threadDataArray);
    HeapFree(GetProcessHeap(), 0, threadHandles);
}


void InsertCharacter(char* buffer, DWORD& bytesRead, char insertChar, DWORD position, DWORD bufferSize, char*& newBuffer, DWORD& newBufferSize) {

    newBufferSize = bytesRead + 1;
    newBuffer = new char[newBufferSize];
    memcpy(newBuffer, buffer, position);

    newBuffer[position] = insertChar;
    memcpy(newBuffer + position + 1, buffer + position, bytesRead - position);

    bytesRead = newBufferSize;
}

void CopyFileData(const char* sourceFilename, const char* destFilename, char insertChar, DWORD insertPosition) {

    HANDLE hSourceFile = CreateFileA(sourceFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    HANDLE hDestFile = CreateFileA(destFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    const DWORD bufferSize = 4096;
    char buffer[bufferSize];
    DWORD bytesRead, bytesWritten;
    DWORD totalBytesRead = 0;
    char* newBuffer = nullptr;
    DWORD newBufferSize = 0;

    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Синхронный метод" << std::endl;

    while (ReadFile(hSourceFile, buffer, bufferSize, &bytesRead, NULL) && bytesRead > 0) {

        if (totalBytesRead <= insertPosition && insertPosition < totalBytesRead + bytesRead) {
            DWORD localInsertPos = insertPosition - totalBytesRead;
            InsertCharacter(buffer, bytesRead, insertChar, localInsertPos, bufferSize, newBuffer, newBufferSize);
        }
        else {
            newBuffer = buffer;
            newBufferSize = bytesRead;
        }

        totalBytesRead += bytesRead;

        if (!WriteFile(hDestFile, newBuffer, newBufferSize, &bytesWritten, NULL) || bytesWritten != newBufferSize) {
            std::cerr << "Ошибка записи в файл: " << GetLastError() << std::endl;
            CloseHandle(hSourceFile);
            CloseHandle(hDestFile);
            return;
        }
        if (newBuffer != buffer) {
            delete[] newBuffer;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Время обработки: " << elapsed.count() << " секунд." << std::endl;

    CloseHandle(hSourceFile);
    CloseHandle(hDestFile);
}

int main() {
    setlocale(LC_ALL, "RUS");

    const char* filename = "file.txt";
    const char* file = "file1.txt";
    const char* outputFilename = "outputheard.txt";
    char insertChar = 'X';
    DWORD insertPosition = 5;
    int numThreads = 4;

    AsyncFileProcessing(filename, outputFilename, insertChar, insertPosition);
    MultiThreadedFileProcessing(file, outputFilename, insertChar, insertPosition, numThreads);
    CopyFileData(filename, "destFilename.txt", insertChar, insertPosition);


    return 0;
}
