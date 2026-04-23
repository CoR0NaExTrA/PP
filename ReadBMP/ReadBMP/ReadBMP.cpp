#include <windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <cassert>

using namespace std;
using Clock = chrono::high_resolution_clock;
using ms = chrono::duration<double, milli>;

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

void BlurPass3x3(const Image& src, Image& dst, int y0, int y1, int x0, int x1) {
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
        }
    }
}

void RunSequential(const Image& in, Image& out, int iterations) {
    Image a = in;
    Image b = in;
    b.data = a.data;
    for (int it = 0; it < iterations; ++it) {
        BlurPass3x3(a, b, 0, a.height, 0, a.width);
        a.data.swap(b.data);
    }
    out = a;
}

struct Tile { int x0, x1, y0, y1; };
void ThreadWorkerOnce(const Image& src, Image& dst, const vector<Tile>& tiles) {
    for (const Tile& t : tiles) {
        BlurPass3x3(src, dst, t.y0, t.y1, t.x0, t.x1);
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
};

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    ThreadParams* p = reinterpret_cast<ThreadParams*>(lpParam);
    ThreadWorkerOnce(*p->src, *p->dst, *p->tiles);
    return 0;
};

int main(int argc, char** argv) {
    if (argc < 5) {
        cout << "Usage: " << argv[0] << " input.bmp output.bmp threads cores\n";
        cout << "Example: " << argv[0] << " in.bmp out.bmp 4 2\n";
        return 1;
    }
    const char* inFile = argv[1];
    const char* outFile = argv[2];
    int threads = atoi(argv[3]);
    int cores = atoi(argv[4]);

    if (threads < 1 || threads > 16) {
        cerr << "threads must be in [1,16]\n"; return 1;
    }
    if (cores < 1 || cores > 4) {
        cerr << "cores must be in [1,4]\n"; return 1;
    }

    DWORD_PTR mask = ((1ULL << cores) - 1ULL);
    HANDLE hProc = GetCurrentProcess();
    BOOL affOk = SetProcessAffinityMask(hProc, mask);
    if (!affOk) {
        cerr << "Warning: SetProcessAffinityMask failed (insufficient privileges?). Continuing.\n";
    }
    else {
        cout << "Set process affinity mask to use " << cores << " core(s).\n";
    }

    Image input;
    cout << "Reading input BMP...\n";
    auto t0 = Clock::now();
    if (!ReadBMP24(inFile, input)) {
        cerr << "Failed to read BMP: " << inFile << "\n";
        return 2;
    }
    auto t_read = Clock::now();
    cout << "Image: " << input.width << "x" << input.height << ", rowStride=" << input.rowStride << "\n";

    Image seqOut = input;
    seqOut.data = input.data;
    Image parOut = input;
    parOut.data = input.data;

    int iterations = 40;

    auto seqStart = Clock::now();
    RunSequential(input, seqOut, iterations);
    string tmpSeq = "tmp_seq_out.bmp";
    WriteBMP24(tmpSeq.c_str(), seqOut);
    auto seqEnd = Clock::now();

    double seqMs = ms(seqEnd - seqStart).count();
    cout << "Sequential run time (iterations=" << iterations << "): " << seqMs << " ms\n";

    if (seqMs < 500.0) {
        int desired = (int)ceil(500.0 / max(seqMs, 1.0));
        if (desired > 2000) desired = 2000;
        iterations = max(iterations, desired);
        cout << "Increasing iterations to " << iterations << " to get measurable times.\n";

        seqStart = Clock::now();
        RunSequential(input, seqOut, iterations);
        WriteBMP24(tmpSeq.c_str(), seqOut);
        seqEnd = Clock::now();
        seqMs = ms(seqEnd - seqStart).count();
        cout << "New sequential run time (iterations=" << iterations << "): " << seqMs << " ms\n";
    }

    int N = threads;
    auto assignments = BuildAndDistributeTiles(input.width, input.height, N);
    assert((int)assignments.size() == N);

    auto runParallelOnce = [&](Image& outImg) -> double {
        Image srcBuf = input;
        srcBuf.data = input.data;
        Image dstBuf = input;
        dstBuf.data = input.data;

        auto parStart = Clock::now();

        for (int it = 0; it < iterations; ++it) {
            // Создаём структуры параметров
            vector<ThreadParams> params(N);
            for (int t = 0; t < N; ++t) {
                params[t].src = &srcBuf;
                params[t].dst = &dstBuf;
                params[t].tiles = &assignments[t];
            }

            // Создаём потоки
            vector<HANDLE> handles(N);
            for (int t = 0; t < N; ++t) {
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

        string tmpPar = "tmp_par_out.bmp";
        WriteBMP24(tmpPar.c_str(), outImg);

        auto parEnd = Clock::now();
        return ms(parEnd - parStart).count();
        };



    double parMs = runParallelOnce(parOut);
    cout << "Parallel run time (threads=" << threads << ", iterations=" << iterations << "): " << parMs << " ms\n";

    WriteBMP24(outFile, parOut);

    double S = seqMs / parMs;
    double E = S / threads;
    cout << "\n=== Results ===\n";
    cout << "T1 (sequential) = " << seqMs << " ms\n";
    cout << "T" << threads << " (parallel) = " << parMs << " ms\n";
    cout << "Speedup S" << threads << " = " << S << "\n";
    cout << "Efficiency E" << threads << " = " << E << "\n";
    cout << "Iterations used = " << iterations << "\n";

    cout << "Wrote output: " << outFile << "\n";
    cout << "Temporary files: tmp_seq_out.bmp, tmp_par_out.bmp (can be removed)\n";
    return 0;
}
