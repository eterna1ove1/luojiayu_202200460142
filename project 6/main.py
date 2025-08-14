from Crypto.Util.number import getPrime, getRandomRange
from Crypto.Hash import SHA256
from phe import paillier
import random
import math


class DDHPrivateIntersectionSum:
    def __init__(self, p=None, g=None):
        # 初始化阶段:设置参数
        if p is None or g is None:
            # 生成一个安全素数用于DDH(实际应用中应该更大)
            self.p = getPrime(20)  # 小素数用于演示
            # 寻找群的生成元
            self.g = self.find_generator(self.p)
        else:
            self.p = p
            self.g = g

        # 初始化哈希函数(建模为随机预言机)
        self.hash_func = SHA256.new

    def find_generator(self, p):
        # 寻找模p乘法群的生成元
        # 这是简化方法-实际应用中应使用安全素数和正确的生成元
        for g in range(2, p):
            if math.gcd(g, p) == 1:
                return g
        raise ValueError("无法为给定素数找到生成元")

    def hash_to_group(self, element):
        # 将元素哈希到群成员
        h = self.hash_func(str(element).encode()).hexdigest()
        return pow(self.g, int(h, 16) % (self.p - 1), self.p)

    def party1_round1(self, V, k1=None):
        # P1的第一轮:计算并发送Z
        self.V = V
        self.k1 = k1 if k1 is not None else getRandomRange(1, self.p - 1)

        Z = []
        for v in V:
            h = self.hash_to_group(v)
            z = pow(h, self.k1, self.p)
            Z.append(z)

        # 打乱集合Z的顺序
        random.shuffle(Z)
        return Z

    def party2_round2(self, Z, W, k2=None, paillier_key=None):
        # P2的处理:计算Z2和加密值
        self.W = W
        self.k2 = k2 if k2 is not None else getRandomRange(1, self.p - 1)

        # 生成Paillier密钥(如果未提供)
        if paillier_key is None:
            self.paillier_public, self.paillier_private = paillier.generate_paillier_keypair()
        else:
            self.paillier_public, self.paillier_private = paillier_key

        # 计算Z2
        Z2 = []
        for z in Z:
            z2 = pow(z, self.k2, self.p)
            Z2.append(z2)

        # 为W中的每个元素计算h'_j和加密的t_j
        encrypted_values = []
        h_primes = []
        for w, t in W:
            h_prime = pow(self.hash_to_group(w), self.k2, self.p)
            c_j = self.paillier_public.encrypt(t)
            encrypted_values.append((h_prime, c_j))
            h_primes.append(h_prime)

        # 打乱加密值的顺序
        combined = list(zip(h_primes, encrypted_values))
        random.shuffle(combined)
        h_primes_shuffled, encrypted_values_shuffled = zip(*combined)

        return Z2, self.paillier_public, encrypted_values_shuffled

    def party1_round3(self, Z2, encrypted_values):
        # P1的最后一轮:计算交集并求和
        intersection_sum = 0

        for h_prime, c_j in encrypted_values:
            h_double_prime = pow(h_prime, self.k1, self.p)
            if h_double_prime in Z2:
                intersection_sum += c_j

        return intersection_sum

    def party2_decrypt(self, encrypted_sum):
        # P2解密最终的和
        return self.paillier_private.decrypt(encrypted_sum)


# 示例用法
if __name__ == "__main__":
    # 设置参数(实际应用中这些应该大得多)
    p = 100003  # 素数
    g = 2  # 群的生成元

    # 初始化协议
    protocol = DDHPrivateIntersectionSum(p, g)

    # 参与方1的数据
    V = ["aa", "bb", "cc", "dd"]

    # 参与方2的数据(带值)
    W = [("aa", 10), ("bb", 20), ("ee", 30), ("dd", 40)]

    # 协议执行
    # 第一轮: P1 → P2
    Z = protocol.party1_round1(V)

    # 第二轮: P2处理并回复
    Z2, pk, encrypted_values = protocol.party2_round2(Z, W)

    # 第三轮: P1计算交集和
    encrypted_sum = protocol.party1_round3(Z2, encrypted_values)

    # P2解密结果
    intersection_sum = protocol.party2_decrypt(encrypted_sum)

    # 打印结果
    print("参与方1的集合:", V)
    print("参与方2的集合(带值):", W)
    print("交集和:", intersection_sum)

    # 验证结果
    intersection = set(V) & set([w for w, t in W])
    expected_sum = sum(t for w, t in W if w in intersection)
    print("期望的和:", expected_sum)
    print("协议是否正确:", intersection_sum == expected_sum)