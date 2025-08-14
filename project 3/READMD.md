## 整体架构

代码实现包含了三层结构：

1. **顶层电路**：Poseidon2Hash：定义了公开/私有输入输出接口，调用核心 Poseidon2 组件

2. **核心算法** ：Poseidon2：实现完整的哈希计算流程，包含轮函数模板

3. **测试**：生成标准哈希结果输出 Circom 所需的输入文件

   

## **Poseidon2 哈希算法的电路实现思路**
Poseidon 是一种 **ZK-friendly 哈希算法**，专为零知识证明优化。Poseidon 哈希计算主要包括三步：第一次 Full Rounds、Partial Rounds 和第二次 Full Rounds。poseidon2对poseidon做了优化，如下图

![img](https://img.foresightnews.pro/202404/10-1714270364865.png?x-oss-process=style/scale70)



##  Poseidon2 的数学结构

Poseidon2 的线性层使用的是 **MDS（Maximum Distance Separable）矩阵**，这是一种特殊的矩阵类型，具有最优的扩散属性。

**MDS 矩阵**

一个 \($ t \times t $\) 的 MDS 矩阵 \( $M$\) 满足：
- 任意子方阵都是可逆的
- 满足 **分支数（Branch Number）** 最大化的性质：
  $$
  \mathcal{B}(M) = \min_{x \neq 0} (wt(x) + wt(M \cdot x)) = t + 1
  $$
  其中 \( wt \) 表示非零元素的个数。

---

在实现的的 Circom 代码中使用的 MDS
```circom
var MDS = [
    ["0x0a0b0c0d0e0f0102", "0x030405060708090a"], // 第一行
    ["0x0b0c0d0e0f010203", "0x0405060708090a0b"]  // 第二行
];
```
这是一个 \($ 2 \times 2$\) 的 MDS 矩阵，用于 2 元素的状态扩散。

---

**MDS 矩阵的具有完全扩散性**：单个输入比特的变化会影响所有输出比特。若输入为 $( [x_0, x_1] )$，则输出为：
$$
y_0 = m_{00}x_0 + m_{01}x_1 \\
     y_1 = m_{10}x_0 + m_{11}x_1
$$
即使仅 $ x_0  $变化，$ y_0 $ 和 $y_1  $都会变化。因此具有**密码学安全性**：阻止攻击者通过线性关系破解算法。

在 `FullRound` 和 `PartialRound` 模板中，矩阵乘法显式实现为：
```circom
// MDS 矩阵乘法
out[0] <== mds[0][0]*s0 + mds[0][1]*s1;
out[1] <== mds[1][0]*s0 + mds[1][1]*s1;
```
依据$\mathbf{y} = M \cdot \mathbf{x}$。

对于 \( t=2 \)。矩阵通常形如：
$$
M = \begin{bmatrix}
  \alpha & 1 \\
  1 & \alpha
  \end{bmatrix}
$$
如果是$t=n$ 则形如

![img](https://img.foresightnews.pro/202404/10-1714270763955.png?x-oss-process=style/scale70)



## 电路设计
- **输入大小（t）**：每个 block 的域元素数量（如 t=2 或 t=3）。
- **安全位数（n）**：目标安全级别（如 n=256）。
- **轮数（R_F + R_P）**：
  - \( R_F \)：完整轮次（Full Rounds，所有 S-box 激活）。
  - \( R_P \)：部分轮次（Partial Rounds，仅 1 个 S-box 激活）。

**常见配置**：
| 安全级别 (n) | 输入大小 (t) | 总轮数 (R_F + R_P) |
| ------------ | ------------ | ------------------ |
| 256-bit      | 2            | 8 + 56 = 64        |
| 256-bit      | 3            | 8 + 57 = 65        |



##  实现步骤
### **1、电路结构**
1. **输入顶层层**：私有输入：原像（如 2 个域元素 `x[0]`, `x[1]`）。公开输入：预期哈希值 `hash`。
2. **Poseidon 核心**：初始化状态：`state = [x[0], x[1], 0, ..., 0]`（填充至 t 个元素）。包含轮函数处理结构

```circom
template Poseidon2() {
     // 初始化状态
     var state = [x[0], x[1]];

     // 轮函数处理
     state = FullRound(state); // 前4轮
     state = PartialRound(state); // 56轮
     state = FullRound(state); // 后4轮
    
     hash <== state[0]; // 输出最终哈希

   }
```

3. **输出层**：最后测试一下，生成电路的公开输入(`input.json`)

### **2、 约束生成**
- **S-box 约束**：对每个状态元素 $ s_i $，计算 $s_i^5 $。
- **MDS 约束**：矩阵乘法中每个输出是输入的线性组合）。
- **哈希匹配约束**：强制电路输出等于公开的 `hash`。

---



这样实现的 Poseidon2 电路既高效又可验证，最后运行结果验证如下，输入在input文件架下的.json



![image-20250813191039400](C:\Users\LuoJY\AppData\Roaming\Typora\typora-user-images\image-20250813191039400.png)