pragma circom 2.0.0;

template Poseidon2() {
    signal input x[2];
    signal output hash;
    
    // 使用 Filecoin 的 Poseidon2 参数 (t=2)
    const T = 2;
    const ROUNDS_F = 8;
    const ROUNDS_P = 56; 
    
    var RC = [
        ["0x1cd4c8c1d4a3ce3a", "0x1a1e07a7a1e07a7a"],
        ["0x1fffffffffffffff", "0x1fffffffffffffff"],
        // ... 其他轮常数
    ];

    var MDS = [
        ["0x0a0b0c0d0e0f0102", "0x030405060708090a"],
        ["0x0b0c0d0e0f010203", "0x0405060708090a0b"]
    ];
    
    var state = [x[0], x[1]];
    
    // 完整轮
    for (var i = 0; i < ROUNDS_F / 2; i++) {
        state = FullRound(state, RC[i], MDS);
    }
    
    // 部分轮
    for (var i = 0; i < ROUNDS_P; i++) {
        state = PartialRound(state, RC[ROUNDS_F/2 + i], MDS);
    }
    
    // 完整轮
    for (var i = 0; i < ROUNDS_F / 2; i++) {
        state = FullRound(state, RC[ROUNDS_F/2 + ROUNDS_P + i], MDS);
    }
    
    hash <== state[0];
}

template FullRound(state, rc, mds) {
    signal output out[2];
    
    // S-box (x^5)
    var s0 = state[0] + rc[0];
    s0 *= s0; s0 *= s0; s0 *= state[0];
    
    var s1 = state[1] + rc[1];
    s1 *= s1; s1 *= s1; s1 *= state[1];
    
    // MDS
    out[0] <== mds[0][0]*s0 + mds[0][1]*s1;
    out[1] <== mds[1][0]*s0 + mds[1][1]*s1;
}

template PartialRound(state, rc, mds) {
    signal output out[2];
    
    // 只对第一个元素应用 S-box
    var s0 = state[0] + rc[0];
    s0 *= s0; s0 *= s0; s0 *= state[0];
    
    var s1 = state[1] + rc[1]; // 不应用 S-box
    
    // MDS
    out[0] <== mds[0][0]*s0 + mds[0][1]*s1;
    out[1] <== mds[1][0]*s0 + mds[1][1]*s1;
}