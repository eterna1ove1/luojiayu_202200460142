#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <intrin.h>  
#include "sm4.h"

#define TEST_GROUPS 100     // 明文样本组数
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
		for (int i = 0; i < 16; ++i) {
			uint32_t t = rk[i];
			rk[i] = rk[31 - i];
			rk[31 - i] = t;
		}
	}
}
// SM4加密单块
void sm4_encrypt_block(const uint8_t input[16], uint8_t output[16], const uint32_t rk[32]) {
    uint32_t X[36];
    memcpy(X, input, 16);  // 避免未对齐访问

    for (int i = 0; i < 32; ++i) {
        uint32_t temp = X[i + 1] ^ X[i + 2] ^ X[i + 3] ^ rk[i];
        X[i + 4] = X[i] ^ L(tau(temp));
    }

    uint32_t out[4] = { X[35], X[34], X[33], X[32] };
    memcpy(output, out, 16);
}

// SM4解密单块
void sm4_decrypt_block(const uint8_t input[16], uint8_t output[16], const uint32_t rk[32]) {
    // 解密与加密结构相同，只是轮密钥顺序相反
    sm4_encrypt_block(input, output, rk);
}

// 显示字节数组 (保持不变)
void print_block(const char* label, const uint8_t* data, int len) {
    printf("%s:\n", label);
    for (int i = 0; i < len; ++i)
        printf("%02x ", data[i]);
    printf("\n\n");
}

// 生成随机明文 (保持不变)
void generate_random_plaintexts(uint8_t blocks[TEST_GROUPS][16]) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < TEST_GROUPS; ++i) {
        for (int j = 0; j < 16; ++j) {
            blocks[i][j] = rand() & 0xFF;
        }
    }
}

int main() {
    uint8_t key[16] = { 0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef };
    uint8_t plaintexts[TEST_GROUPS][16];
    uint8_t ciphertexts[TEST_GROUPS][16];
    uint8_t decrypted[TEST_GROUPS][16]; 
    uint32_t enc_rk[32], dec_rk[32];

    generate_random_plaintexts(plaintexts);





    // ===== 加密 =====
    key_expansion(key, enc_rk, 0); // 生成加密轮密钥
    unsigned __int64 start, end;

    _ReadWriteBarrier(); // 防止重排序
    start = __rdtsc();
    for (int i = 0; i < TEST_ROUNDS; ++i) {
        for (int j = 0; j < TEST_GROUPS; ++j) {
            sm4_encrypt_block(plaintexts[j], ciphertexts[j], enc_rk);
        }
    }
    end = __rdtsc();
    _ReadWriteBarrier();

    uint64_t total_encrypt_cycles = end - start;

    printf("\n⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");
    printf("        SM4 加密性能测试报告\n");
    printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");

    printf("│ %-14s: %d组 × %d次 = %d次加密\n", 
        "测试规模", TEST_GROUPS, TEST_ROUNDS, TEST_GROUPS * TEST_ROUNDS);
    printf("│ %-14s: %llu CPU时钟周期\n", "总耗时", total_encrypt_cycles);

    // 核心性能指标
    printf("├───────────────────────────────────\n");
    printf("│ %-14s: %.2f cycles/block\n", "单块加密耗时", 
        (double)total_encrypt_cycles / (TEST_GROUPS * TEST_ROUNDS));

    // 吞吐量计算（假设已知CPU频率为3.5GHz）
    double cpu_ghz = 2.5;
    double throughput = (TEST_GROUPS * TEST_ROUNDS * 16) / 
                    (total_encrypt_cycles / (cpu_ghz * 1e9))/(1024 * 1024);
    printf("│ %-14s: %.2f MB/s\n", "吞吐量", throughput);

    // 测试环境信息
    printf("├───────────────────────────────────\n");
    printf("│ %-14s: %.1f GHz (预设)\n", "CPU频率", cpu_ghz);
    printf("│ %-18s: %s\n", "测试时间", __TIMESTAMP__);
    printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n\n");







    // ===== 解密 =====
    key_expansion(key, dec_rk, 1); // 生成解密轮密钥（顺序相反）

    _ReadWriteBarrier();
    start = __rdtsc();
    for (int i = 0; i < TEST_ROUNDS; ++i) {
        for (int j = 0; j < TEST_GROUPS; ++j) {
            sm4_decrypt_block(ciphertexts[j], decrypted[j], dec_rk);
        }
    }
    end = __rdtsc();
    _ReadWriteBarrier();

    uint64_t total_decrypt_cycles = end - start;
    printf("\n⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");
    printf("        SM4 解密性能测试报告\n");
    printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n");

    // 基础测试信息
    printf("│ %-14s: %d组 × %d次 = %d次解密\n", 
        "测试规模", TEST_GROUPS, TEST_ROUNDS, TEST_GROUPS * TEST_ROUNDS);
    printf("│ %-14s: %llu CPU时钟周期\n", "总耗时", total_decrypt_cycles);

    // 核心性能指标
    printf("├───────────────────────────────────\n");
    printf("│ %-14s: %.2f cycles/block\n", "单块解密耗时", 
        (double)total_decrypt_cycles / (TEST_GROUPS * TEST_ROUNDS));

    // 吞吐量计算（使用与加密测试相同的CPU频率）
    double decrypt_throughput = (TEST_GROUPS * TEST_ROUNDS * 16.0) /  // 总字节数
                            (total_decrypt_cycles / (cpu_ghz * 1e9)) / // 总秒数
                            (1024 * 1024); // 转换为MB
    printf("│ %-14s: %.2f MB/s\n", "吞吐量", decrypt_throughput);

    // 测试环境信息
   printf("├───────────────────────────────────\n");
    printf("│ %-14s: %.1f GHz (预设)\n", "CPU频率", cpu_ghz);
    printf("│ %-18s: %s\n", "测试时间", __TIMESTAMP__);
    printf("⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯⎯\n\n");


    // 验证解密结果
    int error_count = 0;
    for (int j = 0; j < TEST_GROUPS; ++j) {
        if (memcmp(plaintexts[j], decrypted[j], 16) != 0) {
            error_count++;
           // 打印第一个错误样本
           if (error_count == 1) {
               print_block("原始明文", plaintexts[j], 16);
              print_block("解密结果", decrypted[j], 16);
           }
       }
    }
    
    printf("解密验证：%d/%d 组数据不匹配\n", error_count, TEST_GROUPS);
       return 0;
    }