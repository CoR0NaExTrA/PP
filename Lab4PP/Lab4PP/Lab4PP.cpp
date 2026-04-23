#include <windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <cassert>
#include <sstream>
#include <iomanip>

using namespace std;
using Clock = chrono::high_resolution_clock;
using ms = chrono::duration<double, milli>;

// структура для хранения данных
struct ThreadTimeData {
    double timestamp;    // время от начала работы в мс
    int threadId;        // номер потока
};

// Глобальные переменные для сбора данных
vector<ThreadTimeData> g_threadTimeData;
CRITICAL_SECTION g_csData;

struct Image {
    int width = 0;
    int height = 0;
    vector<uint8_t> data;
    int rowStride = 0;
};

bool ReadBMP24(const char* filename, Image& img) {
    ifstream f(filename, ios::binary);
    if (!f) return false;

    uint16_t bfType;
    f.read(reinterpret_cast<char*>(&bfType), sizeof(bfType));
    if (bfType != 0x4D42) return false;

    uint32_t bfSize; f.read(reinterpret_cast<char*>(&bfSize), 4);
    uint16_t bfReserved1; f.read(reinterpret_cast<char*>(&bfReserved1), 2);
    uint16_t bfReserved2; f.read(reinterpret_cast<char*>(&bfReserved2), 2);
    uint32_t bfOffBits; f.read(reinterpret_cast<char*>(&bfOffBits), 4);

    uint32_t biSize; f.read(reinterpret_cast<char*>(&biSize), 4);
    if (biSize != 40) {
        cerr << "Unsupported BMP DIB header size: " << biSize << "\n";
        return false;
    }
    int32_t biWidth; f.read(reinterpret_cast<char*>(&biWidth), 4);
    int32_t biHeight; f.read(reinterpret_cast<char*>(&biHeight), 4);
    uint16_t biPlanes; f.read(reinterpret_cast<char*>(&biPlanes), 2);
    uint16_t biBitCount; f.read(reinterpret_cast<char*>(&biBitCount), 2);
    uint32_t biCompression; f.read(reinterpret_cast<char*>(&biCompression), 4);
    uint32_t biSizeImage; f.read(reinterpret_cast<char*>(&biSizeImage), 4);
    uint32_t biXPelsPerMeter; f.read(reinterpret_cast<char*>(&biXPelsPerMeter), 4);
    uint32_t biYPelsPerMeter; f.read(reinterpret_cast<char*>(&biYPelsPerMeter), 4);
    uint32_t biClrUsed; f.read(reinterpret_cast<char*>(&biClrUsed), 4);
    uint32_t biClrImportant; f.read(reinterpret_cast<char*>(&biClrImportant), 4);

    if (biCompression != 0) {
        cerr << "Only uncompressed BMP supported\n";
        return false;
    }
    if (biBitCount != 24) {
        cerr << "Only 24-bit BMP supported\n";
        return false;
    }

    int width = biWidth;
    int height = abs(biHeight);

    int rowStride = ((width * 3 + 3) / 4) * 4;

    img.width = width;
    img.height = height;
    img.rowStride = rowStride;
    img.data.resize((size_t)rowStride * height);

    f.seekg(bfOffBits, ios::beg);
    bool bottomUp = (biHeight > 0);
    for (int row = 0; row < height; ++row) {
        int targetRow = bottomUp ? (height - 1 - row) : row;
        f.read(reinterpret_cast<char*>(&img.data[targetRow * rowStride]), rowStride);
    }

    return true;
}

bool WriteBMP24(const char* filename, const Image& img) {
    ofstream f(filename, ios::binary);
    if (!f) return false;

    int width = img.width;
    int height = img.height;
    int rowStride = img.rowStride;
    int pixelArraySize = rowStride * height;
    int fileHeaderSize = 14;
    int dibHeaderSize = 40;
    int bfOffBits = fileHeaderSize + dibHeaderSize;
    int bfSize = bfOffBits + pixelArraySize;

    uint16_t bfType = 0x4D42; f.write(reinterpret_cast<char*>(&bfType), 2);
    f.write(reinterpret_cast<char*>(&bfSize), 4);
    uint16_t bfReserved1 = 0; f.write(reinterpret_cast<char*>(&bfReserved1), 2);
    uint16_t bfReserved2 = 0; f.write(reinterpret_cast<char*>(&bfReserved2), 2);
    f.write(reinterpret_cast<char*>(&bfOffBits), 4);

    uint32_t biSize = dibHeaderSize; f.write(reinterpret_cast<char*>(&biSize), 4);
    int32_t biWidth = width; f.write(reinterpret_cast<char*>(&biWidth), 4);
    int32_t biHeight = height;
    f.write(reinterpret_cast<char*>(&biHeight), 4);
    uint16_t biPlanes = 1; f.write(reinterpret_cast<char*>(&biPlanes), 2);
    uint16_t biBitCount = 24; f.write(reinterpret_cast<char*>(&biBitCount), 2);
    uint32_t biCompression = 0; f.write(reinterpret_cast<char*>(&biCompression), 4);
    uint32_t biSizeImage = pixelArraySize; f.write(reinterpret_cast<char*>(&biSizeImage), 4);
    uint32_t biXPelsPerMeter = 0x0B13; f.write(reinterpret_cast<char*>(&biXPelsPerMeter), 4);
    uint32_t biYPelsPerMeter = 0x0B13; f.write(reinterpret_cast<char*>(&biYPelsPerMeter), 4);
    uint32_t biClrUsed = 0; f.write(reinterpret_cast<char*>(&biClrUsed), 4);
    uint32_t biClrImportant = 0; f.write(reinterpret_cast<char*>(&biClrImportant), 4);

    for (int row = 0; row < height; ++row) {
        int srcRow = height - 1 - row;
        f.write(reinterpret_cast<const char*>(&img.data[srcRow * rowStride]), rowStride);
    }

    return true;
}

inline void GetPixelBGR(const Image& img, int x, int y, uint8_t& b, uint8_t& g, uint8_t& r) {
    int rs = img.rowStride;
    const uint8_t* p = &img.data[y * rs + x * 3];
    b = p[0]; g = p[1]; r = p[2];
}

inline void SetPixelBGR(Image& img, int x, int y, uint8_t b, uint8_t g, uint8_t r) {
    int rs = img.rowStride;
    uint8_t* p = &img.data[y * rs + x * 3];
    p[0] = b; p[1] = g; p[2] = r;
}

// Функция для записи времени работы потока
void RecordThreadTime(int threadId, const chrono::time_point<Clock>& startTime) {
    auto currentTime = Clock::now();
    double timestamp = ms(currentTime - startTime).count();

    ThreadTimeData data;
    data.timestamp = timestamp;
    data.threadId = threadId;

    EnterCriticalSection(&g_csData);
    g_threadTimeData.push_back(data);
    LeaveCriticalSection(&g_csData);
}

// Модифицированная функция размытия с периодической записью времени
void BlurPass3x3(const Image& src, Image& dst, int y0, int y1, int x0, int x1,
    int threadId, const chrono::time_point<Clock>& startTime) {
    int totalPixels = (y1 - y0) * (x1 - x0);
    int reportInterval = max(1, totalPixels / 100); // Записываем примерно 100 раз за тайл

    int pixelCounter = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int sumB = 0, sumG = 0, sumR = 0, count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                int yy = y + dy;
                if (yy < 0 || yy >= src.height) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx;
                    if (xx < 0 || xx >= src.width) continue;
                    uint8_t b, g, r;
                    GetPixelBGR(src, xx, yy, b, g, r);
                    sumB += b; sumG += g; sumR += r;
                    ++count;
                }
            }
            uint8_t nb = (uint8_t)(sumB / count);
            uint8_t ng = (uint8_t)(sumG / count);
            uint8_t nr = (uint8_t)(sumR / count);
            SetPixelBGR(dst, x, y, nb, ng, nr);

            // Периодическая запись времени
            if (pixelCounter % reportInterval == 0) {
                RecordThreadTime(threadId, startTime);
            }
            pixelCounter++;
        }
    }

    // Запись конечного времени
    RecordThreadTime(threadId, startTime);
}

void RunSequential(const Image& in, Image& out, int iterations) {
    Image a = in;
    Image b = in;
    b.data = a.data;
    auto startTime = Clock::now();
    for (int it = 0; it < iterations; ++it) {
        BlurPass3x3(a, b, 0, a.height, 0, a.width, 0, startTime);
        a.data.swap(b.data);
    }
    out = a;
}

struct Tile { int x0, x1, y0, y1; };

// Модифицированная функция рабочего потока
void ThreadWorkerOnce(const Image& src, Image& dst, const vector<Tile>& tiles,
    int threadId, const chrono::time_point<Clock>& startTime) {
    for (const Tile& t : tiles) {
        BlurPass3x3(src, dst, t.y0, t.y1, t.x0, t.x1, threadId, startTime);
    }
}

vector<vector<Tile>> BuildAndDistributeTiles(int width, int height, int Nthreads) {
    int N = Nthreads;
    vector<Tile> tiles;
    tiles.reserve(N * N);
    int tileW = width / N;
    int tileH = height / N;
    int restW = width % N;
    int restH = height % N;

    vector<int> xStarts(N + 1, 0), yStarts(N + 1, 0);
    int cur = 0;
    for (int i = 0; i < N; ++i) {
        xStarts[i] = cur;
        int add = (i < restW) ? 1 : 0;
        cur += tileW + add;
    }
    xStarts[N] = width;
    cur = 0;
    for (int j = 0; j < N; ++j) {
        yStarts[j] = cur;
        int add = (j < restH) ? 1 : 0;
        cur += tileH + add;
    }
    yStarts[N] = height;

    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            int x0 = xStarts[i];
            int x1 = xStarts[i + 1];
            int y0 = yStarts[j];
            int y1 = yStarts[j + 1];
            tiles.push_back({ x0,x1,y0,y1 });
        }
    }

    vector<vector<Tile>> assignment(N);
    for (int t = 0; t < N; ++t) {
        for (int k = 0; k < N; ++k) {
            int idx = t * N + k;
            if (idx < (int)tiles.size())
                assignment[t].push_back(tiles[idx]);
        }
    }
    return assignment;
}

struct ThreadParams {
    const Image* src;
    Image* dst;
    const vector<Tile>* tiles;
    int threadId;
    chrono::time_point<Clock> startTime;
    int priority;
};

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadParams* p = reinterpret_cast<ThreadParams*>(lpParam);

    // Установка приоритета потока
    switch (p->priority) {
    case 1: SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); break;
    case 0: SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL); break;
    case -1: SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL); break;
    default: SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }

    ThreadWorkerOnce(*p->src, *p->dst, *p->tiles, p->threadId, p->startTime);
    return 0;
}

// Функция для сохранения данных в CSV файл (только номер потока и время)
void SaveThreadTimeDataToCSV(const string& filename) {
    ofstream f(filename);
    if (!f) {
        cerr << "Failed to create CSV file: " << filename << "\n";
        return;
    }

    f << "ThreadID,Time(ms)\n";
    for (const auto& data : g_threadTimeData) {
        f << data.threadId << "," << fixed << setprecision(3) << data.timestamp << "\n";
    }
    f.close();
    cout << "Thread time data saved to: " << filename << "\n";
}

void PrintHelp() {
    cout << "Usage: scheduler_lab.exe input.bmp output.bmp [options]\n";
    cout << "Options:\n";
    cout << "  /threads:N      Number of threads (default: 1)\n";
    cout << "  /cores:N        Number of cores to use (default: 1)\n";
    cout << "  /priorities:P1,P2,... Thread priorities (0=normal, 1=above_normal, -1=below_normal)\n";
    cout << "  /iterations:N   Number of blur iterations (default: 10)\n";
    cout << "  /datafile:FILE  Output CSV file for thread time data (default: thread_times.csv)\n";
    cout << "  /?             Show this help\n";
    cout << "\nExample:\n";
    cout << "  scheduler_lab.exe in.bmp out.bmp /threads:3 /cores:1 /priorities:1,0,-1\n";
}

int main(int argc, char** argv) {
    // Инициализация критической секции
    InitializeCriticalSection(&g_csData);

    // Параметры по умолчанию
    int threads = 1;
    int cores = 1;
    vector<int> priorities = { 0 }; // normal по умолчанию
    int iterations = 7;
    string dataFile = "thread_times.csv";
    string inFile, outFile;

    // Разбор командной строки
    if (argc < 3) {
        PrintHelp();
        return 1;
    }

    inFile = argv[1];
    outFile = argv[2];

    for (int i = 3; i < argc; i++) {
        string arg = argv[i];
        if (arg == "/?") {
            PrintHelp();
            return 0;
        }
        else if (arg.find("/threads:") == 0) {
            threads = stoi(arg.substr(9));
        }
        else if (arg.find("/cores:") == 0) {
            cores = stoi(arg.substr(7));
        }
        else if (arg.find("/priorities:") == 0) {
            priorities.clear();
            string prioStr = arg.substr(12);
            stringstream ss(prioStr);
            string token;
            while (getline(ss, token, ',')) {
                priorities.push_back(stoi(token));
            }
        }
        else if (arg.find("/iterations:") == 0) {
            iterations = stoi(arg.substr(12));
        }
        else if (arg.find("/datafile:") == 0) {
            dataFile = arg.substr(10);
        }
    }

    // Проверка параметров
    if (threads < 1 || threads > 16) {
        cerr << "threads must be in [1,16]\n";
        return 1;
    }
    if (cores < 1 || cores > 64) {
        cerr << "cores must be in [1,64]\n";
        return 1;
    }
    if (priorities.size() < threads) {
        int lastPrio = priorities.empty() ? 0 : priorities.back();
        while (priorities.size() < threads) {
            priorities.push_back(lastPrio);
        }
    }

    cout << "Parameters:\n";
    cout << "  Input file: " << inFile << "\n";
    cout << "  Output file: " << outFile << "\n";
    cout << "  Threads: " << threads << "\n";
    cout << "  Cores: " << cores << "\n";
    cout << "  Iterations: " << iterations << "\n";
    cout << "  Priorities: ";
    for (int i = 0; i < threads; i++) {
        cout << priorities[i] << (i < threads - 1 ? "," : "");
    }
    cout << "\n";
    cout << "  Data file: " << dataFile << "\n";

    // Установка маски affinity
    DWORD_PTR mask = ((1ULL << cores) - 1ULL);
    HANDLE hProc = GetCurrentProcess();
    BOOL affOk = SetProcessAffinityMask(hProc, mask);
    if (!affOk) {
        cerr << "Warning: SetProcessAffinityMask failed (insufficient privileges?). Continuing.\n";
    }
    else {
        cout << "Set process affinity mask to use " << cores << " core(s).\n";
    }

    // Загрузка изображения
    Image input;
    cout << "Reading input BMP...\n";
    if (!ReadBMP24(inFile.c_str(), input)) {
        cerr << "Failed to read BMP: " << inFile << "\n";
        return 2;
    }

    cout << "Image: " << input.width << "x" << input.height << ", rowStride=" << input.rowStride << "\n";

    // Подготовка данных для многопоточного выполнения
    int N = threads;
    auto assignments = BuildAndDistributeTiles(input.width, input.height, N);
    assert((int)assignments.size() == N);

    Image parOut = input;
    parOut.data = input.data;

    auto globalStartTime = Clock::now();

    // Многопоточное выполнение
    auto runParallelOnce = [&](Image& outImg) -> double {
        Image srcBuf = input;
        srcBuf.data = input.data;
        Image dstBuf = input;
        dstBuf.data = input.data;

        auto parStart = Clock::now();

        for (int it = 0; it < iterations; ++it) {
            vector<ThreadParams> params(N);
            vector<HANDLE> handles(N);

            for (int t = 0; t < N; ++t) {
                params[t].src = &srcBuf;
                params[t].dst = &dstBuf;
                params[t].tiles = &assignments[t];
                params[t].threadId = t;
                params[t].startTime = globalStartTime;
                params[t].priority = priorities[t];

                handles[t] = CreateThread(
                    nullptr,
                    0,
                    ThreadProc,
                    &params[t],
                    0,
                    nullptr
                );

                if (!handles[t]) {
                    cerr << "CreateThread failed for thread " << t << "\n";
                    exit(3);
                }
            }

            DWORD res = WaitForMultipleObjects(
                (DWORD)handles.size(),
                handles.data(),
                TRUE,
                INFINITE
            );

            if (res == WAIT_FAILED) {
                cerr << "WaitForMultipleObjects failed\n";
                exit(4);
            }

            for (HANDLE h : handles) CloseHandle(h);

            srcBuf.data.swap(dstBuf.data);
        }

        outImg = srcBuf;
        auto parEnd = Clock::now();
        return ms(parEnd - parStart).count();
        };

    double parMs = runParallelOnce(parOut);
    cout << "Parallel run time (threads=" << threads << ", iterations=" << iterations << "): " << parMs << " ms\n";

    WriteBMP24(outFile.c_str(), parOut);

    // Сохранение данных о времени работы потоков
    SaveThreadTimeDataToCSV(dataFile);

    cout << "Wrote output: " << outFile << "\n";
    cout << "Thread time data saved to: " << dataFile << "\n";
    cout << "CSV format: ThreadID,Time(ms)\n";

    // Очистка
    DeleteCriticalSection(&g_csData);

    return 0;
}