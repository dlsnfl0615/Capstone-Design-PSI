# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **Sender side** of a PSI (Private Set Intersection) protocol using BFV Fully Homomorphic Encryption (FHE). The Sender holds a dataset (sender.csv), receives encrypted query values from a Receiver, evaluates polynomials whose roots are the Sender's items on those encrypted values, and returns the encrypted results — without learning the Receiver's inputs.

## Build Commands

### Java (run from `sender/`)

```powershell
.\gradlew.bat build       # compile + test
.\gradlew.bat bootRun     # start Spring Boot on port 8081
.\gradlew.bat test        # run tests (sets --enable-native-access and PATH automatically)
.\gradlew.bat clean
```

### C++ Native Library (run from `sender/src/main/native/`)

```powershell
mkdir build; cd build
cmake ..
cmake --build . --config Release
```

Output: `sender/src/main/native/build/Release/sender-psi.dll`  
The DLL must exist before running Java tests or the application.

## Architecture

### Dual-Language Design

```
HTTP POST /api/files/upload
        │
SenderController (Java / Spring Boot)
        │
NativeService (Java Foreign Function & Memory API)
        │   SymbolLookup + Linker.downcallHandle()
        ▼
intersect() in sender-psi.dll (C++)
        │
  data_loader → hashing → evaluate → SEAL BFV ops → storage/result.bin
```

### Java Layer (`sender/src/main/java/com/psi/sender/`)

- **`SenderController`** — `POST /api/files/upload` receives binary files from the Receiver (params, public key, relin keys, encrypted powers) and triggers the native call.
- **`NativeService`** — Uses `java.lang.foreign.*` (Java 19+ FFI, not JNI) to load `sender-psi.dll` and invoke `intersect()`. The Gradle build injects `--enable-native-access=ALL-UNNAMED` and the DLL directory into PATH for both `test` and `bootRun` tasks.

### C++ Layer (`sender/src/main/native/src/`)

| File | Responsibility |
|------|---------------|
| `sender-main.cpp` | Exported `intersect()` entry point. Loads SEAL context/keys, orchestrates the full pipeline, writes `storage/result.bin`. |
| `data_loader` | Parses `storage/sender.csv`; packs rows as `(personal_id << 10) \| disease_bits` into `uint64_t`. |
| `hashing` | Simple hashing into m=8192 bins × B=74 slots using h=4 MurmurHash3 functions. |
| `evaluate` | Partitions bins into α=64 partitions; computes polynomial coefficients `P(x) = r × ∏(x − sᵢ)`; evaluates on encrypted powers via SEAL. |
| `mod` | Overflow-safe modular arithmetic (`safe_mul_mod`, `power_mod`) using bit-splitting — needed because MSVC lacks `__int128`. |

### Protocol Parameters

| Parameter | Value |
|-----------|-------|
| Poly degree n | 32768 |
| Plaintext modulus t | 44-bit prime |
| Bins m | 8192 |
| Slots per bin B | 74 |
| Partitions α | 64 |

### File I/O (relative to `sender/`)

| Path | Direction | Content |
|------|-----------|---------|
| `storage/parms.bin` | Input (from Receiver) | SEAL encryption parameters |
| `storage/public_key.bin` | Input | BFV public key |
| `storage/relin_key.bin` | Input | Relinearization keys |
| `storage/powers.bin` | Input | Encrypted powers of Receiver's value |
| `storage/sender.csv` | Input (local) | Sender's dataset |
| `storage/result.bin` | Output (to Receiver) | Encrypted polynomial evaluations |

## Tests

The single test in `NativeServiceTest.java` instantiates `NativeService` and calls `intersect()` directly. It requires the compiled DLL and valid files in `storage/`. Run with:

```powershell
.\gradlew.bat test
```

To run a single test class:

```powershell
.\gradlew.bat test --tests "com.psi.sender.service.NativeServiceTest"
```

## Dependencies

- **Microsoft SEAL** — BFV FHE library (must be installed/built separately; linked via CMake)
- **MurmurHash3** — bundled in `src/main/native/external/`
- **Spring Boot 4.0.6** — REST server
- **Java 25** — required for Foreign Function & Memory API (`java.lang.foreign.*`)