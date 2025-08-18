use sha2::{Digest, Sha256};

fn main() {
    let password = "123456";
    let controlled_salt = "salt";
    let challenge = "challenge";
    let controlling_random_salt = "ramdon_salt";
    let mut saved_share_hasher = Sha256::new();
    saved_share_hasher.update(password);
    saved_share_hasher.update(controlling_random_salt);
    let saved_controlled_salted_password_hash = saved_share_hasher.finalize()[..].to_vec();

    current(password, controlled_salt, challenge);
    order1(password, controlled_salt, challenge);
    order2(password, controlled_salt, challenge);
    controlling(
        controlled_salt,
        challenge,
        &saved_controlled_salted_password_hash,
    );
    controlled(
        password,
        controlled_salt,
        challenge,
        controlling_random_salt,
    );

    // Test if universal salt exists
    println!("\n=== Testing if universal salt exists ===");
    test_universal_salt();
}

fn current(password: &str, salt: &str, challenge: &str) {
    let mut hasher = Sha256::new();
    hasher.update(password);
    hasher.update(salt);
    let mut hasher2 = Sha256::new();
    hasher2.update(&hasher.finalize()[..]);
    hasher2.update(challenge);
    let hash2 = hasher2.finalize();
    println!("current: {:?}", hash2);
}

fn order1(password: &str, salt: &str, challenge: &str) {
    let mut hasher1 = Sha256::new();
    hasher1.update(password);
    hasher1.update(salt);
    hasher1.update(challenge);
    let hash1 = hasher1.finalize();
    println!("order1: {:?}", hash1);
}

fn order2(password: &str, salt: &str, challenge: &str) {
    let mut hasher = Sha256::new();
    hasher.update(salt);
    hasher.update(password);
    let mut hasher2 = Sha256::new();
    hasher2.update(&hasher.finalize()[..]);
    hasher2.update(challenge);
    let hash2 = hasher2.finalize();
    println!("order2: {:?}", hash2);
}
fn controlling(
    controlled_salt: &str,
    challenge: &str,
    saved_controlled_salted_password_hash: &[u8],
) {
    let mut hasher = Sha256::new();
    hasher.update(saved_controlled_salted_password_hash);
    hasher.update(controlled_salt);
    let mut hasher2 = Sha256::new();
    hasher2.update(&hasher.finalize()[..]);
    hasher2.update(challenge);
    let hash2 = hasher2.finalize();
    println!("controlling: {:?}", hash2);
}

fn controlled(
    password: &str,
    controlled_salt: &str,
    challenge: &str,
    controlling_random_salt: &str,
) {
    let mut saved_share_hasher = Sha256::new();
    saved_share_hasher.update(password);
    saved_share_hasher.update(controlling_random_salt);
    let saved_controlled_salted_password_hash = saved_share_hasher.finalize()[..].to_vec();
    let mut hasher = Sha256::new();
    hasher.update(saved_controlled_salted_password_hash);
    hasher.update(controlled_salt);
    let mut hasher2 = Sha256::new();
    hasher2.update(&hasher.finalize()[..]);
    hasher2.update(challenge);
    let hash2 = hasher2.finalize();
    println!("controlled: {:?}", hash2);
}

// Test if universal salt exists that makes H(any_password + salt) = constant
fn test_universal_salt() {
    let passwords = [
        "123456", "password", "admin", "test", "abc123", "qwerty", "letmein", "welcome",
    ];

    // Test some special salt values that might cause issues
    let test_salts: &[&[u8]] = &[
        b"",                                                                 // empty salt
        b"\x00",                                                             // null byte
        b"\x00\x00\x00\x00\x00\x00\x00\x00",                                 // multiple null bytes
        b"0",                                                                // character zero
        b"000000000000000000000000000000000000000000000000000000000000",     // all zero string
        b"\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f",                                 // high ASCII bytes
        b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", // repeated 'a'
        b"1234567890123456789012345678901234567890123456789012345678901234", // repeated digits
    ];

    for (i, salt) in test_salts.iter().enumerate() {
        println!("\n--- Testing salt {} ---", i + 1);
        println!("Salt: {:?} (length: {})", salt, salt.len());
        let mut hashes = Vec::new();

        for password in &passwords {
            let mut hasher = Sha256::new();
            hasher.update(password.as_bytes());
            hasher.update(salt);
            let hash = hasher.finalize();
            hashes.push(hash);
            println!("Password '{}' + salt => {:02x?}", password, &hash[..8]); // show first 8 bytes in hex
        }

        // Check if all hash values are the same
        let all_same = hashes.windows(2).all(|w| w[0] == w[1]);
        if all_same {
            println!("⚠️  ALERT: Universal salt found! All passwords produce the same hash!");
            println!("Dangerous salt value: {:?}", salt);
        } else {
            println!("✓ This salt is normal - different passwords produce different hashes");
        }
    }

    // Additional test: check hash distribution patterns
    println!("\n--- Testing hash collision probability ---");
    test_hash_distribution();

    // Test salt generation vulnerability
    println!("\n--- Testing malicious salt scenarios ---");
    test_malicious_salt_scenarios();
}

fn test_hash_distribution() {
    use std::collections::HashMap;
    let mut hash_counts = HashMap::new();

    // Test 1000 random salts with same password
    for i in 0..1000 {
        let salt = format!("salt_{}", i);
        let password = "testpassword";

        let mut hasher = Sha256::new();
        hasher.update(password.as_bytes());
        hasher.update(salt.as_bytes());
        let hash = hasher.finalize();

        // Look at first 4 bytes to detect collision probability
        let hash_prefix = &hash[..4];
        *hash_counts.entry(hash_prefix.to_vec()).or_insert(0) += 1;
    }

    let collisions = hash_counts.values().filter(|&&count| count > 1).count();
    println!(
        "Found {} collisions in first 4 bytes out of 1000 tests",
        collisions
    );
    println!("This is normal - 4 bytes only have 2^32 possible values");

    // Test if certain patterns emerge with similar salts
    println!("\nTesting sequential salt patterns:");
    let base_password = "test123";
    for i in 0..5 {
        let salt1 = format!("salt{}", i);
        let salt2 = format!("salt{}", i + 1);

        let mut hasher1 = Sha256::new();
        hasher1.update(base_password.as_bytes());
        hasher1.update(salt1.as_bytes());
        let hash1 = hasher1.finalize();

        let mut hasher2 = Sha256::new();
        hasher2.update(base_password.as_bytes());
        hasher2.update(salt2.as_bytes());
        let hash2 = hasher2.finalize();

        println!("Salt '{}' => {:02x?}", salt1, &hash1[..6]);
        println!("Salt '{}' => {:02x?}", salt2, &hash2[..6]);
        println!(
            "Hamming distance: {}",
            hamming_distance(&hash1[..], &hash2[..])
        );
        println!();
    }
}

fn test_malicious_salt_scenarios() {
    let password = "userpassword123";

    // Test what happens with extremely long salts
    let long_salt = "a".repeat(10000);
    let mut hasher = Sha256::new();
    hasher.update(password.as_bytes());
    hasher.update(long_salt.as_bytes());
    let hash_long = hasher.finalize();
    println!("Very long salt (10000 chars) => {:02x?}", &hash_long[..8]);

    // Test with salt containing special characters
    let special_salts = [
        "\n\r\t", // newlines and tabs
        "salt\x00embedded\x00nulls",
        "🔐🔑🚀",                        // unicode emojis
        "\\x41\\x42\\x43",               // escaped characters
        "$2a$10$N9qo8uLOickgx2ZMRZoMye", // bcrypt-like format
    ];

    for (i, salt) in special_salts.iter().enumerate() {
        let mut hasher = Sha256::new();
        hasher.update(password.as_bytes());
        hasher.update(salt.as_bytes());
        let hash = hasher.finalize();
        println!(
            "Special salt {} '{}' => {:02x?}",
            i + 1,
            salt.chars().take(20).collect::<String>(),
            &hash[..8]
        );
    }

    // Test potential timing attack vectors
    println!("\nTesting timing-related scenarios:");
    test_timing_scenarios(password);
}

fn test_timing_scenarios(password: &str) {
    use std::time::Instant;

    let salts = [
        "short",
        "medium_length_salt",
        "very_very_very_long_salt_that_goes_on_and_on",
    ];

    for salt in &salts {
        let start = Instant::now();
        for _ in 0..1000 {
            let mut hasher = Sha256::new();
            hasher.update(password.as_bytes());
            hasher.update(salt.as_bytes());
            let _ = hasher.finalize();
        }
        let duration = start.elapsed();
        println!("Salt length {}: {:?} for 1000 hashes", salt.len(), duration);
    }
}

fn hamming_distance(a: &[u8], b: &[u8]) -> usize {
    a.iter()
        .zip(b.iter())
        .map(|(x, y)| (x ^ y).count_ones() as usize)
        .sum()
}
