import random
from math import gcd, ceil, log
from gmssl import sm3


# Helper functions for data type conversions
def int_to_bytes(x, k):
    if pow(256, k) <= x:
        raise Exception("Integer too large for target byte length")
    return x.to_bytes(k, 'big')


def bytes_to_int(M):
    return int.from_bytes(M, 'big')


def bits_to_bytes(s):
    k = ceil(len(s) / 8)
    s = s.zfill(k * 8)
    return int(s, 2).to_bytes(k, 'big')


def bytes_to_bits(M):
    return bin(int.from_bytes(M, 'big'))[2:].zfill(8 * len(M))


def fielde_to_bytes(e):
    q = 0x8542D69E4C044F18E8B92435BF6FF7DE457283915C45517D722EDB8B08F1DFC3
    l = ceil(ceil(log(q, 2) / 8))
    return int_to_bytes(e, l)


def bytes_to_fielde(M):
    return bytes_to_int(M)


def point_to_bytes(P):
    xp, yp = P[0], P[1]
    x = fielde_to_bytes(xp)
    y = fielde_to_bytes(yp)
    return b'\x04' + x + y


def bytes_to_point(s):
    if len(s) % 2 == 0 or s[0] != 4:
        raise Exception("Invalid point format")
    l = (len(s) - 1) // 2
    x = bytes_to_fielde(s[1:l + 1])
    y = bytes_to_fielde(s[l + 1:2 * l + 1])
    return (x, y)


# Cryptographic operations
def KDF(Z_bits, klen):
    v = 256  # SM3 hash size in bits
    if klen >= (pow(2, 32) - 1) * v:
        raise Exception("KDF output length too large")

    l = ceil(klen / v)
    Ha = []
    Z_bytes = bits_to_bytes(Z_bits)

    for i in range(1, l + 1):
        s = Z_bytes + i.to_bytes(4, 'big')
        s_list = list(s)
        hash_hex = sm3.sm3_hash(s_list)
        hash_bits = bin(int(hash_hex, 16))[2:].zfill(256)
        Ha.append(hash_bits)

    k = ''.join(Ha)[:klen]
    return k


def calc_inverse(M, m):
    if gcd(M, m) != 1:
        return None
    return pow(M, -1, m)


def add_point(P, Q, p, a):
    if P == 0: return Q
    if Q == 0: return P
    x1, y1, x2, y2 = P[0], P[1], Q[0], Q[1]

    if x1 == x2:
        if y1 == y2:
            return double_point(P, p, a)
        return 0

    l = (y2 - y1) * calc_inverse(x2 - x1, p) % p
    x3 = (l * l - x1 - x2) % p
    y3 = (l * (x1 - x3) - y1) % p
    return (x3, y3)


def double_point(P, p, a):
    if P == 0: return P
    x1, y1 = P[0], P[1]
    l = (3 * x1 * x1 + a) * calc_inverse(2 * y1, p) % p
    x3 = (l * l - 2 * x1) % p
    y3 = (l * (x1 - x3) - y1) % p
    return (x3, y3)


def mult_point(P, k, p, a):
    Q = 0
    for bit in bin(k)[2:]:
        Q = double_point(Q, p, a)
        if bit == '1':
            Q = add_point(P, Q, p, a)
    return Q


# SM2 encryption and decryption
def encry_sm2(args, PB, M):
    p, a, b, h, G, n = args
    M_bytes = M.encode('ascii')

    # Step A1: Generate random k
    k = random.randint(1, n - 1)
    print(f"Random k: {hex(k)[2:]}")

    # Step A2: Compute C1 = [k]G
    C1 = mult_point(G, k, p, a)
    print(f"C1 point: ({hex(C1[0])[2:]}, {hex(C1[1])[2:]})")

    # Step A4: Compute [k]PB = (x2,y2)
    x2, y2 = mult_point(PB, k, p, a)
    x2_bits = bytes_to_bits(fielde_to_bytes(x2))
    y2_bits = bytes_to_bits(fielde_to_bytes(y2))

    # Step A5: Compute t = KDF(x2||y2, klen)
    klen = len(M_bytes) * 8
    t = KDF(x2_bits + y2_bits, klen)
    print(f"KDF output t: {hex(int(t, 2))[2:]}")

    # Step A6: Compute C2 = M XOR t
    C2 = int.from_bytes(M_bytes, 'big') ^ int(t, 2)

    # Step A7: Compute C3 = Hash(x2||M||y2)
    C3 = sm3.sm3_hash(list(fielde_to_bytes(x2) + M_bytes + fielde_to_bytes(y2)))

    # Step A8: Output ciphertext C = C1||C2||C3
    C = point_to_bytes(C1) + C2.to_bytes(ceil(len(t) / 8), 'big') + bytes.fromhex(C3)
    return C.hex()


def decry_sm2(args, dB, C):
    p, a, b, h, G, n = args
    C_bytes = bytes.fromhex(C)

    # Step B1: Extract C1 and convert to point
    l = ceil(ceil(log(p, 2) / 8))
    C1_bytes = C_bytes[:2 * l + 1]
    C1 = bytes_to_point(C1_bytes)

    # Step B3: Compute [dB]C1 = (x2,y2)
    x2, y2 = mult_point(C1, dB, p, a)
    x2_bits = bytes_to_bits(fielde_to_bytes(x2))
    y2_bits = bytes_to_bits(fielde_to_bytes(y2))

    # Step B4: Compute t = KDF(x2||y2, klen)
    C3_length = 32  # SM3 output is 32 bytes
    C2_length = len(C_bytes) - (2 * l + 1) - C3_length
    klen = C2_length * 8
    t = KDF(x2_bits + y2_bits, klen)

    # Step B5: Extract C2 and compute M' = C2 XOR t
    C2_start = 2 * l + 1
    C2_end = C2_start + C2_length
    C2 = C_bytes[C2_start:C2_end]
    M_bytes = (int.from_bytes(C2, 'big') ^ int(t, 2)).to_bytes(C2_length, 'big')

    # Step B6: Verify hash
    C3 = C_bytes[C2_end:].hex()
    computed_hash = sm3.sm3_hash(list(fielde_to_bytes(x2) + M_bytes + fielde_to_bytes(y2)))
    if computed_hash != C3:
        raise Exception("Hash verification failed")

    return M_bytes.decode('ascii')


# System parameters and key setup
def get_args():
    p = 0x8542D69E4C044F18E8B92435BF6FF7DE457283915C45517D722EDB8B08F1DFC3
    a = 0x787968B4FA32C3FD2417842E73BBFEFF2F3C848B6831D7E0EC65228B3937E498
    b = 0x63E4C6D3B23B0C849CF84241484BFE48F61D59A5B16BA06E6E12D1DA27C5249A
    h = 1
    G = (0x421DEBD61B62EAB6746434EBC3CC315E32220B3BADD50BDC4C4E6C147FEDD43D,
         0x0680512BCBB42C07D47349D2153B70C4E5D7FDFCBFA36EA1A85841B9E46E09A2)
    n = 0x8542D69E4C044F18E8B92435BF6FF7DD297720630485628D5AE74EE7C32E79B7
    return (p, a, b, h, G, n)


def get_key():
    PB = (0x435B39CCA8F3B508C1488AFC67BE491A0F7BA07E581A0E4849A5CF70628A7E0A,
          0x75DDBA78F15FEECB4C7895E2C1CDF5FE01DEBB2CDBADF45399CCF77BBA076A42)
    dB = 0x1649AB77A00637BD5E2EFE283FBF353534AA7F7CB89463F208DDBC2920BB0DA0
    return (PB, dB)


# Main execution
if __name__ == "__main__":
    print("SM2椭圆曲线公钥密码算法".center(60, '='))
    args = get_args()
    key_B = get_key()

    M = input("请输入要加密的明文: ")
    print(f"\n明文: {M}")

    print("\n加密过程:")
    C = encry_sm2(args, key_B[0], M)
    print(f"\n密文: {C}")

    print("\n解密过程:")
    de_M = decry_sm2(args, key_B[1], C)
    print(f"\n解密结果: {de_M}")

    print("\n验证结果:", "成功" if M == de_M else "失败")