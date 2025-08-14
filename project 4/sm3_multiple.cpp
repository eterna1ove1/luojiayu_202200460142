#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstdint>
#include <vector>
#include <thread>
#include <mutex>

using namespace std;

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define P0(x) ((x) ^ ROTATE_LEFT((x), 9) ^ ROTATE_LEFT((x), 17))
#define P1(x) ((x) ^ ROTATE_LEFT((x), 15) ^ ROTATE_LEFT((x), 23))

// 初始向量
const uint32_t IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

const uint32_t T[2] = {0x79CC4519, 0x7A879D8A};

mutex mtx;// 互斥锁用于线程同步

// 处理单个消息块的函数
void process_block(const uint32_t* block, uint32_t* V) {
    uint32_t W[68] = {0};
    uint32_t W1[64] = {0};

    // 消息扩展
    for (int j = 0; j < 16; ++j) {
        W[j] = block[j];
    }

    for (int j = 16; j < 68; ++j) {
        W[j] = P1(W[j-16] ^ W[j-9] ^ ROTATE_LEFT(W[j-3], 15)) ^ 
               ROTATE_LEFT(W[j-13], 7) ^ W[j-6];
    }

    for (int j = 0; j < 64; ++j) {
        W1[j] = W[j] ^ W[j+4];
    }

    // Compression function
    uint32_t A = V[0], B = V[1], C = V[2], D = V[3];
    uint32_t E = V[4], F = V[5], G = V[6], H = V[7];

    for (int j = 0; j < 64; ++j) {
        uint32_t SS1 = ROTATE_LEFT((ROTATE_LEFT(A, 12) + E + ROTATE_LEFT(T[j < 16 ? 0 : 1], j % 32)), 7);
        uint32_t SS2 = SS1 ^ ROTATE_LEFT(A, 12);
        uint32_t TT1, TT2;

        if (j < 16) {
            TT1 = (A ^ B ^ C) + D + SS2 + W1[j];
            TT2 = (E ^ F ^ G) + H + SS1 + W[j];
        } else {
            TT1 = ((A & B) | (A & C) | (B & C)) + D + SS2 + W1[j];
            TT2 = ((E & F) | ((~E) & G)) + H + SS1 + W[j];
        }

        D = C;
        C = ROTATE_LEFT(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = ROTATE_LEFT(F, 19);
        F = E;
        E = P0(TT2);
    }

    // 使用锁防止更新V时出现竞态条件
    lock_guard<mutex> lock(mtx);
    V[0] ^= A;
    V[1] ^= B;
    V[2] ^= C;
    V[3] ^= D;
    V[4] ^= E;
    V[5] ^= F;
    V[6] ^= G;
    V[7] ^= H;
}

string sm3Hash(const string& message) {
    // Message padding
    uint64_t bitLength = message.size() * 8;
    string padded = message;
    padded += static_cast<char>(0x80);
    
    while ((padded.size() * 8 + 64) % 512 != 0) {
        padded += static_cast<char>(0x00);
    }
    
    // 添加长度(大端序)
    for (int i = 7; i >= 0; --i) {
        padded += static_cast<char>((bitLength >> (i * 8)) & 0xFF);
    }

    // 初始化状态
    uint32_t V[8];
    for (int i = 0; i < 8; ++i) {
        V[i] = IV[i];
    }

    // 确定使用的线程数(基于可用硬件并发性)
    unsigned num_threads = thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // Fallback if hardware_concurrency not available
    
    // 仅对大消息进行并行处理(阈值可调整)
    const size_t parallel_threshold = 1024 * 1024; // 1MB
    
    if (padded.size() < parallel_threshold) {
        // 对小消息顺序处理块
        for (size_t i = 0; i + 64 <= padded.size(); i += 64) {
            uint32_t block[16];
            for (int j = 0; j < 16; ++j) {
                block[j] = (static_cast<uint32_t>(static_cast<unsigned char>(padded[i + j * 4])) << 24 |
                           (static_cast<uint32_t>(static_cast<unsigned char>(padded[i + j * 4 + 1])) << 16 |
                           (static_cast<uint32_t>(static_cast<unsigned char>(padded[i + j * 4 + 2])) << 8 |
                           (static_cast<uint32_t>(static_cast<unsigned char>(padded[i + j * 4 + 3]))))));
            }
            process_block(block, V);
        }
    } else {
        // 对大消息进行并行处理
        vector<thread> threads;
        size_t block_count = padded.size() / 64;
        size_t blocks_per_thread = block_count / num_threads;
        
        // 每个线程获得初始V的副本
        vector<vector<uint32_t>> thread_states(num_threads, vector<uint32_t>(8));
        for (int i = 0; i < num_threads; ++i) {
            copy(V, V + 8, thread_states[i].begin());
        }
        
        // threads
        for (unsigned t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                size_t start_block = t * blocks_per_thread;
                size_t end_block = (t == num_threads - 1) ? block_count : (t + 1) * blocks_per_thread;
                
                for (size_t i = start_block; i < end_block; ++i) {
                    uint32_t block[16];
                    size_t byte_offset = i * 64;
                    for (int j = 0; j < 16; ++j) {
                        block[j] = (static_cast<uint32_t>(static_cast<unsigned char>(padded[byte_offset + j * 4])) << 24 |
                                   (static_cast<uint32_t>(static_cast<unsigned char>(padded[byte_offset + j * 4 + 1])) << 16 |
                                   (static_cast<uint32_t>(static_cast<unsigned char>(padded[byte_offset + j * 4 + 2])) << 8 |
                                   (static_cast<uint32_t>(static_cast<unsigned char>(padded[byte_offset + j * 4 + 3]))))));
                    }
                    process_block(block, thread_states[t].data());
                }
            });
        }
        
        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }
        
        // 合并所有线程的结果
        for (int i = 0; i < 8; ++i) {
            for (int t = 0; t < num_threads; ++t) {
                V[i] ^= thread_states[t][i];
            }
        }
    }

    // Generate hash value
    ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        oss << hex << setfill('0') << setw(8) << V[i];
    }
    return oss.str();
}

int main() {
    string testCases[] = {
        "",
        "abc",
        "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
    };
    cout<<"SM3 Hash threads Results:"<<endl;

    for (const auto& msg : testCases) {
        cout << "SM3(\"" << (msg.empty() ? "empty" : (msg.length() > 20 ? msg.substr(0, 20) + "..." : msg)) 
                  << "\") = " << sm3Hash(msg) << endl;
    }

    return 0;
}