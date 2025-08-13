#include <stdio.h>
#include <stdint.h>  
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <intrin.h>
#include "sm4_aesni.h"

#define TEST_GROUPS 100     // 明文样本组数（每组包含4个分组）
#define TEST_ROUNDS 10000   // 每组加解密轮数

// 密钥扩展：加密或解密（方向由 reverse 控制）
void key_expansion(const uint8_t* key, uint32_t rk[32], int reverse) {
    uint32_t MK[4], K[36];
    memcpy(MK, key, 16);

    K[0] = MK[0] ^ 0xa3b1bac6;
    K[1] = MK[1] ^ 0x56aa3350;
    K[2] = MK[2] ^ 0x677d9197;
    K[3] = MK[3] ^ 0xb27022dc;

    for (int i = 0; i < 32; ++i) {
        uint32_t temp = K[i + 1] ^ K[i + 2] ^ K[i + 3] ^ CK[i];
        temp = tau(temp);
        rk[i] = K[i] ^ L_prime(temp);
        K[i + 4] = rk[i];
    }

    if (reverse) {
        // 创建临时数组存储反向密钥
        uint32_t temp_rk[32];
        for (int i = 0; i < 32; ++i) {
            temp_rk[i] = rk[31 - i];
        }
        memcpy(rk, temp_rk, sizeof(temp_rk));
    }
}

// 生成随机明文（每组4个分组，共64字节）
void generate_random_plaintexts(uint8_t blocks[TEST_GROUPS][64]) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < TEST_GROUPS; ++i) {
        for (int j = 0; j < 64; ++j) {
            blocks[i][j] = rand() & 0xFF;
        }
    }
}

int main() {
    uint8_t key[16] = { 0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef };
    uint8_t plaintexts[TEST_GROUPS][64];  // 每组4个分组
    uint8_t ciphertexts[TEST_GROUPS][64];
    uint8_t decrypted[TEST_GROUPS][64]; 
    uint32_t enc_rk[32], dec_rk[32];
    unsigned __int64 start, end;

    generate_random_plaintexts(plaintexts);
    key_expansion(key, enc_rk, 0);
    key_expansion(key, dec_rk, 1);

    // ===== AES-NI加密测试 =====
    if (has_aesni()) {
        // ===== 4分组并行加密测试 =====
        _ReadWriteBarrier();
        start = __rdtsc();
        for (int i = 0; i < TEST_ROUNDS; ++i) {
            for (int j = 0; j < TEST_GROUPS; ++j) {
                sm4_encrypt_4blocks_aesni(plaintexts[j], ciphertexts[j], enc_rk);
            }
        }
        end = __rdtsc();
        _ReadWriteBarrier();
        uint64_t encrypt_cycles = end - start;

        // ===== 4分组并行解密测试 ===== 
        _ReadWriteBarrier();
        start = __rdtsc();
        for (int i = 0; i < TEST_ROUNDS; ++i) {
            for (int j = 0; j < TEST_GROUPS; ++j) {
                sm4_decrypt_4blocks_aesni(ciphertexts[j], decrypted[j], dec_rk);
            }
        }
        end = __rdtsc();
        _ReadWriteBarrier();
        uint64_t decrypt_cycles = end - start;

        // ===== 打印性能报告 =====
        const double cpu_ghz = 2.5; // 根据实际CPU修改
        const uint64_t total_blocks = TEST_GROUPS * TEST_ROUNDS * 4; // 总处理分组数
        
        printf("\n⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");
        printf("          SM4 AES-NI 4分组并行性能测试报告         \n");
        printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");
        
        // 加密性能
        printf("│ %-20s: %lu 次\n", "加密操作总数(4分组)", TEST_GROUPS * TEST_ROUNDS);
        printf("│ %-20s: %lu 个分组\n", "总加密分组数", total_blocks);
        printf("│ %-20s: %lu CPU周期\n", "加密总耗时", encrypt_cycles);
        printf("├─────────────────────────────────────\n");
        printf("│ %-20s: %.2f cycles/block\n", "加密单块耗时", 
            (double)encrypt_cycles / total_blocks);
        printf("│ %-20s: %.2f MB/s\n", "加密吞吐量", 
            (total_blocks * 16) / (encrypt_cycles / (cpu_ghz * 1e9)) / (1024 * 1024));
        
        // 解密性能  
        printf("\n│ %-20s: %lu 次\n", "解密操作总数(4分组)", TEST_GROUPS * TEST_ROUNDS);
        printf("│ %-20s: %lu 个分组\n", "总解密分组数", total_blocks);
        printf("│ %-20s: %lu CPU周期\n", "解密总耗时", decrypt_cycles);
        printf("├─────────────────────────────────────\n");
        printf("│ %-20s: %.2f cycles/block\n", "解密单块耗时",
            (double)decrypt_cycles / total_blocks);
        printf("│ %-20s: %.2f MB/s\n", "解密吞吐量",
            (total_blocks * 16) / (decrypt_cycles / (cpu_ghz * 1e9)) / (1024 * 1024));
        
        // 综合对比
        printf("├─────────────────────────────────────\n");
        printf("│ %-20s: %.2f%%\n", "解密/加密耗时比",
            (double)decrypt_cycles / encrypt_cycles * 100);
        printf("│ %-20s: %.2f MB/s\n", "平均吞吐量",
            ((total_blocks * 32) / ((encrypt_cycles + decrypt_cycles) / (cpu_ghz * 1e9))) / (1024 * 1024));
        printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");
        
        // 验证结果（检查每组4个分组）
        int errors = 0;
        for (int j = 0; j < TEST_GROUPS; ++j) {
            if (memcmp(plaintexts[j], decrypted[j], 64) != 0) errors++;
        }
        printf("│ %-20s: %d/%d (组)\n", "数据匹配数量", errors, TEST_GROUPS);
        printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n\n");
    } else {
        printf("CPU不支持AES-NI指令集\n");
    }
    
    return 0;
}