#include <iostream>
#include <windows.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <pdh.h>
#include <pdhmsg.h>

using namespace std;

struct Data
{
	int TID;
	bool isComplate;
	vector<int>* part;
};

DWORD WINAPI SortThread(LPVOID param)
{
	Data* data = (Data*)param;

	sort(data->part->begin(), data->part->end());

	data->isComplate = true;

	return 0;
}

double GetCPUUsage()
{
	static FILETIME prevIdleTime = { 0 }, prevKernelTime = { 0 }, prevUserTime = { 0 };
	FILETIME idleTime, kernelTime, userTime;

	if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		cerr << "GetSystemTimes failed" << endl;
		return -1;
	}

	ULONGLONG idleDiff = ((static_cast<ULONGLONG>(idleTime.dwLowDateTime) | (static_cast<ULONGLONG>(idleTime.dwHighDateTime) << 32)) -
		(static_cast<ULONGLONG>(prevIdleTime.dwLowDateTime) | (static_cast<ULONGLONG>(prevIdleTime.dwHighDateTime) << 32)));
	ULONGLONG kernelDiff = ((static_cast<ULONGLONG>(kernelTime.dwLowDateTime) | (static_cast<ULONGLONG>(kernelTime.dwHighDateTime) << 32)) -
		(static_cast<ULONGLONG>(prevKernelTime.dwLowDateTime) | (static_cast<ULONGLONG>(prevKernelTime.dwHighDateTime) << 32)));
	ULONGLONG userDiff = ((static_cast<ULONGLONG>(userTime.dwLowDateTime) | (static_cast<ULONGLONG>(userTime.dwHighDateTime) << 32)) -
		(static_cast<ULONGLONG>(prevUserTime.dwLowDateTime) | (static_cast<ULONGLONG>(prevUserTime.dwHighDateTime) << 32)));

	double cpuUsage = 100.0 * (static_cast<double>(kernelDiff + userDiff - idleDiff) / (kernelDiff + userDiff));

	prevIdleTime = idleTime;
	prevKernelTime = kernelTime;
	prevUserTime = userTime;

	return cpuUsage;
}


void DisplayCPUUsage() {
	double cpuUsage = GetCPUUsage();
	if (cpuUsage >= 0) 
	{
		cout << "Current CPU usage: " << cpuUsage << "%" << endl;
	}
	else 
	{
		cout << "Failed to get CPU usage." << endl;
	}
}



int main()
{
	int size;
	int count;

	cout << "Enter arr size: ";
	cin >> size;
	cout << "Enter number of threads: ";
	cin >> count;

	vector<int> arr(size);
	iota(arr.begin(), arr.end(), 0);
	random_shuffle(arr.begin(), arr.end());

	int partSize = size / count;
	vector<vector<int>> partsArr(count);

	for (int i = 0; i < count; i++)
	{
		if (i == count - 1) {
			partsArr[i].assign(arr.begin() + i * partSize, arr.end());
		}
		else {
			partsArr[i].assign(arr.begin() + i * partSize, arr.begin() + (i + 1) * partSize);
		}
	}

	vector<HANDLE> threads(count);
	vector<Data> data(count);
	for (int i = 0; i < count; i++)
	{
		data[i].TID = i;
		data[i].isComplate = false;
		data[i].part = &partsArr[i];
	}

	auto start = chrono::high_resolution_clock::now();

	for (int i = 0; i < count; i++)
	{
		threads[i] = CreateThread(NULL, 0, SortThread, &data[i], 0, NULL);
	}

	while (true)
	{
		bool allComplate = true;
		system("cls");


		for (int i = 0; i < count; i++)
		{
			if (data[i].isComplate)
			{
				cout << "Complated" << endl;
			}
			else
			{
				cout << "In progress" << endl;
			}

			if (!data[i].isComplate)
			{
				allComplate = false;
			}
		}

		DisplayCPUUsage();

		if (allComplate)
			break;

		Sleep(800);
	}

	WaitForMultipleObjects(count, threads.data(), TRUE, INFINITE);

	auto end = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed = end - start;

	for (int i = 0; i < count; i++) {
		CloseHandle(threads[i]);
	}

	vector<int> sortedArray;
	for (const auto& part : partsArr) {
		sortedArray.insert(sortedArray.end(), part.begin(), part.end());
	}
	sort(sortedArray.begin(), sortedArray.end());

	cout << "Array sorted in: " << elapsed.count() << " seconds" << endl;

	return 0;



}

