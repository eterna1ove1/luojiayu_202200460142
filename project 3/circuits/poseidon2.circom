pragma circom 2.0.0;

include "./poseidon2lib.circom";

template Poseidon2Hash() {
    signal input x[2];  // 私有输入
    signal output hash; // 公开输出

    component poseidon2 = Poseidon2();
    poseidon2.x[0] <== x[0];
    poseidon2.x[1] <== x[1];
    
    hash <== poseidon2.hash;
}

component main = Poseidon2Hash();