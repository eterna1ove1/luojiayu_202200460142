use neptune::poseidon::Poseidon;
use neptune::poseidon::PoseidonConstants;
use typenum::U2;
use blstrs::Scalar;
use serde_json::json;

fn main() {
    // 1. 初始化 Poseidon 参数
    let constants = PoseidonConstants::<Scalar, U2>::new();
    
    // 2. 准备输入数据
    let inputs = vec![
        Scalar::from(123456789u64),
        Scalar::from(987654321u64)
    ];
    
    // 3. 创建可变 Poseidon 实例
    let mut poseidon = Poseidon::<Scalar, U2>::new_with_preimage(&inputs, &constants);
    
    // 4. 计算哈希
    let hash = poseidon.hash();  // 现在可以调用需要 &mut self 的方法
    
    println!("Hash: {:?}", hash);
    
    // 5. 生成 input.json
    std::fs::write(
        "../inputs/input.json",
        serde_json::to_string_pretty(&json!({
            "x": [123456789, 987654321],
            "hash": hash.to_string()
        })).unwrap()
    ).unwrap();
}